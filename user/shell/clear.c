#include "shell.h"

int clear(int argc, char *argv[])
{
    shell_cls();
    return 0;
}

HOOK_SHELL_CMD("clear", clear);
