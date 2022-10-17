#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>
#include "os_config.h"

typedef void (*task_function_t)(void *);

enum {
	TASK_READY = 0,
	TASK_RUNNING = 1,
	TASK_WAIT = 2,
	TASK_SUSPENDED = 3
};

/* layout of the user stack */
typedef struct {
	uint32_t r4;
	uint32_t r5;
	uint32_t r6;
	uint32_t r7;
	uint32_t r8;
	uint32_t r9;
	uint32_t r10;
	uint32_t r11;
	uint32_t _lr;
	uint32_t _r7; //syscall number
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
	uint32_t r12;
	uint32_t lr;
	uint32_t pc;
	uint32_t xpsr;
	uint32_t stack[TASK_STACK_SIZE - 17];
} user_stack_t;

/* task control block */
struct tcb {
	user_stack_t *stack_top;         //pointer of the stack top address
	uint32_t stack[TASK_STACK_SIZE]; //stack memory

	char task_name[TASK_NAME_LEN_MAX];

	int priority;
	int status;

	uint32_t ticks_to_delay;

	struct tcb *next;
	struct tcb *last;
};
typedef struct tcb tcb_t;

void task_register(task_function_t task_func,
                   const char *task_name,
                   uint32_t stack_depth,
                   void * const task_params,
                   uint32_t priority,
                   tcb_t *tcb);
void os_start(void);

void os_env_init(uint32_t stack);
uint32_t *jump_to_user_space(uint32_t stack);

void task_delay(uint32_t ms);
void task_yield(void);

#endif
