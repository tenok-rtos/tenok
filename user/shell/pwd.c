#include <stdio.h>

#include <fs/fs.h>

#include "shell.h"

extern struct inode *shell_dir_curr;

void shell_cmd_pwd(int argc, char *argv[])
{
    char str[PRINT_SIZE_MAX] = {0};
    char path[PATH_LEN_MAX] = {'/'};

    fs_get_pwd(path, shell_dir_curr);

    snprintf(str, PATH_LEN_MAX, "%s\n\r", path);
    shell_puts(str);
}

HOOK_SHELL_CMD(pwd);
