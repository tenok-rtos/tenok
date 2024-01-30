#include <stdio.h>

#include "kconfig.h"
#include "shell.h"

int uname(int argc, char *argv[])
{
    char str[PRINT_SIZE_MAX] = {0};
    snprintf(str, sizeof(str), "Tenok %s %s %s\n\r", __REVISION__,
             __TIMESTAMP__, __ARCH__);
    shell_puts(str);

    return 0;
}

HOOK_SHELL_CMD("uname", uname);
