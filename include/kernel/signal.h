#ifndef __KERNEL_SIGNAL_H__
#define __KERNEL_SIGNAL_H__

#include <stdbool.h>

bool is_signal_defined(int signum);
uint32_t sig2bit(int signum);
int get_signal_index(int signum);

#endif
