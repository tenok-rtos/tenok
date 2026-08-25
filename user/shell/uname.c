#include <stdio.h>
#include <sys/utsname.h>

#include "kconfig.h"
#include "shell.h"

static int uname_cmd(int argc, char *argv[])
{
    char str[PRINT_SIZE_MAX] = {0};
    struct utsname buf;

    uname(&buf);
    snprintf(str, sizeof(str), "%s %s %s %s\n", buf.sysname, buf.release,
             buf.version, buf.machine);
    shell_puts(str);

    return 0;
}

HOOK_SHELL_CMD("uname", uname_cmd);
