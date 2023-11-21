#include <stdio.h>
#include <sys/resource.h>
#include <task.h>
#include <tenok.h>
#include <time.h>
#include <unistd.h>

#include "shell.h"

extern char _shell_cmds_start;
extern char _shell_cmds_end;
extern struct inode *shell_dir_curr;

struct shell shell;

static struct shell_history history[SHELL_HISTORY_MAX];
static struct shell_autocompl autocompl[100]; /* FIXME */

static char shell_path[PATH_LEN_MAX];
static char prompt[SHELL_PROMPT_LEN_MAX];

void shell_task(void)
{
    setprogname("shell");

    struct shell_cmd *shell_cmds = (struct shell_cmd *) &_shell_cmds_start;
    int shell_cmd_cnt = SHELL_CMDS_CNT(_shell_cmds_start, _shell_cmds_end);

    /* Shell initialization */
    shell_init(&shell, shell_cmds, shell_cmd_cnt, history, SHELL_HISTORY_MAX,
               autocompl);
    shell_serial_init();

    shell_puts("type `help' for help\n\r");

    while (1) {
        getcwd(shell_path, PATH_LEN_MAX);
        snprintf(prompt, SHELL_PROMPT_LEN_MAX, __USER_NAME__ "@%s:%s$ ",
                 __BOARD_NAME__, shell_path);
        shell_set_prompt(&shell, prompt);

        shell_listen(&shell);
        shell_execute(&shell);
    }
}

HOOK_USER_TASK(shell_task, 3, 2048);
