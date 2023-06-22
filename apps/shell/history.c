#include "shell.h"

extern struct shell shell;

void shell_cmd_history(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN])
{
    shell_print_history(&shell);
}
