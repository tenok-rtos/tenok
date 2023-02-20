#include <stdio.h>
#include <string.h>
#include "shell.h"
#include "kernel.h"
#include "fs.h"
#include "syscall.h"

extern struct shell_struct shell;
extern struct inode inodes[INODE_CNT_MAX];
extern struct mount mount_points[MOUNT_CNT_MAX + 1];

struct inode *shell_dir_curr = NULL;

void shell_path_init(void)
{
	DIR dir;
	opendir("/", &dir);
	shell_dir_curr = dir.inode_dir;
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
	if(shell_dir_curr->i_size == 0)
		return;

	/* get current directoory path */
	char path[PATH_LEN_MAX] = {0};
	fs_get_pwd(path, shell_dir_curr);

	/* open the directory */
	DIR dir;
	opendir(path, &dir);

	/* enumerate the directory */
	struct dirent dirent;
	while((readdir(&dir, &dirent)) != -1) {
		if(dirent.d_type == S_IFDIR) {
			sprintf(str, "%s%s/  ", str, dirent.d_name);
		} else {
			sprintf(str, "%s%s  ", str, dirent.d_name);
		}
	}

	sprintf(str, "%s\n\r", str);

	shell_puts(str);
}

void shell_cmd_cd(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char str[200] = {0};

	DIR dir;

	if(param_cnt == 1) {
		opendir("/", &dir);
		shell_dir_curr = dir.inode_dir;
	} else if(param_cnt == 2) {
		/* handle cd .. */
		if(strcmp("..", param_list[1]) == 0) {
			shell_dir_curr = &inodes[inodes->i_parent];
			return;
		}

		char path[PATH_LEN_MAX] = {0};

		if(param_list[1][0] == '/') {
			/* handle the absolute path */
			strncpy(path, param_list[1], PATH_LEN_MAX);
		} else {
			/* handle the relative path */
			fs_get_pwd(path, shell_dir_curr);
			sprintf(path, "%s%s", path, param_list[1]);
		}

		/* open the directory */
		opendir(path, &dir);

		/* directory not found */
		if(dir.inode_dir == NULL) {
			sprintf(str, "cd: %s: No such file or directory\n\r", param_list[1]);
			shell_puts(str);
			return;
		}

		/* not a directory */
		if(dir.inode_dir->i_mode != S_IFDIR) {
			sprintf(str, "cd: %s: Not a directory\n\r", param_list[1]);
			shell_puts(str);
			return;
		}

		shell_dir_curr = dir.inode_dir;
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

	fs_get_pwd(path, shell_dir_curr);

	sprintf(str, "%s\n\r", path);
	shell_puts(str);
}

void shell_cmd_cat(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char path[PATH_LEN_MAX] = {0};

	if(param_list[1][0] != '/') {
		/* input is a relative path */
		char pwd[200] = {0};
		fs_get_pwd(pwd, shell_dir_curr);

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
