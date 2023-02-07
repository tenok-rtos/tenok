#ifndef __SHELL_CMD_H__
#define __SHELL_CMD_H__

#include "shell.h"

void shell_cmd_help(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt);
void shell_cmd_clear(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt);
void shell_cmd_history(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt);
void shell_cmd_ps(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt);
void shell_cmd_echo(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt);

#endif
