#include <stdio.h>
#include <string.h>
#include "shell.h"
#include "kernel.h"
#include "fs.h"
#include "syscall.h"

extern struct shell_struct shell;
extern struct inode inodes[INODE_CNT_MAX];
extern struct mount mount_points[MOUNT_CNT_MAX + 1];

struct inode *inode_curr = NULL;

void shell_path_init(void)
{
	inode_curr = &inodes[0];
}

void shell_get_pwd(char *path)
{
	path[0] = '/';
	path[1] = '\0';

	struct inode *inode = inode_curr;

	while(1) {
		if(inode->i_ino == 0)
			return;

		/* switch to the parent directory */
		uint32_t inode_last = inode->i_ino;
		inode = &inodes[inode->i_parent];

		/* no file is under this directory */
		if(list_is_empty(&inode->i_dentry) == true)
			return;

		struct list *list_curr;
		list_for_each(list_curr, &inode->i_dentry) {
			struct dentry *dir = list_entry(list_curr, struct dentry, d_list);

			if(dir->d_inode == inode_last) {
				sprintf(path, "%s%s/", path, dir->d_name);
				break;
			}
		}
	}
}

void shell_cmd_help(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char *s = "supported commands:\n\r"
	          "help, clear, history, ps, echo, ls, cd, pwd, cat\n\r";
	shell_puts(s);
}

void shell_cmd_clear(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	shell_cls();
}

void shell_cmd_history(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	shell_print_history(&shell);
}

void shell_cmd_ps(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char str[200];
	sprint_tasks(str);
	shell_puts(str);
}

void shell_cmd_echo(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char str[200] = {0};

	int i;
	for(i = 1; i < param_cnt - 1; i++) {
		sprintf(str, "%s%s ", str, param_list[i]);
	}
	sprintf(str, "%s%s\n\r", str, param_list[param_cnt-1]);

	shell_puts(str);
}

void print_mount_directory(char *str, struct inode *inode_dir)
{
	if(inode_dir->i_sync == false) {
		//TODO: handled with the file server
		fs_mount_directory(inode_dir, inode_dir);
	}

	const uint32_t sb_size     = sizeof(struct super_block);
	const uint32_t inode_size  = sizeof(struct inode);
	const uint32_t dentry_size = sizeof(struct dentry);

	loff_t inode_addr;
	struct inode inode;

	loff_t dentry_addr;
	struct dentry dentry;

	/* load storage device file */
	struct file *dev_file = mount_points[inode_dir->i_rdev].dev_file;
	ssize_t (*dev_read)(struct file *filp, char *buf, size_t size, loff_t offset) = dev_file->f_op->read;

	/* get the list head of the dentry list */
	struct list i_dentry_list = inode_dir->i_dentry;
	dentry.d_list = i_dentry_list;

	while(1) {
		/* if the address points to the inodes region, then the iteration is back to the list head */
		if((uint32_t)dentry.d_list.next < (sb_size + inode_size * INODE_CNT_MAX))
			return; //no more dentry to read

		/* calculate the address of the next dentry to read */
		dentry_addr = (loff_t)list_entry(dentry.d_list.next, struct dentry, d_list);

		/* load the dentry from the storage device */
		dev_read(dev_file, (uint8_t *)&dentry, dentry_size, dentry_addr);

		/* load the file inode from the storage */
		inode_addr = sb_size + (inode_size * dentry.d_inode);
		dev_read(dev_file, (uint8_t *)&inode, inode_size, inode_addr);

		if(inode.i_mode == S_IFDIR) {
			sprintf(str, "%s%s/  ", str, dentry.d_name);
		} else {
			sprintf(str, "%s%s  ", str, dentry.d_name);
		}
	}
}

void print_rootfs_directory(char *str, struct inode *inode_dir)
{
	struct list *list_curr;
	list_for_each(list_curr, &inode_curr->i_dentry) {
		struct dentry *dir = list_entry(list_curr, struct dentry, d_list);

		/* get the file inode */
		int file_inode_num = dir->d_inode;
		struct inode *file_inode = &inodes[file_inode_num];

		if(file_inode->i_mode == S_IFDIR) {
			sprintf(str, "%s%s/  ", str, dir->d_name);
		} else {
			sprintf(str, "%s%s  ", str, dir->d_name);
		}
	}
}

void shell_cmd_ls(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char str[200] = {0};

	/* no file is under this directory */
	if(inode_curr->i_size == 0)
		return;

	if(inode_curr->i_rdev != RDEV_ROOTFS) {
		print_mount_directory(str, inode_curr);
	} else {
		print_rootfs_directory(str, inode_curr);
	}

	sprintf(str, "%s\n\r", str);
	shell_puts(str);
}

void shell_cmd_cd(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char str[200] = {0};

	if(param_cnt == 1) {
		inode_curr = &inodes[0];
	} else if(param_cnt == 2) {
		/* handle cd .. */
		if(strcmp("..", param_list[1]) == 0) {
			inode_curr = &inodes[inodes->i_parent];
			return;
		}

		/* no file is under this directory */
		if(list_is_empty(&inode_curr->i_dentry) == true) {
			sprintf(str, "cd: %s: No such file or directory\n\r", param_list[1]);
			shell_puts(str);
			return;
		}

		/* compare all the file names under the current directory */
		struct list *list_curr;
		list_for_each(list_curr, &inode_curr->i_dentry) {
			struct dentry *dir = list_entry(list_curr, struct dentry, d_list);

			if(strcmp(dir->d_name, param_list[1]) == 0) {
				struct inode *inode = &inodes[dir->d_inode];
				if(inode->i_mode != S_IFDIR) {
					sprintf(str, "cd: %s: Not a directory\n\r", param_list[1]);
					shell_puts(str);
					return;
				} else {
					inode_curr = inode;
					return;
				}
			}
		}
	} else {
		sprintf(str, "cd: too many arguments\n\r");
		shell_puts(str);
		return;
	}
}

void shell_cmd_pwd(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char str[200] = {0};
	char path[200] = {'/'};

	shell_get_pwd(path);

	sprintf(str, "%s\n\r", path);
	shell_puts(str);
}

void shell_cmd_cat(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char path[200] = {0};

	if(param_list[1][0] != '/') {
		/* input is a relative path */
		char pwd[200] = {0};
		shell_get_pwd(pwd);

		sprintf(path, "%s%s", pwd, param_list[1]);
	} else {
		/* input is a absolute path */
		strcpy(path, param_list[1]);
	}

	/* open the file */
	int fd = open(path, 0, 0);
	if(fd == -1) {
		char str[200] = {0};
		sprintf(str, "cat: %s: No such file or directory\n\r", path);
		shell_puts(str);
		return;
	}

	/* check if the file mode is set as regular file */
	struct stat stat;
	fstat(fd, &stat);
	if(stat.st_mode != S_IFREG) {
		char str[200] = {0};
		sprintf(str, "cat: %s: Invalid argument\n\r", path);
		shell_puts(str);
		return;
	}

	/* reset the start position of the file */
	lseek(fd, 0, SEEK_SET);

	/* read file content until EOF is detected */
	signed char c;
	char str[200] = {0};
	int i = 0;

	while(1) {
		read(fd, &c, 1);

		if(c == EOF) {
			str[i] = '\0';
			break;
		} else {
			str[i] = c;
			i++;
		}
	}

	shell_puts(str);
}
