#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>

#define TASK_NAME_LEN_MAX (100) //bytes
#define TASK_STACK_SIZE   (512) //words

typedef uint32_t stack_type_t;
typedef void (*task_function_t)(void *);

typedef struct task_control_block {
	volatile stack_type_t *top_of_stack; //point to the top of the stack
	stack_type_t *stack; //point to the start of the stack

	char task_name[TASK_NAME_LEN_MAX];
} tcb_t;

void task_register(task_function_t task_func,
                   const char *task_name,
                   uint32_t stack_depth,
                   void * const task_params,
                   tcb_t *tcb);
void os_start(void);

#endif
