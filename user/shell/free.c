#include <stdio.h>

#include <mm/mpool.h>

#include "shell.h"

extern struct mpool kmpool;

void shell_cmd_free(int argc, char *argv[])
{
    char str[PRINT_SIZE_MAX] = {0};

    int total = kmpool.size;
    int used = kmpool.offset;
    int _free = total - used;

    snprintf(str, PRINT_SIZE_MAX,
             "               total    used    free\n\r"
             "Memory pool: %7d %7d %7d\n\r",
             total, used, _free);
    shell_puts(str);
}

HOOK_SHELL_CMD(free);
