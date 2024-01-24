#include "shell.h"

extern struct shell shell;

void history(int argc, char *argv[])
{
    shell_print_history(&shell);
}

HOOK_SHELL_CMD("history", history);
