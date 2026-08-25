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

/**
 * @brief  Wait for a child process to change state. Tenok has no child
 *         processes, so the wait always reports that there are none.
 * @param  pid: The process to wait for.
 * @param  status: Where to store the status of the process.
 * @param  options: Flags altering how the wait behaves.
 * @retval int: -1 with errno set to ECHILD.
 */
int waitpid(int pid, int *status, int options);

/**
 * @brief  Wait for any child process to change state. Tenok has no child
 *         processes, so the wait always reports that there are none.
 * @param  status: Where to store the status of the process.
 * @retval int: -1 with errno set to ECHILD.
 */
int wait(int *status);

#endif
