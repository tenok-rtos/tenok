#include "shell.h"

extern struct shell shell;

int history(int argc, char *argv[])
{
    shell_print_history(&shell);
    return 0;
}

HOOK_SHELL_CMD("history", history);
