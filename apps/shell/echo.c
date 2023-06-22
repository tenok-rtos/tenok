#include <stdio.h>
#include "shell.h"

void shell_cmd_echo(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN])
{
    char str[PRINT_SIZE_MAX] = {0};
    int pos = 0;

    int i;
    for(i = 1; i < argc - 1; i++) {
        pos += snprintf(&str[pos], PRINT_SIZE_MAX, "%s ", argv[i]);
    }

    pos += snprintf(&str[pos], PRINT_SIZE_MAX, "%s\n\r", argv[argc-1]);

    shell_puts(str);
}
