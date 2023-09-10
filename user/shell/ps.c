#include <kernel/kernel.h>

#include "shell.h"

void shell_cmd_ps(int argc, char *argv[])
{
    char str[PRINT_SIZE_MAX];
    sprint_tasks(str, PRINT_SIZE_MAX);
    shell_puts(str);
}

HOOK_SHELL_CMD(ps);
