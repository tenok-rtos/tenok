#include "task.h"
#include "shell.h"
#include "syscall.h"
#include "fs.h"

extern char _shell_cmds_start;
extern char _shell_cmds_end;
extern struct inode *shell_dir_curr;

struct shell shell;
struct shell_history history[SHELL_HISTORY_MAX];
struct shell_autocompl autocompl[100]; //XXX: handle with mpool

void boot_message(void)
{
    //TODO: implement dev_info() like linux kernel

    char s[100] = {0};

    snprintf(s, 100, "[0.000000] firmware build time: %s %s\n\r", __TIME__, __DATE__);
    shell_puts(s);

    snprintf(s, 100, "[0.000000] chardev [serial0] for console\n\r");
    shell_puts(s);

    snprintf(s, 100, "[0.000000] chardev [serial1] for mavlink\n\r");
    shell_puts(s);

    snprintf(s, 100, "[0.000000] chardev [serial2] for debug-link\n\r");
    shell_puts(s);

    snprintf(s, 100, "type `help' for help\n\r");
    shell_puts(s);
}

void shell_task(void)
{
    set_program_name("shell");
    setpriority(0, getpid(), 2);

    char shell_path[PATH_LEN_MAX] = {0};
    char prompt[SHELL_PROMPT_LEN_MAX] = {0};

    struct shell_cmd *shell_cmds = (struct shell_cmd *)&_shell_cmds_start;
    int  shell_cmd_cnt = SHELL_CMDS_CNT(_shell_cmds_start, _shell_cmds_end);

    /* shell initialization */
    shell_init(&shell, shell_cmds, shell_cmd_cnt, history, SHELL_HISTORY_MAX, autocompl);
    shell_path_init();
    shell_serial_init();

    /* clean screen */
    //shell_cls();

    boot_message();

    while(1) {
        fs_get_pwd(shell_path, shell_dir_curr);
        snprintf(prompt, SHELL_PROMPT_LEN_MAX, __USER_NAME__ "@%s:%s$ ",  __BOARD_NAME__, shell_path);
        shell_set_prompt(&shell, prompt);

        shell_listen(&shell);
        shell_execute(&shell);
    }
}

HOOK_USER_TASK(shell_task);
