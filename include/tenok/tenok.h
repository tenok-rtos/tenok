#ifndef __TENOK_H__
#define __TENOK_H__

/* non-posix syscalls */
void setprogname(const char *name);
uint32_t delay_ticks(uint32_t ticks);

#endif
