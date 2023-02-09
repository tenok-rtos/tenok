#include <stdio.h>
#include <string.h>
#include "util.h"
#include "file.h"
#include "kernel.h"
#include "syscall.h"
#include "fifo.h"

#include "uart.h"

extern struct file *files;

struct inode inodes[INODE_CNT_MAX];
int inode_cnt = 0;

uint8_t data[1024];
int data_ptr = 0;

/*----------------------------------------------------------*
 |               file descriptors list layout               |
 *---------*----------------*--------------*----------------*
 |  usage  | path server fd |   task fds   |    file fds    |
 |---------*----------------*--------------*----------------*
 | numbers |       1        | TASK_NUM_MAX | FILE_CNT_LIMIT |
 *---------*----------------*--------------*----------------*/

char path_list[FILE_CNT_LIMIT][PATH_LEN_MAX];
int  path_cnt = 0;

bool fs_search_entry_in_dir(struct inode *inode, char *entry_name)
{
	/* the function take a diectory inode and return the inode of the entry to find */

	struct dir_info *dir = (struct dir_info *)inode->data;

	while(dir != NULL) {
		if(strcmp(dir->entry_name, entry_name) == 0) {
			inode = &inodes[dir->entry_inode];
			return true;
		}

		dir = dir->next;
	}

	return false;
}

struct inode *fs_add_new_dir(struct inode *inode_dir, char *dir_name)
{
	/* allocate new memory for the directory  */
	inode_dir->data = &data[data_ptr];
	data_ptr += sizeof(struct dir_info);

	struct dir_info *dir = (struct dir_info *)inode_dir->data;
	dir->entry_inode = inode_cnt;      //assign new inode number for the directory
	strcpy(dir->entry_name, dir_name); //copy the directory name

	/* configure the new directory inode */
	struct inode *new_inode = &inodes[dir->entry_inode];
	new_inode->is_dir = true;
	new_inode->data_size = sizeof(struct dir_info);
	new_inode->data = inode_dir->data;
	strcpy(inodes->name, dir_name);

	inode_cnt++;

	return &inodes[dir->entry_inode];
}

struct inode *fs_add_new_file(struct inode *inode_dir, char *file_name, int file_size)
{
	/* allocate new memory for the file */
	uint8_t *file_data_p = &data[data_ptr];
	data_ptr += sizeof(uint8_t) * file_size;

	struct dir_info *dir = (struct dir_info*)file_data_p;
	dir->entry_inode = inode_cnt;       //assign new inode number for the file
	strcpy(dir->entry_name, file_name); //copy the file name

	/* configure the new file inode */
	struct inode *new_inode = &inodes[dir->entry_inode];
	new_inode->is_dir = false;
	new_inode->data_size = file_size;
	new_inode->data = file_data_p;
	strcpy(inodes->name, file_name);

	inode_cnt++;

	return &inodes[dir->entry_inode];
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

int create_file(char *pathname)
{
	/* a legal path name must start with '/' */
	if(pathname[0] != '/') {
		return -1;
	}

	struct inode *inode_curr = &inodes[0]; //traverse from the root inode

	char entry_curr[PATH_LEN_MAX];
	char *_path = pathname;

	_path = split_path(entry_curr, _path); //get rid of the first '/'

	while(1) {
		/* split the path and get the entry hierarchically */
		_path = split_path(entry_curr, _path);

		//char s[100] = {0};
		//sprintf(s, "%s\n\r", entry_curr);
		//uart3_puts(s);

		if(_path != NULL) {
			/* the path can be further splitted, which means it is a directory */

			/* two successive '/' are detected */
			if(entry_curr == '\0') {
				continue;
			}

			/* check if the directory exists */
			if(fs_search_entry_in_dir(inode_curr, entry_curr) == false) {
				/* directory does not exists, create one */
				struct inode *new_inode = fs_add_new_dir(inode_curr, entry_curr);

				if(new_inode != NULL) {
					inode_curr = new_inode;
				} else {
					return -1; //failed to create the directory
				}
			}
		} else {
			/* no more path string to be splitted */

			/* check if the file already exists */
			if(fs_search_entry_in_dir(inode_curr, entry_curr) == true) {
				return -1;
			}

			/* if the last character of the pathname is not equal to '/', it is a file */
			int len = strlen(pathname);
			if(pathname[len - 1] != '/') {
				if(fs_add_new_file(inode_curr, entry_curr, 1 /* XXX */) == NULL) {
					return -1; //failed to create the file
				}
			}

			break;
		}
	}

	return 0;
}

void request_path_register(int reply_fd, char *path)
{
	int file_cmd = PATH_CMD_REGISTER_PATH;
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

	fifo_write(&files[PATH_SERVER_FD], buf, buf_size, 0);
}

void request_file_open(int reply_fd, char *path)
{
	int file_cmd = PATH_CMD_OPEN;
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

	fifo_write(&files[PATH_SERVER_FD], buf, buf_size, 0);
}

void file_system(void)
{
	set_program_name("file system");
	setpriority(PRIO_PROCESS, getpid(), 0);

	//mknod("/sys/path_server", 0, S_IFIFO);

	int  path_len;
	char path[PATH_LEN_MAX];

	int  dev;

	/* initialize the root node */
	fs_add_new_dir(&inodes[0], "");

	while(1) {
		int file_cmd;
		read(PATH_SERVER_FD, &file_cmd, sizeof(file_cmd));

		int reply_fd;
		read(PATH_SERVER_FD, &reply_fd, sizeof(reply_fd));

		switch(file_cmd) {
		case PATH_CMD_REGISTER_PATH:
			read(PATH_SERVER_FD, &path_len, sizeof(path_len));
			read(PATH_SERVER_FD, path, path_len);

			int new_fd;

			if(path_cnt < FILE_CNT_LIMIT) {
				memcpy(path_list[path_cnt], path, path_len);

				create_file(path);

				new_fd = path_cnt + TASK_NUM_MAX + 1;
				path_cnt++;
			} else {
				new_fd = -1;
			}

			write(reply_fd, &new_fd, sizeof(new_fd));

			break;
		case PATH_CMD_OPEN:
			read(PATH_SERVER_FD, &path_len, sizeof(path_len));
			read(PATH_SERVER_FD, path, path_len);

			int open_fd;

			/* invalid empty path */
			if(path[0] == '\0') {
				open_fd = -1;
			}

			/* path finding */
			int i;
			for(i = 0; i < path_cnt; i++) {
				if(strcmp(path_list[i], path) == 0) {
					open_fd = i + TASK_NUM_MAX + 1;
					break;
				}
			}

			/* path not found */
			if(i >= path_cnt) {
				open_fd = -1;
			}

			write(reply_fd, &open_fd, sizeof(open_fd));

			break;
		}
	}
}
