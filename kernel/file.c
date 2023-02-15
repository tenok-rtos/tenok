#include <stdio.h>
#include <string.h>
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

	rootfs_super.inode_cnt = 1;
	rootfs_super.blk_cnt = 0;
}

struct inode *fs_search_entry_in_dir(struct inode *inode, char *file_name)
{
	/* the function take a diectory inode and return the inode of the entry to find */

	struct dentry *dir = (struct dentry *)inode->i_data;

	while(dir != NULL) {
		if(strcmp(dir->file_name, file_name) == 0) {
			return &inodes[dir->file_inode];
		}

		dir = dir->next;
	}

	return NULL;
}

struct inode *fs_add_new_dir(struct inode *inode_dir, char *dir_name)
{
	if(rootfs_super.inode_cnt >= INODE_CNT_MAX) {
		return NULL;
	}

	uint8_t *dir_data_p; //for memory allocation of new dentry

	/* calculate how many dentries can a block hold */
	int dentry_per_blk = ROOTFS_BLK_SIZE / sizeof(struct dentry);

	/* calculate how many dentries the directory has */
	int dentry_cnt = inode_dir->i_size / sizeof(struct dentry);

	/* check if current block can fit a new dentry */
	if((dentry_cnt + 1) <= (inode_dir->i_blocks * dentry_per_blk)) {
		/* TODO: replace with double linked list */

		/* find the end of the list */
		struct dentry *dir = (struct dentry *)inode_dir->i_data;
		while(dir->next != NULL) {
			dir = dir->next;
		}
		dir_data_p = (uint8_t *)dir + sizeof(struct dentry);
	} else {
		/* can not fit, requires a new block */
		dir_data_p = (uint8_t *)&rootfs_blk[rootfs_super.blk_cnt];
		rootfs_super.blk_cnt += 1;
	}

	/* configure the directory file table */
	struct dentry *new_dir = (struct dentry *)dir_data_p;
	new_dir->file_inode   = rootfs_super.inode_cnt; //assign new inode number for the directory
	new_dir->parent_inode = inode_dir->i_ino;       //save the inode of the parent directory
	new_dir->next         = NULL;
	strcpy(new_dir->file_name, dir_name);           //copy the directory name

	/* configure the new directory inode */
	struct inode *new_inode = &inodes[rootfs_super.inode_cnt];
	new_inode->i_mode   = S_IFDIR;
	new_inode->i_ino    = rootfs_super.inode_cnt;
	new_inode->i_parent = inode_dir->i_ino;
	new_inode->i_size   = 0;
	new_inode->i_blocks = 0;
	new_inode->i_data   = NULL; //new directory without any files

	rootfs_super.inode_cnt++;

	/* insert the new directory under the current directory */
	int dir_file_cnt = 1;
	struct dentry *curr_dir = (struct dentry *)inode_dir->i_data;

	if(curr_dir == NULL) {
		/* currently no files is under the directory */
		inode_dir->i_data = (uint8_t *)new_dir; //add new file info
	} else {
		/* insert at the end of the table */
		while(curr_dir->next != NULL) {
			curr_dir = curr_dir->next;
			dir_file_cnt++;
		}
		curr_dir->next = new_dir;
	}

	/* update the directory inode */
	inode_dir->i_size += sizeof(struct dentry);
	inode_dir->i_blocks = dir_file_cnt / dentry_per_blk;

	return new_inode;
}

struct inode *fs_add_new_file(struct inode *inode_dir, char *file_name, int file_type, int *new_fd)
{
	if(rootfs_super.inode_cnt >= INODE_CNT_MAX) {
		return NULL;
	}

	int file_size = sizeof(uint8_t); //reserve one byte for the file descriptor number

	uint8_t *dir_data_p; //for memory allocation of new dentry

	/* calculate how many dentries can a block hold */
	int dentry_per_blk = ROOTFS_BLK_SIZE / sizeof(struct dentry);

	/* calculate how many dentries the directory has */
	int dentry_cnt = inode_dir->i_size / sizeof(struct dentry);

	/* check if current block can fit a new dentry */
	if((dentry_cnt + 1) <= (inode_dir->i_blocks * dentry_per_blk)) {
		/* TODO: replace with double linked list */

		/* find the end of the list */
		struct dentry *dir = (struct dentry *)inode_dir->i_data;
		while(dir->next != NULL) {
			dir = dir->next;
		}
		dir_data_p = (uint8_t *)dir + sizeof(struct dentry);
	} else {
		/* can not fit, requires a new block */
		dir_data_p = (uint8_t *)&rootfs_blk[rootfs_super.blk_cnt];
		rootfs_super.blk_cnt += 1;
	}

	/* allocate memory for the new file */
	uint8_t *file_data_p = (uint8_t *)&rootfs_blk[rootfs_super.blk_cnt];
	rootfs_super.blk_cnt += 1;

	/* configure the directory file table to describe the new file */
	struct dentry *file_info = (struct dentry*)dir_data_p;
	file_info->file_inode = rootfs_super.inode_cnt; //assign new inode number for the file
	file_info->parent_inode = inode_dir->i_ino;     //save the inode of the parent directory
	file_info->next = NULL;
	strcpy(file_info->file_name, file_name);        //copy the file name

	/* configure the new file inode */
	struct inode *new_inode = &inodes[rootfs_super.inode_cnt];
	new_inode->i_mode     = file_type;
	new_inode->i_ino      = rootfs_super.inode_cnt;
	new_inode->i_parent   = inode_dir->i_parent;
	new_inode->i_size     = file_size;
	new_inode->i_blocks   = 1;
	new_inode->i_data     = file_data_p;

	/* dispatch a new file descriptor number */
	if(file_cnt < FILE_CNT_LIMIT) {
		*new_fd = file_cnt + TASK_NUM_MAX + 1;
	} else {
		return NULL; //file descriptor table is full
	}

	/* create new file according to its type */
	int result = 0;

	switch(file_type) {
	case S_IFIFO:
		result = fifo_init(*new_fd, (struct file **)&files, &mem_pool);
		files[*new_fd]->file_inode = new_inode;
		break;
	case S_IFCHR:
		//details are handled by the register_chrdev()
		files[*new_fd]->file_inode = NULL;
		break;
	case S_IFBLK:
		//details are handled by the register_blkdev()
		files[*new_fd]->file_inode = NULL;
		break;
	case S_IFREG:
		result = reg_file_init(*new_fd, new_inode, (struct file **)&files, NULL /* TODO */, &mem_pool);
		files[*new_fd]->file_inode = new_inode;
		break;
	default:
		result = -1;
	}

	if(result == 0) {
		file_cnt++;               //update file descriptor number for the next file
		rootfs_super.inode_cnt++; //update inode number for the next file
	} else {
		return NULL;
	}

	/* file descriptor number is stored in the first byte of the file */
	*(new_inode->i_data) = *new_fd;

	/* insert the new file under the current directory */
	int dir_file_cnt = 1;
	struct dentry *curr_dir = (struct dentry *)inode_dir->i_data;

	if(curr_dir == NULL) {
		/* currently no files is under the directory */
		inode_dir->i_data = (uint8_t *)file_info; //add new file info
	} else {
		/* insert at the end of the table */
		while(curr_dir->next != NULL) {
			curr_dir = curr_dir->next;
			dir_file_cnt++;
		}
		curr_dir->next = file_info;
	}

	/* update the directory inode */
	inode_dir->i_size += sizeof(struct dentry);
	inode_dir->i_blocks = dir_file_cnt / dentry_per_blk;
	if(dir_file_cnt % dentry_per_blk) {
		inode_dir->i_blocks++;
	}

	return new_inode;
}

char *split_path(char *entry, char *path)
{
	/*
	 * get the first entry of a given path.
	 * e.g., input = "some_dir/test_file", output = "some_dir"
	 */

	while(1) {
		bool end = (*path == '/') || (*path == '\0');

		/* copy */
		if(end == false) {
			*entry = *path;
			entry++;
		}

		path++;

		if(end == true) {
			break;
		}
	}

	*entry = '\0';

	if(*path == '\0') {
		return NULL; //the path can not be splitted anymore
	} else {
		return path; //return the address of the left path string
	}
}

int create_file(char *pathname, uint8_t file_type)
{
	/* a legal path name must start with '/' */
	if(pathname[0] != '/') {
		return -1;
	}

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
			if(entry_curr[0] == '\0') {
				continue;
			}

			/* search the entry and get the inode */
			inode = fs_search_entry_in_dir(inode_curr, entry_curr);

			/* check if the directory exists */
			if(inode == NULL) {
				/* directory does not exist, create one */
				inode = fs_add_new_dir(inode_curr, entry_curr);

				/* check if the directory exists */
				if(inode != NULL) {
					inode_curr = inode;
				} else {
					return -1; //failed to create the directory
				}
			} else {
				/* directory exists */
				inode_curr = inode;
			}
		} else {
			/* no more path string to be splitted */

			/* check if the file already exists */
			if(fs_search_entry_in_dir(inode_curr, entry_curr) != NULL) {
				return -1;
			}

			/* if the last character of the pathname is not equal to '/', it is a file */
			int len = strlen(pathname);
			if(pathname[len - 1] != '/') {
				/* create new inode for the file */
				int new_fd;
				inode = fs_add_new_file(inode_curr, entry_curr, file_type, &new_fd);

				/* check if the inode is created successfully */
				if(inode == NULL) {
					return -1; //failed to create the file
				} else {
					return new_fd;
				}
			}
		}
	}
}

int open_file(char *pathname)
{
	/* input: file path, output: file descriptor number */

	/* a legal path name must start with '/' */
	if(pathname[0] != '/') {
		return -1;
	}

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
			if(entry_curr[0] == '\0') {
				continue;
			}

			/* search the entry and get the inode */
			inode = fs_search_entry_in_dir(inode_curr, entry_curr);

			if(inode == NULL) {
				return -1; //directory does not exist
			} else {
				inode_curr = inode;
			}
		} else {
			/* check if the file exists */
			inode = fs_search_entry_in_dir(inode_curr, entry_curr);

			if(inode == NULL) {
				return -1;
			}

			if(inode->i_mode == S_IFDIR) {
				/* not a file, no file descriptor number to be return */
				return -1;
			} else {
				/* return the file descriptor number */
				char file_fd = *((char *)inode->i_data);
				return file_fd;
			}
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
