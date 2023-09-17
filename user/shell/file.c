#include <string.h>

#include <tenok/stdio.h>
#include <tenok/fcntl.h>
#include <tenok/dirent.h>
#include <tenok/unistd.h>
#include <tenok/sys/stat.h>

#include "shell.h"

extern struct inode *shell_dir_curr;

void shell_cmd_file(int argc, char *argv[])
{
    char str[PRINT_SIZE_MAX] = {0};

    /* no file is under this directory */
    if(shell_dir_curr->i_size == 0)
        return;

    /* get current directoory path */
    char path[PATH_LEN_MAX] = {0};
    fs_get_pwd(path, shell_dir_curr);

    if(argc == 1) {
        shell_puts("Usage: file <file>\n\r");
        return;
    } else if(argc > 2) {
        shell_puts("file: too many arguments\n\r");
        return;
    }

    if(argv[1][0] != '/') {
        /* user feed a relative path */
        int pos = strlen(path);
        snprintf(&path[pos], PRINT_SIZE_MAX, "%s", argv[1]);
    } else {
        /* user feed a absolute path */
        strncpy(path, argv[1], PATH_LEN_MAX);
    }

    /* open the file */
    int fd = open(path, 0);
    if(fd == -1) {
        /* maybe it is a direcotry */
        DIR dir;
        if(opendir(path, &dir) == 0) {
            snprintf(str, PRINT_SIZE_MAX, "%s: directory\n\r", path);
            shell_puts(str);
            return;
        }

        /* the given path is neither a file or direcotry */
        snprintf(str, PRINT_SIZE_MAX,
                 "%s: cannot open `%s' (No such file or directory)\n\r", path, path);
        shell_puts(str);
        return;
    }

    /* read and print the file type */
    struct stat stat;
    fstat(fd, &stat);

    switch(stat.st_mode) {
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
    return;
}

HOOK_SHELL_CMD(file);
