#include "shell.h"
#include "kernel.h"

void shell_cmd_ps(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN])
{
    char str[PRINT_SIZE_MAX];
    sprint_tasks(str, PRINT_SIZE_MAX);
    shell_puts(str);
}

HOOK_SHELL_CMD(ps);
