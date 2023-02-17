#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "util.h"
#include "file.h"
#include "kernel.h"
#include "syscall.h"
#include "fifo.h"
#include "rom_dev.h"
#include "reg_file.h"
#include "uart.h"

int create_file(char *pathname, uint8_t file_type);

extern struct file *files[TASK_NUM_MAX+FILE_CNT_LIMIT+1];
extern struct memory_pool mem_pool;

struct super_block rootfs_super;
struct inode inodes[INODE_CNT_MAX];
uint8_t rootfs_blk[ROOTFS_BLK_CNT][ROOTFS_BLK_SIZE];

int mount_cnt = 0;
struct mount mount_points[MOUNT_CNT_MAX];

int file_cnt = 0;

int register_chrdev(char *name, struct file_operations *fops)
{
	char dev_path[100] = {0};
	sprintf(dev_path, "/dev/%s", name);

	/* create new file in the file system */
	int fd = create_file(dev_path, S_IFCHR);

	/* allocate and initialize the new device file */
	struct file *dev_file = memory_pool_alloc(&mem_pool, sizeof(struct file));
	dev_file->f_op = fops;
	list_init(&dev_file->task_wait_list);

	/* register the new device file to the file table */
	files[fd] = dev_file;
}

int register_blkdev(char *name, struct file_operations *fops)
{
	char dev_path[100] = {0};
	sprintf(dev_path, "/dev/%s", name);

	/* create new file in the file system */
	int fd = create_file(dev_path, S_IFBLK);

	/* allocate and initialize the new device file */
	struct file *dev_file = memory_pool_alloc(&mem_pool, sizeof(struct file));
	dev_file->f_op = fops;
	list_init(&dev_file->task_wait_list);

	/* register the new device file to the file table */
	files[fd] = dev_file;
}

void file_system_init(void)
{
	/* configure the root directory inode */
	struct inode *inode_root = &inodes[0];
	inode_root->i_mode   = S_IFDIR;
	inode_root->i_ino    = 0;
	inode_root->i_size   = 0;
	inode_root->i_blocks = 0;
	inode_root->i_data   = NULL;
	list_init(&inode_root->i_dentry);

	rootfs_super.inode_cnt = 1;
	rootfs_super.blk_cnt = 0;
}

struct inode *fs_search_file(struct inode *inode, char *file_name)
{
	/* the function take a diectory inode and return the inode of the entry to find */

	/* currently the dentry table is empty */
	if(list_is_empty(&inode->i_dentry) == true)
		return NULL;

	/* compare the file name in the dentry list */
	struct list *list_curr;
	list_for_each(list_curr, &inode->i_dentry) {
		struct dentry *dentry = list_entry(list_curr, struct dentry, list);

		if(strcmp(dentry->file_name, file_name) == 0)
			return &inodes[dentry->file_inode];
	}

	return NULL;
}

int calculate_dentry_block_size(size_t block_size, size_t dentry_cnt)
{
	/* calculate how many dentries can a block hold */
	int dentry_per_blk = block_size / sizeof(struct dentry);

	/* calculate how many blocks is required for storing N dentries */
	int blocks = dentry_cnt / dentry_per_blk;
	if(dentry_cnt % dentry_per_blk)
		blocks++;

	return blocks;
}

int fs_write(struct inode *inode, uint8_t *write_addr, uint8_t *data, size_t size)
{
	if(inode->i_rdev == RDEV_ROOTFS) {
		uint8_t *start_addr = (uint8_t *)rootfs_blk;
		uint8_t *end_addr   = (uint8_t *)rootfs_blk + (ROOTFS_BLK_CNT * ROOTFS_BLK_SIZE);

		if((write_addr > end_addr) || (write_addr < start_addr))
			return EFAULT;

		memcpy(write_addr, data, sizeof(uint8_t) * size);

		return size;
	} else {
		/* get storage device file */
		struct file *dev_file = mount_points[inode->i_rdev].dev_file;

		/* get write function pointer */
		ssize_t (*_write)(struct file *filp, const char *buf, size_t size, loff_t offset);
		_write = dev_file->f_op->write;

		/* write memory */
		return _write(dev_file, write_addr, size, 0);
	}
}

int fs_read(struct inode *inode, uint8_t *read_addr, uint8_t *data, size_t size)
{
	if(inode->i_rdev == RDEV_ROOTFS) {
		uint8_t *start_addr = (uint8_t *)rootfs_blk;
		uint8_t *end_addr   = (uint8_t *)rootfs_blk + (ROOTFS_BLK_CNT * ROOTFS_BLK_SIZE);

		if((read_addr > end_addr) || (read_addr < start_addr))
			return EFAULT;

		memcpy(data, read_addr, sizeof(uint8_t) * size);

		return size;
	} else {
		/* get storage device file */
		struct file *dev_file = mount_points[inode->i_rdev].dev_file;

		/* get read function pointer */
		ssize_t (*_read)(struct file *filp, char *buf, size_t size, loff_t offset);
		_read = dev_file->f_op->read;

		/* read memory */
		return _read(dev_file, read_addr, size, 0);
	}
}

struct inode *fs_add_file(struct inode *inode_dir, char *file_name, int file_type)
{
	/* inodes numbers is full */
	if(rootfs_super.inode_cnt >= INODE_CNT_MAX)
		return NULL;

	/* calculate how many dentries can a block hold */
	int dentry_per_blk = ROOTFS_BLK_SIZE / sizeof(struct dentry);

	/* calculate how many dentries the directory has */
	int dentry_cnt = inode_dir->i_size / sizeof(struct dentry);

	/* check if current block can fit a new dentry */
	bool fit = ((dentry_cnt + 1) <= (inode_dir->i_blocks * dentry_per_blk)) &&
	           (inode_dir->i_data != NULL);

	/* allocate memory for the new dentry */
	uint8_t *dir_data_p;
	if(fit == true) {
		struct list *list_end = inode_dir->i_dentry.last;
		struct dentry *dir = list_entry(list_end, struct dentry, list);
		dir_data_p = (uint8_t *)dir + sizeof(struct dentry);
	} else {
		/* can not fit, requires a new block */
		dir_data_p = (uint8_t *)&rootfs_blk[rootfs_super.blk_cnt];
		rootfs_super.blk_cnt += 1;
	}

	/* configure the new dentry */
	struct dentry *new_dentry = (struct dentry*)dir_data_p;
	new_dentry->file_inode    = rootfs_super.inode_cnt; //assign new inode number for the file
	new_dentry->parent_inode  = inode_dir->i_ino;       //save the inode of the parent directory
	strcpy(new_dentry->file_name, file_name);           //copy the file name

	/* file table is full */
	if(file_cnt >= FILE_CNT_LIMIT)
		return NULL;

	/* dispatch a new file descriptor number */
	int fd = file_cnt + TASK_NUM_MAX + 1;

	/* configure the new file inode */
	struct inode *new_inode = &inodes[rootfs_super.inode_cnt];
	new_inode->i_ino    = rootfs_super.inode_cnt;
	new_inode->i_parent = inode_dir->i_ino;
	new_inode->i_fd     = fd;
	list_init(&new_inode->i_dentry);

	/* file instantiation */
	int result = 0;

	switch(file_type) {
	case S_IFIFO:
		/* create new named pipe */
		result = fifo_init(fd, (struct file **)&files, &mem_pool);

		new_inode->i_mode   = S_IFIFO;
		new_inode->i_size   = 0;
		new_inode->i_blocks = 0;
		new_inode->i_data   = NULL;

		files[fd]->file_inode = new_inode;

		break;
	case S_IFCHR:
		new_inode->i_mode   = S_IFCHR;
		new_inode->i_size   = 0;
		new_inode->i_blocks = 0;
		new_inode->i_data   = NULL;

		files[fd]->file_inode = NULL;

		break;
	case S_IFBLK:
		new_inode->i_mode   = S_IFBLK;
		new_inode->i_size   = 0;
		new_inode->i_blocks = 0;
		new_inode->i_data   = NULL;

		files[fd]->file_inode = NULL;

		break;
	case S_IFREG:
		/* create new regular file */
		result = reg_file_init(fd, new_inode, (struct file **)&files, NULL /* TODO */, &mem_pool);

		/* allocate memory for the new file */
		uint8_t *file_data_p = (uint8_t *)&rootfs_blk[rootfs_super.blk_cnt];
		rootfs_super.blk_cnt += 1;

		new_inode->i_mode   = S_IFREG;
		new_inode->i_size   = 1;
		new_inode->i_blocks = 1;
		new_inode->i_data   = file_data_p;

		files[fd]->file_inode = new_inode;

		break;
	case S_IFDIR:
		new_inode->i_mode   = S_IFDIR;
		new_inode->i_size   = 0;
		new_inode->i_blocks = 0;
		new_inode->i_data   = NULL; //new directory without any files

		break;
	default:
		result = -1;
	}

	if(result != 0)
		return NULL;

	file_cnt++;               //update file descriptor number for the next file
	rootfs_super.inode_cnt++; //update inode number for the next file

	/* currently no files is under the directory */
	if(list_is_empty(&inode_dir->i_dentry) == true)
		inode_dir->i_data = (uint8_t *)new_dentry; //add the first dentry

	/* insert the new file under the current directory */
	list_push(&inode_dir->i_dentry, &new_dentry->list);

	/* update size and block information of the inode */
	inode_dir->i_size += sizeof(struct dentry);

	dentry_cnt = inode_dir->i_size / sizeof(struct dentry);
	inode_dir->i_blocks = calculate_dentry_block_size(ROOTFS_BLK_SIZE, dentry_cnt);

	return new_inode;
}

char *split_path(char *entry, char *path)
{
	/*
	 * get the first entry of a given path.
	 * e.g., input = "some_dir/test_file", output = "some_dir"
	 */

	while(1) {
		bool found_dir = (*path == '/');

		/* copy */
		if(found_dir == false) {
			*entry = *path;
			entry++;
		}

		path++;

		if((found_dir == true) || (*path == '\0'))
			break;
	}

	*entry = '\0';

	if(*path == '\0')
		return NULL; //the path can not be splitted anymore

	return path; //return the address of the left path string
}

int create_file(char *pathname, uint8_t file_type)
{
	/* a legal path name must start with '/' */
	if(pathname[0] != '/')
		return -1;

	struct inode *inode_curr = &inodes[0]; //start from the root node
	struct inode *inode;

	char entry_curr[PATH_LEN_MAX];
	char *_path = pathname;

	_path = split_path(entry_curr, _path); //get rid of the first '/'

	while(1) {
		/* split the path and get the entry hierarchically */
		_path = split_path(entry_curr, _path);

		if(_path != NULL) {
			/* the path can be further splitted, which means it is a directory */

			/* two successive '/' are detected */
			if(entry_curr[0] == '\0')
				continue;

			/* search the entry and get the inode */
			inode = fs_search_file(inode_curr, entry_curr);

			/* check if the directory exists */
			if(inode == NULL) {
				/* directory does not exist, create one */
				inode = fs_add_file(inode_curr, entry_curr, S_IFDIR);

				if(inode == NULL)
					return -1;

				inode_curr = inode;
			} else {
				/* directory exists */
				inode_curr = inode;
			}
		} else {
			/* no more path string to be splitted */

			/* check if the file already exists */
			if(fs_search_file(inode_curr, entry_curr) != NULL)
				return -1;

			/* if the last character of the pathname is not equal to '/', it is a file */
			int len = strlen(pathname);
			if(pathname[len - 1] != '/') {
				/* create new inode for the file */
				inode = fs_add_file(inode_curr, entry_curr, file_type);

				/* check if the inode is created successfully */
				if(inode == NULL)
					return -1; //failed to create the file

				return inode->i_fd;
			}
		}
	}
}

int open_file(char *pathname)
{
	/* input: file path, output: file descriptor number */

	/* a legal path name must start with '/' */
	if(pathname[0] != '/')
		return -1;

	struct inode *inode_curr = &inodes[0]; //start from the root node
	struct inode *inode;

	char entry_curr[PATH_LEN_MAX];
	char *_path = pathname;

	_path = split_path(entry_curr, _path); //get rid of the first '/'

	while(1) {
		/* split the path and get the entry hierarchically */
		_path = split_path(entry_curr, _path);

		if(_path != NULL) {
			/* the path can be further splitted, which means it is a directory */

			/* two successive '/' are detected */
			if(entry_curr[0] == '\0')
				continue;

			/* search the entry and get the inode */
			inode = fs_search_file(inode_curr, entry_curr);

			if(inode == NULL)
				return -1; //directory does not exist

			inode_curr = inode;
		} else {
			/* check if the file exists */
			inode = fs_search_file(inode_curr, entry_curr);

			if(inode == NULL)
				return -1;

			if(inode->i_mode == S_IFDIR)
				return -1; //not a file, no file descriptor number to be return

			return inode->i_fd; //return the file descriptor number
		}
	}
}

void request_create_file(int reply_fd, char *path, uint8_t file_type)
{
	int file_cmd = FS_CREATE_FILE;
	int path_len = strlen(path) + 1;

	const int overhead = sizeof(file_cmd) + sizeof(reply_fd) + sizeof(path_len) + sizeof(file_type);
	char buf[PATH_LEN_MAX + overhead];

	int buf_size = 0;

	memcpy(&buf[buf_size], &file_cmd, sizeof(file_cmd));
	buf_size += sizeof(file_cmd);

	memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
	buf_size += sizeof(reply_fd);

	memcpy(&buf[buf_size], &path_len, sizeof(path_len));
	buf_size += sizeof(path_len);

	memcpy(&buf[buf_size], path, path_len);
	buf_size += path_len;

	memcpy(&buf[buf_size], &file_type, sizeof(file_type));
	buf_size += sizeof(file_type);

	fifo_write(files[PATH_SERVER_FD], buf, buf_size, 0);
}

void request_open_file(int reply_fd, char *path)
{
	int file_cmd = FS_OPEN_FILE;
	int path_len = strlen(path) + 1;

	const int overhead = sizeof(file_cmd) + sizeof(reply_fd) + sizeof(path_len);
	char buf[PATH_LEN_MAX + overhead];

	int buf_size = 0;

	memcpy(&buf[buf_size], &file_cmd, sizeof(file_cmd));
	buf_size += sizeof(file_cmd);

	memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
	buf_size += sizeof(reply_fd);

	memcpy(&buf[buf_size], &path_len, sizeof(path_len));
	buf_size += sizeof(path_len);

	memcpy(&buf[buf_size], path, path_len);
	buf_size += path_len;

	fifo_write(files[PATH_SERVER_FD], buf, buf_size, 0);
}

void file_system(void)
{
	set_program_name("file system");
	setpriority(PRIO_PROCESS, getpid(), 0);

	int  path_len;
	char path[PATH_LEN_MAX];

	int  dev;

	while(1) {
		int file_cmd;
		read(PATH_SERVER_FD, &file_cmd, sizeof(file_cmd));

		int reply_fd;
		read(PATH_SERVER_FD, &reply_fd, sizeof(reply_fd));

		switch(file_cmd) {
		case FS_CREATE_FILE:
			read(PATH_SERVER_FD, &path_len, sizeof(path_len));
			read(PATH_SERVER_FD, path, path_len);

			uint8_t file_type;
			read(PATH_SERVER_FD, &file_type, sizeof(file_type));

			int new_fd = create_file(path, file_type);
			write(reply_fd, &new_fd, sizeof(new_fd));

			break;
		case FS_OPEN_FILE:
			read(PATH_SERVER_FD, &path_len, sizeof(path_len));
			read(PATH_SERVER_FD, path, path_len);

			int open_fd = open_file(path);
			write(reply_fd, &open_fd, sizeof(open_fd));

			break;
		}
	}
}
