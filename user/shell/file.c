#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kconfig.h"
#include "shell.h"

void shell_cmd_file(int argc, char *argv[])
{
    char str[PRINT_SIZE_MAX] = {0};

    /* Get current directory path */
    char path[PATH_MAX] = {0};
    getcwd(path, PATH_MAX);

    if (argc == 1) {
        shell_puts("Usage: file <file>\n\r");
        return;
    } else if (argc > 2) {
        shell_puts("file: too many arguments\n\r");
        return;
    }

    if (argv[1][0] != '/') {
        /* Input filename using relative path */
        int pos = strlen(path);
        snprintf(&path[pos], PRINT_SIZE_MAX, "%s", argv[1]);
    } else {
        /* Input filename using absolute path */
        strncpy(path, argv[1], PATH_MAX);
    }

    /* Open the file */
    int fd = open(path, 0);
    if (fd < 0) {
        /* Check if it is a direcotry */
        DIR dir;
        if (opendir(path, &dir) == 0) {
            snprintf(str, PRINT_SIZE_MAX, "%s: directory\n\r", path);
            shell_puts(str);
            return;
        }

        /* The given path is neither a file or direcotry */
        snprintf(str, PRINT_SIZE_MAX,
                 "%s: cannot open `%s' (No such file or directory)\n\r", path,
                 path);
        shell_puts(str);
        return;
    }

    /* Read and print the file type */
    struct stat stat;
    fstat(fd, &stat);

    /* Close the file */
    close(fd);

    switch (stat.st_mode) {
    case S_IFIFO:
        snprintf(str, PRINT_SIZE_MAX, "%s: fifo (named pipe)\n\r", path);
        break;
    case S_IFCHR:
        snprintf(str, PRINT_SIZE_MAX, "%s: character device\n\r", path);
        break;
    case S_IFBLK:
        snprintf(str, PRINT_SIZE_MAX, "%s: block device\n\r", path);
        break;
    case S_IFREG:
        snprintf(str, PRINT_SIZE_MAX, "%s: regular file\n\r", path);
        break;
    default:
        snprintf(str, PRINT_SIZE_MAX, "%s: unknown\n\r", path);
        break;
    }

    shell_puts(str);
}

HOOK_SHELL_CMD(file);
