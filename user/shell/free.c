#include <stdio.h>
#include <tenok.h>

#include "kconfig.h"
#include "shell.h"

void shell_cmd_free(int argc, char *argv[])
{
    char str[PRINT_SIZE_MAX] = {0};

    int heap_total = minfo(HEAP_TOTAL_SIZE);
    int heap_free = minfo(HEAP_FREE_SIZE);
    int heap_used = heap_total - heap_free;

    int stack_total = minfo(PAGE_TOTAL_SIZE);
    int stack_free = minfo(PAGE_FREE_SIZE);
    int stack_used = stack_total - stack_free;

    shell_puts("               total    used    free\n\r");

    snprintf(str, PRINT_SIZE_MAX, "Page memory: %7d %7d %7d\n\r", stack_total,
             stack_used, stack_free);
    shell_puts(str);

    snprintf(str, PRINT_SIZE_MAX, "User heap:   %7d %7d %7d\n\r", heap_total,
             heap_used, heap_free);
    shell_puts(str);
}

HOOK_SHELL_CMD(free);
