#include <tenok.h>
#include <stdio.h>
#include <string.h>

#include <kernel/kernel.h>

#include "shell.h"

static void ps(void)
{
    struct procstat_info info[TASK_CNT_MAX];
    int task_cnt = procstat(info);

    char *stat;
    char s[100] = {0};

    shell_puts("PID\tPR\tSTAT\tNAME\n\r");

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

        if(info[i].privilege == KERNEL_THREAD) {
            snprintf(s, 100, "%d\t%d\t%s\t[%s]\n\r",
                     info[i].pid, info[i].priority,
                     stat, info[i].name);
        } else {
            snprintf(s, 100, "%d\t%d\t%s\t%s\n\r",
                     info[i].pid, info[i].priority,
                     stat, info[i].name);
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
