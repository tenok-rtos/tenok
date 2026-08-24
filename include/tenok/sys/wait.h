/* Tenok has no child processes, these only exist so that the shell parses */
#ifndef _TENOK_SYS_WAIT_H
#define _TENOK_SYS_WAIT_H

#define WNOHANG 1
#define WUNTRACED 2

#define WIFEXITED(s) (((s) &0x7f) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WIFSIGNALED(s) ((((s) &0x7f) + 1) >> 1 > 0)
#define WTERMSIG(s) ((s) &0x7f)
#define WIFSTOPPED(s) (((s) &0xff) == 0x7f)
#define WSTOPSIG(s) WEXITSTATUS(s)
#define WCOREDUMP(s) ((s) &0x80)

int waitpid(int pid, int *status, int options);
int wait(int *status);

#endif
