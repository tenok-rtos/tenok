#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#define DEF_SYSCALL(func, _num) \
	{.syscall_handler = sys_ ## func, .num = _num}

typedef struct {
	void (* syscall_handler)(void);
	uint32_t num;
} syscall_info_t;

int fork();
uint32_t sleep(uint32_t ticks);

#endif
