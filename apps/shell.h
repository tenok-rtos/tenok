#ifndef __SHELL_H__
#define __SHELL_H__

#include <stdbool.h>
#include "list.h"
#include "kconfig.h"

#define PARAM_LIST_SIZE_MAX 10
#define PARAM_LEN_MAX       10

#define SIZE_OF_SHELL_CMD_LIST(list) (sizeof(list) / sizeof(struct shell_cmd))
#define DEF_SHELL_CMD(cmd_name) {.handler = shell_cmd_ ## cmd_name, .name = #cmd_name}

enum {
	NULL_CH = 0,       /* null character */
	CTRL_A = 1,        /* ctrl + a */
	CTRL_B = 2,        /* ctrl + b */
	CTRL_C = 3,        /* ctrl + c */
	CTRL_D = 4,        /* ctrl + d */
	CTRL_E = 5,        /* ctrl + e */
	CTRL_F = 6,        /* ctrl + f */
	CTRL_G = 7,        /* ctrl + g */
	CTRL_H = 8,        /* ctrl + h */
	TAB = 9,           /* tab */
	CTRL_J = 10,       /* ctrl + j */
	CTRL_K = 11,       /* ctrl + k */
	CTRL_L = 12,       /* ctrl + l */
	ENTER = 13,        /* enter */
	CTRL_N = 14,       /* ctrl + n */
	CTRL_O = 15,       /* ctrl + o */
	CTRL_P = 16,       /* ctrl + p */
	CTRL_Q = 17,       /* ctrl + r */
	CTRL_R = 18,       /* ctrl + r */
	CTRL_S = 19,       /* ctrl + s */
	CTRL_T = 20,       /* ctrl + t */
	CTRL_U = 21,       /* ctrl + u */
	CTRL_W = 23,       /* ctrl + w */
	CTRL_X = 24,       /* ctrl + x */
	CTRL_Y = 25,       /* ctrl + y */
	CTRL_Z = 26,       /* ctrl + z */
	ESC_SEQ1 = 27,     /* first byte of the vt100/xterm escape sequence */
	SPACE = 32,        /* space */
	DELETE = 51,       /* delete, third byte of the xterm escape sequence */
	UP_ARROW = 65,     /* up arrow, third byte of the xterm escape sequence */
	DOWN_ARROW = 66,   /* down arrow, third byte of the xterm escape sequence */
	RIGHT_ARROW = 67,  /* right arrow, third byte of the xterm escape sequence */
	LEFT_ARROW = 68,   /* left arrow, third byte of the xterm escape sequence */
	END_XTERM = 70,    /* end, third byte of the xterm escape sequence */
	END_VT100 = 52,    /* end, third byte of the vt100 escape sequence */
	HOME_XTERM = 72,   /* home, third byte of the escape sequence */
	HOME_VT100 = 49,   /* home, third byte of the vt100 escape sequence */
	ESC_SEQ2 = 91,     /* second byte of the escape sequence */
	ESC_SEQ4 = 126,    /* fourth byte of the vt100 escape sequence */
	BACKSPACE = 127,   /* backspace */
} KEYS;

struct shell_history {
	char cmd[SHELL_CMD_LEN_MAX];
	struct list list;
};

struct shell {
	int cursor_pos;
	int char_cnt;
	int prompt_len;

	char *buf;
	char prompt[SHELL_PROMPT_LEN_MAX];
	char input_backup[SHELL_CMD_LEN_MAX];

	/* autocomplete */
	bool ac_ready;

	/* history */
	int  history_max_cnt;
	int  history_cnt;
	bool show_history;
	struct shell_history *history; //memory for storing the history
	struct list history_head;
	struct list *history_curr;

	/* shell commands */
	struct shell_cmd *shell_cmds;
	int cmd_cnt;
};

struct shell_cmd {
	void (*handler)(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt);
	char name[SHELL_PROMPT_LEN_MAX];
};

/* serial input/output */
void shell_serial_init(void);
void shell_puts(char *s);
void shell_cls(void);

/* shell functions */
void shell_init(struct shell *shell, char *ret_cmd,
                struct shell_cmd *shell_cmds, int cmd_cnt,
                struct shell_history *history, int history_max_cnt);
void shell_set_prompt(struct shell *shell, char *new_prompt);
void shell_listen(struct shell *_shell);
void shell_execute(struct shell *shell);
void shell_print_history(struct shell *shell);

#endif
