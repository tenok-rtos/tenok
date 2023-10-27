#include <tenok.h>
#include <stdio.h>
#include <string.h>

#include "shell.h"

static void stack_usage(char *buf, size_t buf_size, size_t stack_usage, size_t stack_size)
{
    int integer = (stack_usage * 100) / stack_size;
    int fraction = ((stack_usage * 1000) / stack_size) - (integer * 10);
    snprintf(buf, buf_size, "%d.%d", integer, fraction);
}

static void ps(void)
{
    struct procstat_info info[TASK_CNT_MAX];
    int task_cnt = procstat(info);

    char *stat;
    char s[100] = {0};

    shell_puts("PID\tPR\tSTAT\tSTACK\%\t  COMMAND\n\r");

    for(int i = 0; i < task_cnt; i++) {
        switch(info[i].status) {
            case THREAD_WAIT:
                stat = "S";
                break;
            case THREAD_SUSPENDED:
                stat = "T";
                break;
            case THREAD_READY:
            case THREAD_RUNNING:
                stat = "R";
                break;
            default:
                stat = "?";
                break;
        }

        char s_stack_usage[10] = {0};
        stack_usage(s_stack_usage, 10, info[i].stack_usage, info[i].stack_size);

        if(info[i].privileged) {
            snprintf(s, 100, "%d\t%d\t%s\t%s\t  [%s]\n\r",
                     info[i].pid, info[i].priority,
                     stat, s_stack_usage,
                     info[i].name);
        } else {
            snprintf(s, 100, "%d\t%d\t%s\t%s\t  %s\n\r",
                     info[i].pid, info[i].priority,
                     stat, s_stack_usage,
                     info[i].name);
        }

        shell_puts(s);
    }

}

void shell_cmd_ps(int argc, char *argv[])
{
    if(argc == 1) {
        ps();
        return;
    } else if(argc == 2 &&
              (!strcmp("-h", argv[1]) ||
               !strcmp("--help", argv[1]))) {
        shell_puts("process state codes:\n\r"
                   "  R    running or runnable\n\r"
                   "  T    stopped (suspended)\n\r"
                   "  S    sleep\n\r");
        return;
    }

    shell_puts("Usage: ps [-h]\n\r");
}

HOOK_SHELL_CMD(ps);
