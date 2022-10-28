#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#define DEF_SYSCALL(func, _num) \
	{.syscall_handler = sys_ ## func, .num = _num}

typedef struct {
	void (* syscall_handler)(void);
	uint32_t num;
} syscall_info_t;

typedef struct {
	uint32_t count;
	list_t wait_list;
} sem_t;

uint32_t sem_up(uint32_t *_lock, uint32_t new_val);
uint32_t sem_down(uint32_t *_lock, uint32_t new_val);

int fork(void);
uint32_t sleep(uint32_t ticks);
uint32_t getpriority(void);
int setpriority(int which, int who, int prio);
int getpid(void);
int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_post(sem_t *sem);
int sem_wait(sem_t *sem);

#endif
