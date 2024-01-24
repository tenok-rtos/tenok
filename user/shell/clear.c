#include "shell.h"

void clear(int argc, char *argv[])
{
    shell_cls();
}

HOOK_SHELL_CMD("clear", clear);
