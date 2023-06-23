#include <stdio.h>
#include "shell.h"
#include "mpool.h"

extern struct memory_pool mem_pool;

void shell_cmd_mpool(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN])
{
    char str[PRINT_SIZE_MAX] = {0};

    snprintf(str, PRINT_SIZE_MAX, "[kernel memory pool] size: %d bytes, used: %d bytes\n\r",
             mem_pool.size, mem_pool.offset);
    shell_puts(str);
}

HOOK_SHELL_CMD(mpool);
