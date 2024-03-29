#include <stdio.h>
#include <time.h>

#include "shell.h"

int uptime(int argc, char *argv[])
{
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);

    // TODO: printf() should support %lld
    int time_sec = (int) tp.tv_sec;

    printf("%d seconds up\n\r", time_sec);

    return 0;
}

HOOK_SHELL_CMD("uptime", uptime);
