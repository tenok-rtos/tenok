#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f4xx.h"
#include "uart.h"
#include "shell.h"
#include "syscall.h"

int serial_fd = 0;

void shell_serial_init(void)
{
	serial_fd = open("/dev/serial0", 0, 0);
}

char shell_getc(void)
{
	char c;
	while(read(serial_fd, &c, 1) != 1);

	return c;
}

void shell_puts(char *s)
{
	write(serial_fd, s, strlen(s));
}

static void shell_ctrl_c_handler(struct shell *shell)
{
	shell_puts("^C\n\r");
	shell_puts(shell->prompt_msg);
	shell_reset(shell);
}

static void shell_unknown_cmd_handler(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
	char s[50 + CMD_LEN_MAX];
	snprintf(s, 50 + CMD_LEN_MAX, "unknown command: %s\n\r", param_list[0]);
	shell_puts(s);
}

void shell_cls(void)
{
	shell_puts("\x1b[H\x1b[2J");
}

void shell_init(struct shell *shell, char *prompt_msg, char *ret_cmd)
{
	shell->prompt_msg = prompt_msg;
	shell->prompt_len = strlen(shell->prompt_msg);
	shell->cursor_pos = 0;
	shell->char_cnt = 0;
	shell->buf = ret_cmd;
	memset(shell->buf, '\0', CMD_LEN_MAX);

	shell->history_cnt = 0;
	shell->show_history = false;
	list_init(&shell->history_head);
}

void shell_reset(struct shell *shell)
{
	shell->cursor_pos = 0;
	shell->char_cnt = 0;
}

static void shell_remove_char(struct shell *shell, int remove_pos)
{
	int i;
	for(i = (remove_pos - 1); i < (shell->char_cnt); i++) {
		shell->buf[i] = shell->buf[i + 1];
	}

	shell->buf[shell->char_cnt] = '\0';
	shell->char_cnt--;
	shell->cursor_pos--;

	if(shell->cursor_pos > shell->char_cnt) {
		shell->cursor_pos = shell->char_cnt;
	}
}

static void shell_insert_char(struct shell *shell, char c)
{
	int i;
	for(i = shell->char_cnt; i > (shell->cursor_pos - 1); i--) {
		shell->buf[i] = shell->buf[i - 1];
	}
	shell->char_cnt++;
	shell->buf[shell->char_cnt] = '\0';

	shell->buf[shell->cursor_pos] = c;
	shell->cursor_pos++;
}

static void shell_refresh_line(struct shell *shell)
{
	char s[PROMPT_LEN_MAX * 5];
	snprintf(s, PROMPT_LEN_MAX * 5,
	         "\33[2K\r"   /* clear current line */
	         "%s%s\r"     /* show prompt */
	         "\033[%dC",  /* move cursor */
	         shell->prompt_msg, shell->buf, shell->prompt_len + shell->cursor_pos);
	shell_puts(s);
}

static void shell_cursor_shift_one_left(struct shell *shell)
{
	if(shell->cursor_pos <= 0)
		return;

	shell->cursor_pos--;
	shell_refresh_line(shell);
}

static void shell_cursor_shift_one_right(struct shell *shell)
{
	if(shell->cursor_pos >= shell->char_cnt)
		return;

	shell->cursor_pos++;
	shell_refresh_line(shell);
}

static void shell_reset_history_scrolling(struct shell *shell)
{
	shell->show_history = false;
}

static void shell_push_new_history(struct shell *shell, char *cmd)
{
	struct list *history_list;
	struct shell_history *history_new;

	/* check the size of the history list */
	if(shell->history_cnt < HISTORY_MAX_SIZE) {
		/* history list is not full, allocate new memory space */
		history_list = &shell->history[HISTORY_MAX_SIZE - shell->history_cnt - 1].list;
		shell->history_cnt++;

		/* push new history record into the list */
		history_new = list_entry(history_list, struct shell_history, list);
		strncpy(history_new->cmd, shell->buf, CMD_LEN_MAX);
		list_push(&shell->history_head, history_list);
	} else {
		/* history list is full, pop the oldest record */
		history_list = list_pop(&shell->history_head);

		/* push new history record into the list */
		history_new = list_entry(history_list, struct shell_history, list);
		strncpy(history_new->cmd, shell->buf, CMD_LEN_MAX);
		list_push(&shell->history_head, history_list);
	}

	/* initialize the history pointer */
	shell->history_curr = &shell->history_head;

	/* reset the history reading */
	shell->show_history = false;
}

static bool shell_ac_compare(char *user_input, char *shell_cmd, size_t size)
{
	int i;
	for(i = 0; i < size; i++) {
		if(user_input[i] != shell_cmd[i])
			return false;
	}

	return true;
}

static void shell_reset_autocomplete(struct shell *shell)
{
	shell->ac_ready = false;
}

static void shell_autocomplete(struct shell *shell)
{
	/* find the start position of the first argument */
	int start_pos = 0;
	while((shell->buf[start_pos] == ' ') && (start_pos < shell->char_cnt))
		start_pos++;

	/* find the end position of the first argument */
	int end_pos = start_pos;
	while((shell->buf[end_pos] != ' ') && (end_pos < shell->char_cnt))
		end_pos++;

	/* deactivate the autocompletion besides the first arguments */
	if((shell->cursor_pos < start_pos) || (shell->cursor_pos > end_pos))
		return;

	if(shell->ac_ready == true) {
		return;
	}

	int size = shell->cursor_pos - start_pos;

	char *user_cmd = &shell->buf[start_pos];
	char *shell_cmd;

	/* populate the suggestion word list */
	int i;
	for(i = 0; i < shell->cmd_cnt; i++) {
		shell_cmd = shell->shell_cmds[i].name;
		if(shell_ac_compare(user_cmd, shell_cmd, size)) {
			shell_puts(&shell_cmd[size]);
		}
	}

	shell->ac_ready = true;
}

void shell_print_history(struct shell *shell)
{
	char s[CMD_LEN_MAX * 3];

	struct list *list_curr;
	struct shell_history *history;

	int i = 0;
	list_for_each(list_curr, &shell->history_head) {
		history = list_entry(list_curr, struct shell_history, list);

		snprintf(s, CMD_LEN_MAX * 3, "%d %s\n\r", ++i, history->cmd);
		shell_puts(s);
	}
}

static void shell_ctrl_a_handler(struct shell *shell)
{
	shell->cursor_pos = 0;
	shell_refresh_line(shell);
}

static void shell_ctrl_e_handler(struct shell *shell)
{
	if(shell->char_cnt == 0)
		return;

	shell->cursor_pos = shell->char_cnt;
	shell_refresh_line(shell);
}

static void shell_ctrl_u_handler(struct shell *shell)
{
	shell->buf[0] = '\0';
	shell->char_cnt = 0;
	shell->cursor_pos = 0;
	shell_refresh_line(shell);
}

static void shell_up_arrow_handler(struct shell *shell)
{
	/* ignore the key pressing if the hisotry list is empty */
	if(list_is_empty(&shell->history_head) == true)
		return;

	if(shell->show_history == false) {
		/* create a backup of the user input */
		strncpy(shell->input_backup, shell->buf, CMD_LEN_MAX);

		/* history reading mode is on */
		shell->show_history = true;
	}

	/* load the next history to show */
	shell->history_curr = shell->history_curr->last;

	/* check if there is more hisotry to present */
	if(shell->history_curr == &shell->history_head) {
		/* restore the user input if the traversal of the history list is done */
		strncpy(shell->buf, shell->input_backup, CMD_LEN_MAX);
		shell->show_history = false;
	} else {
		/* display the history to the user */
		struct shell_history *history = list_entry(shell->history_curr, struct shell_history, list);
		strncpy(shell->buf, history->cmd, CMD_LEN_MAX);
	}

	/* refresh the line */
	shell->char_cnt   = strlen(shell->buf);
	shell->cursor_pos = shell->char_cnt;
	shell_refresh_line(shell);
}

static void shell_down_arrow_handler(struct shell *shell)
{
	/* user did not trigger the history reading */
	if(shell->show_history == false)
		return;

	/* load the previos history */
	shell->history_curr = shell->history_curr->next;

	/* is the the traversal of the history list done? */
	if(shell->history_curr == &shell->history_head) {
		/* restore the user input */
		strncpy(shell->buf, shell->input_backup, CMD_LEN_MAX);
		shell->show_history = false;
	} else {
		/* display the history to the user */
		struct shell_history *history = list_entry(shell->history_curr, struct shell_history, list);
		strncpy(shell->buf, history->cmd, CMD_LEN_MAX);
	}

	/* refresh the line */
	shell->char_cnt   = strlen(shell->buf);
	shell->cursor_pos = shell->char_cnt;
	shell_refresh_line(shell);
}

static void shell_right_arrow_handler(struct shell *shell)
{
	shell_reset_autocomplete(shell);
	shell_cursor_shift_one_right(shell);
}

static void shell_left_arrow_handler(struct shell *shell)
{
	shell_reset_autocomplete(shell);
	shell_cursor_shift_one_left(shell);
}

static void shell_home_handler(struct shell *shell)
{
	shell->cursor_pos = 0;
	shell_refresh_line(shell);
}

static void shell_end_handler(struct shell *shell)
{
	if(shell->char_cnt == 0)
		return;

	shell->cursor_pos = shell->char_cnt;
	shell_refresh_line(shell);
}

static void shell_delete_handler(struct shell *shell)
{
	if(shell->char_cnt == 0 || shell->cursor_pos == shell->char_cnt)
		return;

	shell_reset_history_scrolling(shell);
	shell_remove_char(shell, shell->cursor_pos + 1);
	shell->cursor_pos++;
	shell_refresh_line(shell);
}

static void shell_backspace_handler(struct shell *shell)
{
	if(shell->char_cnt == 0 || shell->cursor_pos == 0)
		return;

	shell_reset_history_scrolling(shell);
	shell_remove_char(shell, shell->cursor_pos);
	shell_refresh_line(shell);
}

void shell_listen(struct shell *shell)
{
	shell_puts(shell->prompt_msg);

	int c;
	char seq[2];
	while(1) {
		c = shell_getc();

		switch(c) {
		case NULL_CH:
			break;
		case CTRL_A:
			shell_ctrl_a_handler(shell);
			break;
		case CTRL_B:
			shell_left_arrow_handler(shell);
			break;
		case CTRL_C:
			shell_ctrl_c_handler(shell);
			break;
		case CTRL_D:
			break;
		case CTRL_E:
			shell_ctrl_e_handler(shell);
			break;
		case CTRL_F:
			shell_right_arrow_handler(shell);
			break;
		case CTRL_G:
			break;
		case CTRL_H:
			break;
		case TAB:
			//shell_autocomplete(shell);
			break;
		case CTRL_J:
			break;
		case ENTER:
			shell_reset_history_scrolling(shell);
			if(shell->char_cnt > 0) {
				shell_puts("\n\r");
				shell_reset(shell);
				shell_push_new_history(shell, shell->buf);
				return;
			} else {
				shell_puts("\n\r");
				shell_puts(shell->prompt_msg);
			}
			break;
		case CTRL_K:
			break;
		case CTRL_L:
			break;
		case CTRL_N:
			break;
		case CTRL_O:
			break;
		case CTRL_P:
			break;
		case CTRL_Q:
			break;
		case CTRL_R:
			break;
		case CTRL_S:
			break;
		case CTRL_T:
			break;
		case CTRL_U:
			shell_ctrl_u_handler(shell);
			break;
		case CTRL_W:
			break;
		case CTRL_X:
			break;
		case CTRL_Y:
			break;
		case CTRL_Z:
			break;
		case ESC_SEQ1:
			seq[0] = shell_getc();
			seq[1] = shell_getc();
			if(seq[0] == ESC_SEQ2) {
				if(seq[1] == UP_ARROW) {
					shell_up_arrow_handler(shell);
					break;
				} else if(seq[1] == DOWN_ARROW) {
					shell_down_arrow_handler(shell);
					break;
				} else if(seq[1] == RIGHT_ARROW) {
					shell_right_arrow_handler(shell);
					break;
				} else if(seq[1] == LEFT_ARROW) {
					shell_left_arrow_handler(shell);
					break;
				} else if(seq[1] == HOME_XTERM) {
					shell_home_handler(shell);
					break;
				} else if(seq[1] == HOME_VT100) {
					shell_home_handler(shell);
					shell_getc();
					break;
				} else if(seq[1] == END_XTERM) {
					shell_end_handler(shell);
					break;
				} else if(seq[1] == END_VT100) {
					shell_end_handler(shell);
					shell_getc();
					break;
				} else if(seq[1] == DELETE && shell_getc() == ESC_SEQ4) {
					shell_delete_handler(shell);
					break;
				}
			}
			break;
		case BACKSPACE:
			shell_backspace_handler(shell);
			break;
		case SPACE:
		default: {
			if(shell->char_cnt != (CMD_LEN_MAX - 1)) {
				shell_reset_history_scrolling(shell);
				shell_insert_char(shell, c);
				shell_refresh_line(shell);
			}
			break;
		}
		}
	}
}

static void shell_split_cmd_token(char *cmd, char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int *param_cnt)
{
	bool no_param_after_space = false;
	int param_list_index = 0;
	int i = 0, j = 0;

	int cmd_s_len = strlen(cmd);

	/* skip spaces before first parameter */
	while(i < cmd_s_len && cmd[i] == ' ') {
		i++;
	}

	for(; i < cmd_s_len; i++) {
		if(cmd[i] == ' ') {
			no_param_after_space = true;

			param_list[param_list_index][j] = '\0';
			param_list_index++;
			j = 0;

			/* exceed maximum parameter count */
			if(param_list_index == PARAM_LIST_SIZE_MAX) {
				*param_cnt = param_list_index;
				return;
			}

			/* skip spaces */
			while(cmd[i + 1] == ' ' && i < cmd_s_len) {
				i++;
			}
		} else {
			no_param_after_space = false;
			param_list[param_list_index][j] = cmd[i];
			j++;
		}
	}

	if(no_param_after_space == true) {
		param_list_index--;
	}

	*param_cnt = param_list_index + 1;
}

void shell_execute(struct shell *shell)
{
	char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX] = {0};
	int param_cnt;
	shell_split_cmd_token(shell->buf, param_list, &param_cnt);

	int i;
	for(i = 0; i < shell->cmd_cnt; i++) {
		if(strcmp(param_list[0], shell->shell_cmds[i].name) == 0) {
			shell->shell_cmds[i].handler(param_list, param_cnt);
			shell->buf[0] = '\0';
			shell_reset(shell);
			return;
		}
	}

	shell_unknown_cmd_handler(param_list, param_cnt);
	shell->buf[0] = '\0';
	shell_reset(shell);
}
