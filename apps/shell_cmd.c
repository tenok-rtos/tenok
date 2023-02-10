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

void shell_cmd_help(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char *s = "supported commands:\n\r"
	          "help\n\r"
	          "clear\n\r"
	          "history\n\r"
	          "ps\n\r"
	          "echo\n\r"
	          "ls\n\r"
	          "cd\n\r";
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
	struct dir_info *dir = (struct dir_info *)inode_curr->data;

	char str[200] = {0};

	/* traverse all files under the directory */
	while(dir != NULL) {
		/* get the file inode */
		int file_inode_num = dir->entry_inode;
		struct inode *file_inode = &inodes[file_inode_num];

		if(file_inode->is_dir == true) {
			sprintf(str, "%s%s/ ", str, dir->entry_name);
		} else {
			sprintf(str, "%s%s ", str, dir->entry_name);
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
	} else {
		/* retrieve the directory table */
		struct dir_info *dir = (struct dir_info *)inode_curr->data;

		while(dir != NULL) {
			if(strcmp(dir->entry_name, param_list[1]) == 0) {
				struct inode *_inode = &inodes[dir->entry_inode];
				if(_inode->is_dir == false) {
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
	}
}
