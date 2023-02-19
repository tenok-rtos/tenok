#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "fs.h"
#include "kernel.h"
#include "syscall.h"
#include "fifo.h"
#include "rom_dev.h"
#include "reg_file.h"
#include "uart.h"

static int create_file(char *pathname, uint8_t file_type);
static struct inode *fs_mount_file(struct inode *inode_dir, struct inode *mnt_inode, struct dentry *mnt_dentry, bool sync_now);

ssize_t rootfs_read(struct file *filp, char *buf, size_t size, loff_t offset);
ssize_t rootfs_write(struct file *filp, const char *buf, size_t size, loff_t offset);

extern struct file *files[TASK_NUM_MAX+FILE_CNT_LIMIT+1];
extern struct memory_pool mem_pool;
extern int file_cnt;

struct inode inodes[INODE_CNT_MAX];
uint8_t rootfs_blk[ROOTFS_BLK_CNT][ROOTFS_BLK_SIZE];

struct mount mount_points[MOUNT_CNT_MAX + 1]; //0 is reserved for the rootfs
int mount_cnt = 0;

static struct file_operations rootfs_file_ops = {
	.read = rootfs_read,
	.write = rootfs_write
};

int register_chrdev(char *name, struct file_operations *fops)
{
	char devpath[100] = {0};
	sprintf(devpath, "/dev/%s", name);

	/* create new file in the file system */
	int fd = create_file(devpath, S_IFCHR);

	/* allocate and initialize the new device file */
	struct file *dev_file = memory_pool_alloc(&mem_pool, sizeof(struct file));
	dev_file->f_op = fops;
	list_init(&dev_file->task_wait_list);

	/* register the new device file to the file table */
	files[fd] = dev_file;
}

int register_blkdev(char *name, struct file_operations *fops)
{
	char devpath[100] = {0};
	sprintf(devpath, "/dev/%s", name);

	/* create new file in the file system */
	int fd = create_file(devpath, S_IFBLK);

	/* allocate and initialize the new device file */
	struct file *dev_file = memory_pool_alloc(&mem_pool, sizeof(struct file));
	dev_file->f_op = fops;
	list_init(&dev_file->task_wait_list);

	/* register the new device file to the file table */
	files[fd] = dev_file;
}

void rootfs_init(void)
{
	/* configure the root directory inode */
	struct inode *inode_root = &inodes[0];
	inode_root->i_mode   = S_IFDIR;
	inode_root->i_ino    = 0;
	inode_root->i_size   = 0;
	inode_root->i_blocks = 0;
	inode_root->i_data   = NULL;
	list_init(&inode_root->i_dentry);

	/* create new file for the rootfs */
	struct file *rootfs_file = memory_pool_alloc(&mem_pool, sizeof(struct file));
	rootfs_file->f_op = &rootfs_file_ops;

	int fd = file_cnt + TASK_NUM_MAX + 1;
	files[fd] = rootfs_file;
	file_cnt++;

	/* mount the rootfs */
	struct super_block *rootfs_super_blk = &mount_points[RDEV_ROOTFS].super_blk;
	rootfs_super_blk->s_inode_cnt = 1;
	rootfs_super_blk->s_blk_cnt   = 0;
	rootfs_super_blk->s_sb_addr   = (uint32_t)rootfs_super_blk;
	rootfs_super_blk->s_ino_addr  = (uint32_t)inodes;
	rootfs_super_blk->s_blk_addr  = (uint32_t)rootfs_blk;
	mount_points[RDEV_ROOTFS].dev_file = rootfs_file;
	mount_cnt = 1;
}

ssize_t rootfs_read(struct file *filp, char *buf, size_t size, loff_t offset)
{
	uint8_t *addr = (uint8_t *)offset; //offset is the address to read the data
	memcpy(buf, addr, size);
}

ssize_t rootfs_write(struct file *filp, const char *buf, size_t size, loff_t offset)
{
	uint8_t *addr = (uint8_t *)offset; //offset is the address to write the data
	memcpy(addr, buf, size);
}

static void fs_read_list(struct file *dev_file, uint32_t list_addr, struct list *list)
{
	/* read the list */
	dev_file->f_op->read(dev_file, (uint8_t *)list, sizeof(struct list), list_addr);
}

static void fs_read_dentry(struct file *dev_file, uint32_t dentry_addr, struct dentry *dentry)
{
	/* read the dentry */
	dev_file->f_op->read(dev_file, (uint8_t *)dentry, sizeof(struct dentry), dentry_addr);
}

static void fs_read_inode(uint8_t rdev, struct file *dev_file, uint32_t inode_num, struct inode *inode)
{
	/* calculate the inode address */
	struct super_block *super_blk = &mount_points[rdev].super_blk;
	uint32_t inode_addr = super_blk->s_ino_addr + (sizeof(struct inode) * inode_num);

	/* read the inode */
	dev_file->f_op->read(dev_file, (uint8_t *)inode, sizeof(struct inode), inode_addr);
}

//input : directory inode, file name
//output: file inode
struct inode *fs_search_file(struct inode *inode_dir, char *file_name)
{
	/* currently the dentry table is empty */
	if(inode_dir->i_size == 0)
		return NULL;

	/* load storage device file */
	struct file *dev_file = mount_points[inode_dir->i_rdev].dev_file;

	/* read the first "dentry.d_list" stored at "inode.i_data" in the storage */
	struct list list;
	struct dentry *dentry_first = (struct dentry *)inode_dir->i_data;
	uint32_t list_head_addr     = (uint32_t)&dentry_first->d_list;
	fs_read_list(dev_file, list_head_addr, &list);

	/* get the list head of the dentries */
	struct list *d_list_head = list.last; //&inode.i_dentry = dentry.d_list.last;

	/* initialize the dentry pointer for iteration */
	struct dentry dentry = {.d_list = inode_dir->i_dentry};

	struct list *d_list_curr;
	while(1) {
		if((d_list_curr = dentry.d_list.next) == d_list_head)
			return NULL; //iteration is complete

		/* calculate the dentry address to read */
		uint32_t dentry_addr = (loff_t)list_entry(d_list_curr, struct dentry, d_list);

		/* load the dentry from the storage device */
		fs_read_dentry(dev_file, dentry_addr, &dentry);

		/* compare the file name */
		if(strcmp(dentry.d_name, file_name) == 0) {
			if(inode_dir->i_rdev == RDEV_ROOTFS) {
				return &inodes[dentry.d_inode];
			} else {
				inode_dir->i_sync = true;

				/* read the file inode from the storage device */
				struct inode inode;
				fs_read_inode(inode_dir->i_rdev, dev_file, dentry.d_inode, &inode);

				/* overwrite the device number */
				inode.i_rdev = inode_dir->i_rdev;

				return fs_mount_file(inode_dir, &inode, &dentry, true);
			}
		}
	}
}

static int calculate_dentry_blocks(size_t block_size, size_t dentry_cnt)
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
		/* address translation */
		write_addr += (uint32_t)rootfs_blk;

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
		return _write(dev_file, data, size, (size_t)write_addr);
	}
}

int fs_read(struct inode *inode, uint8_t *read_addr, uint8_t *data, size_t size)
{
	if(inode->i_rdev == RDEV_ROOTFS) {
		/* address translation */
		read_addr += (uint32_t)rootfs_blk;

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
		return _read(dev_file, data, size, (size_t)read_addr);
	}
}

static struct dentry *fs_allocate_dentry(struct inode *inode_dir)
{
	/* calculate how many dentries can a block hold */
	int dentry_per_blk = ROOTFS_BLK_SIZE / sizeof(struct dentry);

	/* calculate how many dentries the directory has */
	int dentry_cnt = inode_dir->i_size / sizeof(struct dentry);

	/* check if current block can fit a new dentry */
	bool fit = ((dentry_cnt + 1) <= (inode_dir->i_blocks * dentry_per_blk)) &&
	           (inode_dir->i_size != 0) /* no memory is allocated if size = 0 */;

	/* allocate memory for the new dentry */
	uint8_t *dir_data_p;
	if(fit == true) {
		struct list *list_end = inode_dir->i_dentry.last;
		struct dentry *dir = list_entry(list_end, struct dentry, d_list);
		dir_data_p = (uint8_t *)dir + sizeof(struct dentry);
	} else {
		/* can not fit, requires a new block */
		dir_data_p = (uint8_t *)&rootfs_blk[mount_points[RDEV_ROOTFS].super_blk.s_blk_cnt];
		mount_points[RDEV_ROOTFS].super_blk.s_blk_cnt++;
	}

	return (struct dentry *)dir_data_p;
}

static struct inode *fs_add_file(struct inode *inode_dir, char *file_name, int file_type)
{
	/* inodes numbers is full */
	if(mount_points[0].super_blk.s_inode_cnt >= INODE_CNT_MAX)
		return NULL;

	/* configure the new dentry */
	struct dentry *new_dentry = fs_allocate_dentry(inode_dir);
	new_dentry->d_inode    = mount_points[RDEV_ROOTFS].super_blk.s_inode_cnt; //assign new inode number for the file
	new_dentry->d_parent  = inode_dir->i_ino; //save the inode of the parent directory
	strcpy(new_dentry->d_name, file_name);    //copy the file name

	/* file table is full */
	if(file_cnt >= FILE_CNT_LIMIT)
		return NULL;

	/* dispatch a new file descriptor number */
	int fd = file_cnt + TASK_NUM_MAX + 1;

	/* configure the new file inode */
	struct inode *new_inode = &inodes[mount_points[RDEV_ROOTFS].super_blk.s_inode_cnt];
	new_inode->i_rdev   = RDEV_ROOTFS;
	new_inode->i_ino    = mount_points[RDEV_ROOTFS].super_blk.s_inode_cnt;
	new_inode->i_parent = inode_dir->i_ino;
	new_inode->i_fd     = fd;

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

		break;
	case S_IFCHR:
		new_inode->i_mode   = S_IFCHR;
		new_inode->i_size   = 0;
		new_inode->i_blocks = 0;
		new_inode->i_data   = NULL;

		break;
	case S_IFBLK:
		new_inode->i_mode   = S_IFBLK;
		new_inode->i_size   = 0;
		new_inode->i_blocks = 0;
		new_inode->i_data   = NULL;

		break;
	case S_IFREG:
		/* create new regular file */
		result = reg_file_init(new_inode, (struct file **)&files, &mem_pool);

		/* allocate memory for the new file */
		uint8_t *file_data_p = (uint8_t *)(mount_points[inode_dir->i_rdev].super_blk.s_blk_cnt * ROOTFS_BLK_SIZE);
		mount_points[inode_dir->i_rdev].super_blk.s_blk_cnt++;

		new_inode->i_mode   = S_IFREG;
		new_inode->i_size   = 0;
		new_inode->i_blocks = 1;
		new_inode->i_data   = file_data_p;

		break;
	case S_IFDIR:
		new_inode->i_mode   = S_IFDIR;
		new_inode->i_size   = 0;
		new_inode->i_blocks = 0;
		new_inode->i_data   = NULL; //new directory without any files
		list_init(&new_inode->i_dentry);

		break;
	default:
		result = -1;
	}

	if(result != 0)
		return NULL;

	file_cnt++; //update file descriptor number for the next file
	mount_points[RDEV_ROOTFS].super_blk.s_inode_cnt++; //update inode number for the next file

	/* currently no files is under the directory */
	if(list_is_empty(&inode_dir->i_dentry) == true)
		inode_dir->i_data = (uint8_t *)new_dentry; //add the first dentry

	/* insert the new file under the current directory */
	list_push(&inode_dir->i_dentry, &new_dentry->d_list);

	/* update size and block information of the inode */
	inode_dir->i_size += sizeof(struct dentry);

	int dentry_cnt = inode_dir->i_size / sizeof(struct dentry);
	inode_dir->i_blocks = calculate_dentry_blocks(ROOTFS_BLK_SIZE, dentry_cnt);

	return new_inode;
}

//if sync_now is set as false, the file system will only update the dentry.d_list of the parent directory
static struct inode *fs_mount_file(struct inode *inode_dir, struct inode *mnt_inode, struct dentry *mnt_dentry, bool sync_now)
{
	/* inodes numbers is full */
	if(mount_points[0].super_blk.s_inode_cnt >= INODE_CNT_MAX)
		return NULL;

	/* configure the new dentry */
	struct dentry *new_dentry = fs_allocate_dentry(inode_dir);
	new_dentry->d_inode  = mount_points[RDEV_ROOTFS].super_blk.s_inode_cnt; //assign new inode number for the file
	new_dentry->d_parent = inode_dir->i_ino;        //save the inode of the parent directory
	strcpy(new_dentry->d_name, mnt_dentry->d_name); //copy the file name

	struct inode *new_inode = NULL;

	if(sync_now == true) {
		/* configure the new file inode */
		new_inode = &inodes[mount_points[RDEV_ROOTFS].super_blk.s_inode_cnt];
		new_inode->i_mode   = mnt_inode->i_mode;
		new_inode->i_rdev   = mnt_inode->i_rdev;
		new_inode->i_ino    = mount_points[RDEV_ROOTFS].super_blk.s_inode_cnt;
		new_inode->i_parent = inode_dir->i_ino;
		new_inode->i_size   = mnt_inode->i_size;
		new_inode->i_blocks = mnt_inode->i_blocks;
		new_inode->i_data   = mnt_inode->i_data;
		new_inode->i_sync   = true;
		new_inode->i_dentry = mnt_inode->i_dentry;

		/* update inode number for the next file */
		mount_points[RDEV_ROOTFS].super_blk.s_inode_cnt++;

		/* dispatch a fd number for the regular file */
		if(mnt_inode->i_mode == S_IFREG) {
			/* dispatch a new file descriptor number */
			if(file_cnt >= FILE_CNT_LIMIT) {
				return NULL; //file table is full
			} else {
				new_inode->i_fd = file_cnt + TASK_NUM_MAX + 1;
				file_cnt++;
			}

			/* create new regular file */
			int result = reg_file_init(new_inode, (struct file **)&files, &mem_pool);

			if(result != 0)
				return NULL;
		}
	}

	/* currently no files is under the directory */
	if(list_is_empty(&inode_dir->i_dentry) == true)
		inode_dir->i_data = (uint8_t *)new_dentry; //add the first dentry

	/* insert the new file under the current directory */
	list_push(&inode_dir->i_dentry, &new_dentry->d_list);

	/* update size and block information of the inode */
	inode_dir->i_size += sizeof(struct dentry);

	int dentry_cnt = inode_dir->i_size / sizeof(struct dentry);
	inode_dir->i_blocks = calculate_dentry_blocks(ROOTFS_BLK_SIZE, dentry_cnt);

	return new_inode;
}

void fs_mount_directory(struct inode *inode_src, struct inode *inode_target)
{
	if(inode_target->i_sync == true)
		return;

	const uint32_t sb_size     = sizeof(struct super_block);
	const uint32_t inode_size  = sizeof(struct inode);
	const uint32_t dentry_size = sizeof(struct dentry);

	loff_t inode_addr;
	struct inode inode;

	loff_t dentry_addr;
	struct dentry dentry;

	/* load storage device file */
	struct file *dev_file = mount_points[inode_src->i_rdev].dev_file;
	ssize_t (*dev_read)(struct file *filp, char *buf, size_t size, loff_t offset) = dev_file->f_op->read;

	/* get the list head of the dentry.d_list */
	struct list i_dentry_list = inode_src->i_dentry;
	dentry.d_list = i_dentry_list;

	while(1) {
		/* if the address points to the inodes region, then the iteration is back to the list head */
		if((uint32_t)dentry.d_list.next < (sb_size + inode_size * INODE_CNT_MAX)) {
			inode_src->i_sync = true;
			return; //no more dentry to read
		}

		/* calculate the address of the next dentry to read */
		dentry_addr = (loff_t)list_entry(dentry.d_list.next, struct dentry, d_list);

		/* load the dentry from the storage device */
		dev_read(dev_file, (uint8_t *)&dentry, dentry_size, dentry_addr);

		/* load the file inode from the storage device */
		inode_addr = sb_size + (inode_size * dentry.d_inode);
		dev_read(dev_file, (uint8_t *)&inode, inode_size, inode_addr);

		/* overwrite the device number */
		inode.i_rdev = inode_src->i_rdev;

		/* files will not be synchronized with the file system before using */
		bool sync_now = inode.i_mode == S_IFDIR ? true : false;

		/* mount the file */
		fs_mount_file(inode_target, &inode, &dentry, sync_now);
	}
}

//get the first entry of a given path. e.g., given a input"dir1/file1.txt" yields with "dir".
//input : path
//output: entry
static char *splitpath(char *entry, char *path)
{
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

static int create_file(char *pathname, uint8_t file_type)
{
	/* a legal path name must start with '/' */
	if(pathname[0] != '/')
		return -1;

	struct inode *inode_curr = &inodes[0]; //start from the root node
	struct inode *inode;

	char entry_curr[PATH_LEN_MAX];
	char *path = pathname;

	path = splitpath(entry_curr, path); //get rid of the first '/'

	while(1) {
		/* split the path and get the entry hierarchically */
		path = splitpath(entry_curr, path);

		if(path != NULL) {
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

static int open_file(char *pathname)
{
	/* input: file path, output: file descriptor number */

	/* a legal path name must start with '/' */
	if(pathname[0] != '/')
		return -1;

	struct inode *inode_curr = &inodes[0]; //start from the root node
	struct inode *inode;

	char entry_curr[PATH_LEN_MAX] = {0};
	char *path = pathname;

	path = splitpath(entry_curr, path); //get rid of the first '/'

	while(1) {
		/* split the path and get the entry hierarchically */
		path = splitpath(entry_curr, path);

		if(path != NULL) {
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

struct inode *open_directory(char *pathname)
{
	/* input: file path, output: directory inode */

	/* a legal path name must start with '/' */
	if(pathname[0] != '/')
		return NULL;

	struct inode *inode_curr = &inodes[0]; //start from the root node
	struct inode *inode;

	char entry_curr[PATH_LEN_MAX] = {0};
	char *path = pathname;

	path = splitpath(entry_curr, path); //get rid of the first '/'
	if(path == NULL) {
		return inode_curr;
	}

	while(1) {
		/* split the path and get the entry hierarchically */
		path = splitpath(entry_curr, path);

		/* two successive '/' are detected */
		if(entry_curr[0] == '\0')
			continue;

		/* search the entry and get the inode */
		inode = fs_search_file(inode_curr, entry_curr);

		/* directory does not exist */
		if(inode == NULL)
			return NULL;

		/* not a directory */
		if(inode->i_mode != S_IFDIR)
			return NULL;

		inode_curr = inode;

		/* no more sub-string to be splitted */
		if(path == NULL)
			break;
	}

	return inode_curr;
}

static int _mount(char *source, char *target)
{
	/* get the file of the storage to be mounted */
	int source_fd = open_file(source);
	if(source_fd < 0)
		return -1;

	/* get the rootfs inode to mount the storage */
	struct inode *target_inode = open_directory(target);
	if(target_inode == NULL)
		return -1;

	/* get storage device file */
	struct file *dev_file = files[source_fd];
	mount_points[mount_cnt].dev_file = dev_file;

	/* get read function pointer */
	ssize_t (*dev_read)(struct file *filp, char *buf, size_t size, loff_t offset) = dev_file->f_op->read;

	/* calculate the start address of the super block, inode table, and block region */
	const uint32_t sb_size     = sizeof(struct super_block);
	const uint32_t inode_size  = sizeof(struct inode);
	const uint32_t dentry_size = sizeof(struct dentry);
	loff_t super_blk_addr = 0;
	loff_t inodes_addr = super_blk_addr + sb_size;
	loff_t block_addr = inodes_addr + (inode_size * INODE_CNT_MAX);

	/* read the super block from the device */
	dev_read(dev_file, (uint8_t *)&mount_points[mount_cnt].super_blk, sb_size, super_blk_addr);

	/* read the root inode of the storage */
	struct inode inode_root;
	dev_read(dev_file, (uint8_t *)&inode_root, inode_size, inodes_addr);

	/* overwrite the device number */
	inode_root.i_rdev = mount_cnt;

	/* mount the root directory */
	fs_mount_directory(&inode_root, target_inode);

	mount_cnt++;

	return 0;
}

void request_create_file(int reply_fd, char *path, uint8_t file_type)
{
	int fs_cmd = FS_CREATE_FILE;
	int path_len = strlen(path) + 1;

	const int overhead = sizeof(fs_cmd) + sizeof(reply_fd) + sizeof(path_len) + sizeof(file_type);
	char buf[PATH_LEN_MAX + overhead];

	int buf_size = 0;

	memcpy(&buf[buf_size], &fs_cmd, sizeof(fs_cmd));
	buf_size += sizeof(fs_cmd);

	memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
	buf_size += sizeof(reply_fd);

	memcpy(&buf[buf_size], &path_len, sizeof(path_len));
	buf_size += sizeof(path_len);

	memcpy(&buf[buf_size], path, path_len);
	buf_size += path_len;

	memcpy(&buf[buf_size], &file_type, sizeof(file_type));
	buf_size += sizeof(file_type);

	fifo_write(files[FILE_SYSTEM_FD], buf, buf_size, 0);
}

void request_open_file(int reply_fd, char *path)
{
	int fs_cmd = FS_OPEN_FILE;
	int path_len = strlen(path) + 1;

	const int overhead = sizeof(fs_cmd) + sizeof(reply_fd) + sizeof(path_len);
	char buf[PATH_LEN_MAX + overhead];

	int buf_size = 0;

	memcpy(&buf[buf_size], &fs_cmd, sizeof(fs_cmd));
	buf_size += sizeof(fs_cmd);

	memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
	buf_size += sizeof(reply_fd);

	memcpy(&buf[buf_size], &path_len, sizeof(path_len));
	buf_size += sizeof(path_len);

	memcpy(&buf[buf_size], path, path_len);
	buf_size += path_len;

	fifo_write(files[FILE_SYSTEM_FD], buf, buf_size, 0);
}

void request_mount(int reply_fd, char *source, char *target)
{
	int fs_cmd = FS_MOUNT;
	int source_len = strlen(source) + 1;
	int target_len = strlen(target) + 1;

	const int overhead = sizeof(fs_cmd) + sizeof(reply_fd) +
	                     sizeof(source_len) + sizeof(target_len);

	char buf[(PATH_LEN_MAX*2) + overhead];

	int buf_size = 0;

	memcpy(&buf[buf_size], &fs_cmd, sizeof(fs_cmd));
	buf_size += sizeof(fs_cmd);

	memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
	buf_size += sizeof(reply_fd);

	memcpy(&buf[buf_size], &source_len, sizeof(source_len));
	buf_size += sizeof(source_len);

	memcpy(&buf[buf_size], source, source_len);
	buf_size += source_len;

	memcpy(&buf[buf_size], &target_len, sizeof(target_len));
	buf_size += sizeof(target_len);

	memcpy(&buf[buf_size], target, target_len);
	buf_size += target_len;

	fifo_write(files[FILE_SYSTEM_FD], buf, buf_size, 0);
}

void file_system(void)
{
	set_program_name("file system");
	setpriority(PRIO_PROCESS, getpid(), 1);

	while(1) {
		int file_cmd;
		read(FILE_SYSTEM_FD, &file_cmd, sizeof(file_cmd));

		int reply_fd;
		read(FILE_SYSTEM_FD, &reply_fd, sizeof(reply_fd));

		switch(file_cmd) {
		case FS_CREATE_FILE: {
			int  path_len;
			read(FILE_SYSTEM_FD, &path_len, sizeof(path_len));

			char path[PATH_LEN_MAX];
			read(FILE_SYSTEM_FD, path, path_len);

			uint8_t file_type;
			read(FILE_SYSTEM_FD, &file_type, sizeof(file_type));

			int new_fd = create_file(path, file_type);
			write(reply_fd, &new_fd, sizeof(new_fd));

			break;
		}
		case FS_OPEN_FILE: {
			int  path_len;
			read(FILE_SYSTEM_FD, &path_len, sizeof(path_len));

			char path[PATH_LEN_MAX];
			read(FILE_SYSTEM_FD, path, path_len);

			int open_fd = open_file(path);
			write(reply_fd, &open_fd, sizeof(open_fd));

			break;
		}
		case FS_MOUNT: {
			int source_len;
			read(FILE_SYSTEM_FD, &source_len, sizeof(source_len));

			char source[PATH_LEN_MAX];
			read(FILE_SYSTEM_FD, source, source_len);

			int target_len;
			read(FILE_SYSTEM_FD, &target_len, sizeof(target_len));

			char target[PATH_LEN_MAX];
			read(FILE_SYSTEM_FD, &target, target_len);

			int result = _mount(source, target);
			write(reply_fd, &result, sizeof(result));

			break;
		}
		}
	}
}
