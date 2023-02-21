#include <stdio.h>
#include <string.h>
#include "shell.h"
#include "kernel.h"
#include "fs.h"
#include "syscall.h"

extern struct shell_struct shell;
extern struct inode inodes[INODE_CNT_MAX];

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
	          "help, clear, history, ps, echo, ls, cd, pwd, cat, file\n\r";
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
	char str[PRINT_SIZE_MAX];
	sprint_tasks(str, PRINT_SIZE_MAX);
	shell_puts(str);
}

void shell_cmd_echo(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char str[PRINT_SIZE_MAX] = {0};

	int i;
	for(i = 1; i < param_cnt - 1; i++) {
		snprintf(str, PRINT_SIZE_MAX, "%s%s ", str, param_list[i]);
	}
	snprintf(str, PRINT_SIZE_MAX, "%s%s\n\r", str, param_list[param_cnt-1]);

	shell_puts(str);
}

void shell_cmd_ls(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char str[PRINT_SIZE_MAX] = {0};

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
			snprintf(str, PRINT_SIZE_MAX, "%s%s/  ", str, dirent.d_name);
		} else {
			snprintf(str, PRINT_SIZE_MAX, "%s%s  ", str, dirent.d_name);
		}
	}

	snprintf(str, PRINT_SIZE_MAX, "%s\n\r", str);

	shell_puts(str);
}

void shell_cmd_cd(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char str[PRINT_SIZE_MAX] = {0};

	DIR dir;

	if(param_cnt == 1) {
		opendir("/", &dir);
		shell_dir_curr = dir.inode_dir;
	} else if(param_cnt == 2) {
		/* handle cd .. */
		if(strncmp("..", param_list[1], PARAM_LEN_MAX) == 0) {
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
			snprintf(path, PATH_LEN_MAX, "%s%s", path, param_list[1]);
		}

		/* open the directory */
		opendir(path, &dir);

		/* directory not found */
		if(dir.inode_dir == NULL) {
			snprintf(str, PRINT_SIZE_MAX, "cd: %s: No such file or directory\n\r", param_list[1]);
			shell_puts(str);
			return;
		}

		/* not a directory */
		if(dir.inode_dir->i_mode != S_IFDIR) {
			snprintf(str, PRINT_SIZE_MAX, "cd: %s: Not a directory\n\r", param_list[1]);
			shell_puts(str);
			return;
		}

		shell_dir_curr = dir.inode_dir;
	} else {
		shell_puts("cd: too many  arguments\n\r");
		return;
	}
}

void shell_cmd_pwd(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char str[PRINT_SIZE_MAX] = {0};
	char path[PATH_LEN_MAX] = {'/'};

	fs_get_pwd(path, shell_dir_curr);

	snprintf(str, PATH_LEN_MAX, "%s\n\r", path);
	shell_puts(str);
}

void shell_cmd_cat(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char path[PATH_LEN_MAX] = {0};

	if(param_list[1][0] != '/') {
		/* input is a relative path */
		char pwd[PATH_LEN_MAX] = {0};
		fs_get_pwd(pwd, shell_dir_curr);

		snprintf(path, PATH_LEN_MAX, "%s%s", pwd, param_list[1]);
	} else {
		/* input is a absolute path */
		strncpy(path, param_list[1], PATH_LEN_MAX);
	}

	char str[PRINT_SIZE_MAX] = {0};

	/* open the file */
	int fd = open(path, 0, 0);
	if(fd == -1) {
		snprintf(str, PRINT_SIZE_MAX, "cat: %s: No such file or directory\n\r", path);
		shell_puts(str);
		return;
	}

	/* check if the file mode is set as regular file */
	struct stat stat;
	fstat(fd, &stat);
	if(stat.st_mode != S_IFREG) {
		snprintf(str, PRINT_SIZE_MAX, "cat: %s: Invalid argument\n\r", path);
		shell_puts(str);
		return;
	}

	/* reset the start position of the file */
	lseek(fd, 0, SEEK_SET);

	/* read file content until EOF is detected */
	signed char c;

	int i;
	for(i = 0; i < stat.st_size; i++) {
		if(i >= (PRINT_SIZE_MAX))
			break;

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

void shell_cmd_file(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char str[PRINT_SIZE_MAX] = {0};

	/* no file is under this directory */
	if(shell_dir_curr->i_size == 0)
		return;

	/* get current directoory path */
	char path[PATH_LEN_MAX] = {0};
	fs_get_pwd(path, shell_dir_curr);

	/* open the directory */
	DIR dir;
	opendir(path, &dir);

	char file_name[FILE_NAME_LEN_MAX] = {0};

	/* check input agrument counts */
	if(param_cnt != 2) {
		shell_puts("Usage: file <file>\n\r");
		return;
	}

	/* get rid of the directory symbol '/' */
	int len = strlen(param_list[1]);
	if(param_list[1][len - 1] == '/') {
		strncpy(file_name, param_list[1], len - 1);
	} else {
		strncpy(file_name, param_list[1], FILE_NAME_LEN_MAX);
	}

	/* enumerate the directory */
	struct dirent dirent;
	while((readdir(&dir, &dirent)) != -1) {
		if(strncmp(file_name, dirent.d_name, FILE_NAME_LEN_MAX) == 0) {
			switch(dirent.d_type) {
			case S_IFIFO:
				snprintf(str, PRINT_SIZE_MAX, "%s: fifo (named pipe)\n\r", dirent.d_name);
				break;
			case S_IFCHR:
				snprintf(str, PRINT_SIZE_MAX, "%s: character device\n\r", dirent.d_name);
				break;
			case S_IFBLK:
				snprintf(str, PRINT_SIZE_MAX, "%s: block device\n\r", dirent.d_name);
				break;
			case S_IFREG:
				snprintf(str, PRINT_SIZE_MAX, "%s: regular file\n\r", dirent.d_name);
				break;
			case S_IFDIR:
				snprintf(str, PRINT_SIZE_MAX, "%s: directory\n\r", dirent.d_name);
				break;
			}

			shell_puts(str);
			return;
		}
	}

	snprintf(str, PRINT_SIZE_MAX, "%s: cannot open `%s' (No such file or directory)\n\r", file_name, file_name);
	shell_puts(str);
}
