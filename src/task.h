#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>
#include "config_rtos.h"

typedef uint32_t stack_type_t;
typedef void (*task_function_t)(void *);

enum {
	TASK_READY = 0,
	TASK_RUNNING = 1,
	TASK_WAIT = 2,
	TASK_SUSPENDED = 3
};

/* task control block */
struct tcb {
	volatile stack_type_t *top_of_stack; //point to the top of the stack
	stack_type_t stack[TASK_STACK_SIZE]; //point to the start of the stack

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

void select_task(void);

void task_delay(uint32_t ms);
void task_yield(void);

#endif
