#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "kconfig.h"
#include "shell.h"

void cd(int argc, char *argv[])
{
    int retval;
    char str[PRINT_SIZE_MAX] = {0};

    switch (argc) {
    case 1:
        chdir("/");
        return;
    case 2: {
        retval = chdir(argv[1]);
        if (retval == -ENOENT) {
            snprintf(str, PRINT_SIZE_MAX,
                     "cd: %s: No such file or directory\n\r", argv[1]);
            shell_puts(str);
        } else if (retval == -ENOTDIR) {
            snprintf(str, PRINT_SIZE_MAX, "cd: %s: Not a directory\n\r",
                     argv[1]);
            shell_puts(str);
        }
        break;
    }
    default:
        shell_puts("cd: too many arguments\n\r");
        break;
    }
}

HOOK_SHELL_CMD("cd", cd);
