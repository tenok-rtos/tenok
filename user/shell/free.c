#include <stdio.h>

#include <mm/mpool.h>

#include "shell.h"

extern struct memory_pool mem_pool;

void shell_cmd_free(int argc, char *argv[])
{
    char str[PRINT_SIZE_MAX] = {0};

    int total = mem_pool.size;
    int used = mem_pool.offset;
    int _free = total - used;

    snprintf(str, PRINT_SIZE_MAX,
             "               total    used    free\n\r"
             "Memory pool: %7d %7d %7d\n\r",
             total, used, _free);
    shell_puts(str);
}

HOOK_SHELL_CMD(free);
