#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "shell.h"

extern struct inode *shell_dir_curr;

void shell_cmd_cat(int argc, char *argv[])
{
    char path[PATH_LEN_MAX] = {0};

    if (argv[1][0] != '/') {
        /* input is a relative path */
        char pwd[PATH_LEN_MAX] = {0};
        fs_get_pwd(pwd, shell_dir_curr);

        snprintf(path, PATH_LEN_MAX, "%s%s", pwd, argv[1]);
    } else {
        /* input is a absolute path */
        strncpy(path, argv[1], PATH_LEN_MAX);
    }

    char str[PRINT_SIZE_MAX] = {0};

    /* open the file */
    FILE *file = fopen(path, "");
    if (!file) {
        snprintf(str, PRINT_SIZE_MAX, "cat: cannot open `%s'\n\r", path);
        shell_puts(str);
        return;
    }

    struct stat stat;
    fstat(fileno(file), &stat);

    /* reset the read position of the file */
    fseek(file, 0, SEEK_SET);

    /* calculate the iteration times to print the whole file */
    int size = stat.st_size / PRINT_SIZE_MAX;
    if ((stat.st_size % PRINT_SIZE_MAX) > 0)
        size++;

    /* read and print the file */
    for (int i = 0; i < size; i++) {
        int recvd = fread(str, PRINT_SIZE_MAX - 1, 1, file);
        str[recvd] = '\0';

        shell_puts(str);
    }

    fclose(file);
}

HOOK_SHELL_CMD(cat);
