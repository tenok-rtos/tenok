#ifndef __SHELL_CMD_H__
#define __SHELL_CMD_H__

#include "fs.h"
#include "shell.h"

void shell_path_init(void);

void shell_cmd_help(char argv[SHELL_ARG_CNT][SHELL_ARG_LEN], int argc);
void shell_cmd_clear(char argv[SHELL_ARG_CNT][SHELL_ARG_LEN], int argc);
void shell_cmd_history(char argv[SHELL_ARG_CNT][SHELL_ARG_LEN], int argc);
void shell_cmd_ps(char argv[SHELL_ARG_CNT][SHELL_ARG_LEN], int argc);
void shell_cmd_echo(char argv[SHELL_ARG_CNT][SHELL_ARG_LEN], int argc);
void shell_cmd_ls(char argv[SHELL_ARG_CNT][SHELL_ARG_LEN], int argc);
void shell_cmd_cd(char argv[SHELL_ARG_CNT][SHELL_ARG_LEN], int argc);
void shell_cmd_pwd(char argv[SHELL_ARG_CNT][SHELL_ARG_LEN], int argc);
void shell_cmd_cat(char argv[SHELL_ARG_CNT][SHELL_ARG_LEN], int argc);
void shell_cmd_file(char argv[SHELL_ARG_CNT][SHELL_ARG_LEN], int argc);
void shell_cmd_mpool(char argv[SHELL_ARG_CNT][SHELL_ARG_LEN], int argc);

#endif
