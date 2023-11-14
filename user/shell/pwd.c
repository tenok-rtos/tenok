#include <stdio.h>
#include <unistd.h>

#include "shell.h"

void shell_cmd_pwd(int argc, char *argv[])
{
    char str[PRINT_SIZE_MAX] = {0};
    char path[PATH_LEN_MAX] = {'/'};

    getcwd(path, PRINT_SIZE_MAX);

    snprintf(str, PATH_LEN_MAX, "%s\n\r", path);
    shell_puts(str);
}

HOOK_SHELL_CMD(pwd);
