#include <stdint.h>
#include <string.h>

#include "shell.h"

extern struct shell_cmd _shell_cmds_start[];
extern struct shell_cmd _shell_cmds_end[];

void shell_cmd_help(int argc, char *argv[])
{
    struct shell_cmd *shell_cmds = _shell_cmds_start;
    int shell_cmd_cnt = SHELL_CMDS_CNT(_shell_cmds_start, _shell_cmds_end);

    char s[SHELL_PROMPT_LEN_MAX + 2] = {'\0'}; /* Reserved 2 for new line */
    int buf_len = 0;

    shell_puts("supported commands:\n\r");

    for (int i = 0; i < shell_cmd_cnt; i++) {
        int cmd_len = strlen(shell_cmds[i].name);

        /* Buffer is full, print the message out */
        if ((buf_len + cmd_len + 1) >= SHELL_PROMPT_LEN_MAX) {
            strcat(s, "\n\r");
            shell_puts(s);
            buf_len = 0;
            s[0] = '\0';
        }

        /* Append command name to the buffer */
        strcat(s, shell_cmds[i].name);
        strcat(s, " ");
        buf_len += cmd_len + 1;
    }

    /* Print out the left message */
    if (buf_len > 0) {
        strcat(s, "\n\r");
        shell_puts(s);
    }
}

HOOK_SHELL_CMD(help);
