#include <stdio.h>
#include <string.h>
#include <tenok.h>

#include "kconfig.h"
#include "shell.h"

static void stack_usage(char *buf,
                        size_t buf_size,
                        size_t stack_usage,
                        size_t stack_size)
{
    int integer = (stack_usage * 100) / stack_size;
    int fraction = ((stack_usage * 1000) / stack_size) - (integer * 10);
    snprintf(buf, buf_size, "%2d.%d", integer, fraction);
}

static void ps_print(void)
{
    char s[PRINT_SIZE_MAX] = {0};

    struct thread_stat info;
    void *next = NULL;

    shell_puts("PID\tPR\tSTAT\tSTACK\%\t  COMMAND\n\r");

    do {
        next = thread_info(&info, next);

        char s_stack_usage[10] = {0};
        stack_usage(s_stack_usage, 10, info.stack_usage, info.stack_size);

        if (info.kernel_thread) {
            snprintf(s, 100, "%d\t%d\t%s\t%s\t  [%s]\n\r", info.pid,
                     info.priority, info.status, s_stack_usage, info.name);
        } else {
            snprintf(s, 100, "%d\t%d\t%s\t%s\t  %s\n\r", info.pid,
                     info.priority, info.status, s_stack_usage, info.name);
        }

        shell_puts(s);
    } while (next != NULL);
}

void ps(int argc, char *argv[])
{
    if (argc == 1) {
        ps_print();
        return;
    } else if (argc == 2 &&
               (!strcmp("-h", argv[1]) || !strcmp("--help", argv[1]))) {
        shell_puts(
            "process state codes:\n\r"
            "  R    running or runnable\n\r"
            "  T    stopped (suspended)\n\r"
            "  S    sleep\n\r");
        return;
    }

    shell_puts("Usage: ps [-h]\n\r");
}

HOOK_SHELL_CMD("ps", ps);
