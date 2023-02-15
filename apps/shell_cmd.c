#include <stdio.h>
#include <string.h>
#include "shell.h"
#include "kernel.h"
#include "file.h"

extern struct shell_struct shell;
extern struct inode inodes[INODE_CNT_MAX];

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
	struct dir_info *dir = (struct dir_info *)inode->i_data;

	while(1) {
		if(inode->i_ino == 0) {
			break;
		}

		/* switch to the parent directory */
		int parent_dir_inode = dir->parent_inode;
		inode = &inodes[parent_dir_inode];
		dir = (struct dir_info *)inode->i_data;

		sprintf(path, "%s%s/", path, dir->file_name);
	}
}

void shell_cmd_help(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char *s = "supported commands:\n\r"
	          "help, clear, history, ps, echo, ls, cd, pwd\n\r";
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
	/* retrieve the directory table */
	struct dir_info *dir = (struct dir_info *)inode_curr->i_data;

	char str[200] = {0};

	/* traverse all files under the directory */
	while(dir != NULL) {
		/* get the file inode */
		int file_inode_num = dir->file_inode;
		struct inode *file_inode = &inodes[file_inode_num];

		if(file_inode->i_mode == S_IFDIR) {
			sprintf(str, "%s%s/  ", str, dir->file_name);
		} else {
			sprintf(str, "%s%s  ", str, dir->file_name);
		}

		/* point to the next file */
		dir = dir->next;
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
		/* retrieve the directory table */
		struct dir_info *dir = (struct dir_info *)inode_curr->i_data;

		/* handle cd .. */
		if(strcmp("..", param_list[1]) == 0) {
			inode_curr = &inodes[dir->parent_inode];
			return;
		}

		/* compare all the file names under the current directory */
		while(dir != NULL) {
			if(strcmp(dir->file_name, param_list[1]) == 0) {
				struct inode *_inode = &inodes[dir->file_inode];
				if(_inode->i_mode != S_IFDIR) {
					sprintf(str, "cd: %s: Not a directory\n\r", param_list[1]);
					shell_puts(str);
					return;
				} else {
					inode_curr = _inode;
					return;
				}
			}

			/* point to the next file */
			dir = dir->next;
		}

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
	char path[200] = {'/'};

	shell_get_pwd(path);

	sprintf(str, "%s\n\r", path);
	shell_puts(str);
}
