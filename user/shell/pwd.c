#include <stdio.h>
#include <sys/limits.h>
#include <unistd.h>

#include "kconfig.h"
#include "shell.h"

int pwd(int argc, char *argv[])
{
    char str[PRINT_SIZE_MAX] = {0};
    char path[PATH_MAX] = {'/'};

    getcwd(path, PRINT_SIZE_MAX);

    snprintf(str, PATH_MAX, "%s\n\r", path);
    shell_puts(str);

    return 0;
}

HOOK_SHELL_CMD("pwd", pwd);
