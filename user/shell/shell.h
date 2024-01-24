#ifndef __SHELL_H__
#define __SHELL_H__

#include <stdbool.h>
#include <sys/limits.h>

#include <common/list.h>

#include "kconfig.h"

#define SHELL_ARG_CNT 10

#define SHELL_CMDS_SIZE(sect_start, sect_end) \
    ((uintptr_t) &sect_end - (uintptr_t) &sect_start)

#define SHELL_CMDS_CNT(sect_start, sect_end) \
    (SHELL_CMDS_SIZE(sect_start, sect_end) / sizeof(struct shell_cmd))

#define HOOK_SHELL_CMD(cmd_name, handler_func)            \
    static struct shell_cmd _##shell_cmd_##name           \
        __attribute__((section(".shell_cmds"), used)) = { \
            .name = cmd_name, .handler = handler_func}

enum {
    NULL_CH = 0,      /* Null character */
    CTRL_A = 1,       /* Ctrl + a */
    CTRL_B = 2,       /* Ctrl + b */
    CTRL_C = 3,       /* Ctrl + c */
    CTRL_D = 4,       /* Ctrl + d */
    CTRL_E = 5,       /* Ctrl + e */
    CTRL_F = 6,       /* Ctrl + f */
    CTRL_G = 7,       /* ctrl + g */
    CTRL_H = 8,       /* Ctrl + h */
    TAB = 9,          /* Tab */
    CTRL_J = 10,      /* Ctrl + j */
    CTRL_K = 11,      /* Ctrl + k */
    CTRL_L = 12,      /* Ctrl + l */
    ENTER = 13,       /* Enter */
    CTRL_N = 14,      /* Ctrl + n */
    CTRL_O = 15,      /* Ctrl + o */
    CTRL_P = 16,      /* Ctrl + p */
    CTRL_Q = 17,      /* Ctrl + r */
    CTRL_R = 18,      /* Ctrl + r */
    CTRL_S = 19,      /* Ctrl + s */
    CTRL_T = 20,      /* Ctrl + t */
    CTRL_U = 21,      /* Ctrl + u */
    CTRL_W = 23,      /* Ctrl + w */
    CTRL_X = 24,      /* Ctrl + x */
    CTRL_Y = 25,      /* Ctrl + y */
    CTRL_Z = 26,      /* Ctrl + z */
    ESC_SEQ1 = 27,    /* The first byte of the vt100/xterm esc. seq. */
    SPACE = 32,       /* Space */
    DELETE = 51,      /* Delete, the third byte of the xterm esc. seq. */
    UP_ARROW = 65,    /* Up arrow, the third byte of the xterm esc. seq. */
    DOWN_ARROW = 66,  /* Down arrow, the third byte of the xterm esc. seq. */
    RIGHT_ARROW = 67, /* Right arrow, the third byte of the xterm esc. seq. */
    LEFT_ARROW = 68,  /* Left arrow, the third byte of the xterm esc. seq. */
    END_XTERM = 70,   /* End, the third byte of the xterm esc. seq. */
    END_VT100 = 52,   /* End, the third byte of the vt100 esc. seq. */
    HOME_XTERM = 72,  /* Home, the third byte of the esc. seq. */
    HOME_VT100 = 49,  /* Home, the third byte of the vt100 esc. seq. */
    ESC_SEQ2 = 91,    /* The second byte of the esc. seq. */
    ESC_SEQ4 = 126,   /* The fourth byte of the vt100 esc. seq. */
    BACKSPACE = 127,  /* Backspace */
} KEYS;

struct shell_history {
    char cmd[LINE_MAX];
    struct list_head list;
};

struct shell_autocompl {
    char cmd[LINE_MAX];
};

struct shell {
    int cursor_pos;
    int char_cnt;
    int prompt_len;

    char buf[LINE_MAX];
    char prompt[LINE_MAX];
    char input_backup[LINE_MAX];

    /* Autocomplete */
    bool show_autocompl;
    int autocompl_cursor_pos;
    int autocompl_cnt;
    int autocompl_curr;
    struct shell_autocompl *autocompl; /* Space provided by user */

    /* History */
    int history_max_cnt;
    int history_cnt;
    bool show_history;
    struct shell_history *history; /* Space provided by user */
    struct list_head history_head;
    struct list_head *history_curr;

    /* Shell commands */
    struct shell_cmd *shell_cmds;
    int cmd_cnt;
};

struct shell_cmd {
    void (*handler)(int argc, char *argv[SHELL_ARG_CNT]);
    char name[LINE_MAX];
};

/* Serial I/O */
void shell_serial_init(void);
char shell_getc(void);
void shell_puts(char *s);
void shell_cls(void);

/* Shell functions */
void shell_init(struct shell *shell,
                struct shell_cmd *shell_cmds,
                int cmd_cnt,
                struct shell_history *history,
                int history_max_cnt,
                struct shell_autocompl *autocompl);
void shell_init_minimal(struct shell *shell);
void shell_path_init(void);
void shell_set_prompt(struct shell *shell, char *new_prompt);
void shell_listen(struct shell *_shell);
void shell_execute(struct shell *shell);
void shell_print_history(struct shell *shell);

#endif
