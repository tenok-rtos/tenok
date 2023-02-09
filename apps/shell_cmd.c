#include <stdio.h>
#include "shell.h"
#include "kernel.h"
#include "file.h"

extern struct shell_struct shell;
extern struct inode inodes[INODE_CNT_MAX];

struct inode *inode_curr = NULL;

void shell_cmd_help(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char *s = "supported commands:\n\r"
	          "help\n\r"
	          "clear\n\r"
	          "history\n\r"
	          "ps\n\r"
	          "echo\n\r"
	          "ls\n\r";
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
	if(inode_curr == NULL) {
		inode_curr = &inodes[0];
	}

	struct dir_info *dir = (struct dir_info *)inode_curr->data;

	char str[100];

	while(dir != NULL) {
		/* root directory inode */
		int file_inode_num = dir->entry_inode;
		struct inode *file_inode = &inodes[file_inode_num];

		sprintf(str, "%s%s ", str, file_inode->name);

		dir = dir->next;
	}

	sprintf(str, "%s\n\r", str);
	shell_puts(str);
}
