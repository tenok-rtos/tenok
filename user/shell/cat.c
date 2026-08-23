#include <stdio.h>

#include "kconfig.h"
#include "shell.h"

int cat(int argc, char *argv[])
{
    char str[PRINT_SIZE_MAX] = {0};

    /* Both relative and absolute paths are resolved by the kernel */
    const char *path = argv[1];

    /* Open the file */
    FILE *file = fopen(path, "");
    if (!file) {
        snprintf(str, PRINT_SIZE_MAX, "cat: cannot open `%s'\n\r", path);
        shell_puts(str);
        return 1;
    }

    /* Reset read position of the file */
    fseek(file, 0, SEEK_SET);

    /* Read and print the file */
    size_t recvd;
    while ((recvd = fread(str, 1, PRINT_SIZE_MAX - 1, file)) > 0) {
        str[recvd] = '\0';
        shell_puts(str);
    }

    fclose(file);

    return 0;
}

HOOK_SHELL_CMD("cat", cat);
