/**
 * @file
 *
 * What a program asks about the thread it is running in. Only the name is
 * asked for on Tenok, which is the one thing a thread of it carries.
 */
#ifndef _TENOK_SYS_PRCTL_H
#define _TENOK_SYS_PRCTL_H

#include "kconfig.h"

/* How much room PR_GET_NAME writes into, which is the longest name a thread
 * can be given
 */
#define PR_NAME_MAX THREAD_NAME_MAX

/* What prctl() is asked to do. Only the two Tenok answers are named */
#define PR_SET_NAME 15
#define PR_GET_NAME 16

/**
 * @brief  Act on the thread the caller is running in.
 * @param  option: PR_SET_NAME or PR_GET_NAME.
 * @param  arg: The name to take, or where to put the one it has.
 * @retval int: 0 on success and -1 on error, with the reason left in errno.
 */
int prctl(int option, ...);

#endif
