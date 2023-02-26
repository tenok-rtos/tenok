#ifndef __SHELL_CMD_H__
#define __SHELL_CMD_H__

#include "fs.h"
#include "shell.h"

void shell_path_init(void);

void shell_cmd_help(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN]);
void shell_cmd_clear(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN]);
void shell_cmd_history(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN]);
void shell_cmd_ps(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN]);
void shell_cmd_echo(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN]);
void shell_cmd_ls(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN]);
void shell_cmd_cd(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN]);
void shell_cmd_pwd(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN]);
void shell_cmd_cat(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN]);
void shell_cmd_file(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN]);
void shell_cmd_mpool(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN]);

#endif
