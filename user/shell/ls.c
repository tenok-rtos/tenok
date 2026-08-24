#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kconfig.h"
#include "shell.h"

int ls(int argc, char *argv[])
{
    char str[PRINT_SIZE_MAX] = {0};

    if (argc > 2) {
        shell_puts("ls: too many arguments\n\r");
        return 1;
    }

    /* Both relative and absolute paths are resolved by the kernel */
    const char *path = (argc == 2) ? argv[1] : ".";

    /* Open the directory */
    DIR dir;
    int retval = opendir(path, &dir);

    /* Check if the directory is open successfully */
    if (retval != 0) {
        snprintf(str, PRINT_SIZE_MAX,
                 "ls: cannot access '%s': No such file or directory\n\r", path);
        shell_puts(str);
        return 1;
    }

    /* Enumerate the directory */
    int pos = 0;
    struct dirent dirent;
    while ((readdir(&dir, &dirent)) != -1) {
        if (dirent.d_type == DT_DIR) {
            pos += snprintf(&str[pos], PRINT_SIZE_MAX, "%s/  ", dirent.d_name);
        } else {
            pos += snprintf(&str[pos], PRINT_SIZE_MAX, "%s  ", dirent.d_name);
        }
    }

    snprintf(&str[pos], PRINT_SIZE_MAX, "\n\r");

    shell_puts(str);

    return 0;
}

HOOK_SHELL_CMD("ls", ls);
