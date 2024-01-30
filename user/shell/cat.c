#include <stdio.h>
#include <string.h>
#include <sys/limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kconfig.h"
#include "shell.h"

int cat(int argc, char *argv[])
{
    char path[PATH_MAX] = {0};

    if (argv[1][0] != '/') {
        /* Input filename using relative path */
        char pwd[PATH_MAX] = {0};
        getcwd(pwd, PATH_MAX);

        snprintf(path, PATH_MAX, "%s%s", pwd, argv[1]);
    } else {
        /* Input filename using absolute path */
        strncpy(path, argv[1], PATH_MAX);
    }

    char str[PRINT_SIZE_MAX] = {0};

    /* Open the file */
    FILE *file = fopen(path, "");
    if (!file) {
        snprintf(str, PRINT_SIZE_MAX, "cat: cannot open `%s'\n\r", path);
        shell_puts(str);
        return 1;
    }

    struct stat stat;
    fstat(fileno(file), &stat);

    /* Reset read position of the file */
    fseek(file, 0, SEEK_SET);

    /* Calculate the iteration times to print the whole file */
    int size = stat.st_size / PRINT_SIZE_MAX;
    if ((stat.st_size % PRINT_SIZE_MAX) > 0)
        size++;

    /* Read and print the file */
    for (int i = 0; i < size; i++) {
        int recvd = fread(str, PRINT_SIZE_MAX - 1, 1, file);
        str[recvd] = '\0';

        shell_puts(str);
    }

    fclose(file);

    return 0;
}

HOOK_SHELL_CMD("cat", cat);
