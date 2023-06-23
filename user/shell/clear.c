#include "shell.h"

void shell_cmd_clear(int argc, char *argv[])
{
    shell_cls();
}

HOOK_SHELL_CMD(clear);
