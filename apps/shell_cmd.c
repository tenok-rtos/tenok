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
			struct dentry *dentry = list_entry(list_curr, struct dentry, d_list);

			if(dentry->d_inode == inode_last) {
				sprintf(path, "%s%s/", path, dentry->d_name);
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

void shell_cmd_ls(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char str[200] = {0};

	/* no file is under this directory */
	if(inode_curr->i_size == 0)
		return;

	if(inode_curr->i_rdev != RDEV_ROOTFS) {
		if(inode_curr->i_sync == false)
			fs_mount_directory(inode_curr, inode_curr); //FIXME
	}

	fs_print_mount_directory(str, inode_curr);

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
		if(inode_curr->i_size == 0) {
			sprintf(str, "cd: %s: No such file or directory\n\r", param_list[1]);
			shell_puts(str);
			return;
		}

		/* compare all the file names under the current directory */
		struct list *list_curr;
		list_for_each(list_curr, &inode_curr->i_dentry) {
			struct dentry *dentry = list_entry(list_curr, struct dentry, d_list);

			if(strcmp(dentry->d_name, param_list[1]) == 0) {
				struct inode *inode = &inodes[dentry->d_inode];
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

		/* can not find the file */
		sprintf(str, "cd: %s: No such file or directory\n\r", param_list[1]);
		shell_puts(str);
		return;
	} else {
		sprintf(str, "cd: too many arguments\n\r");
		shell_puts(str);
		return;
	}
}

void shell_cmd_pwd(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char str[200] = {0};
	char path[PATH_LEN_MAX] = {'/'};

	shell_get_pwd(path);

	sprintf(str, "%s\n\r", path);
	shell_puts(str);
}

void shell_cmd_cat(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char path[PATH_LEN_MAX] = {0};

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

	int i;
	for(i = 0; i < stat.st_size; i++) {
		read(fd, &c, 1);

		if(c == EOF) {
			str[i] = '\0';
			break;
		} else {
			str[i] = c;
		}
	}

	shell_puts(str);
}
