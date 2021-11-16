#include "task.h"

#define INITIAL_XPSR    (0x01000000)
#define START_ADDR_MASK ((stack_type_t) 0xfffffffeUL)

tcb_t *current_tcb;

void task_exit_error(void)
{
	/* should never be happened */
	while(1);
}

void task_register(task_function_t task_func,
                   const char *task_name,
                   uint32_t stack_depth,
                   void * const task_params,
                   tcb_t *tcb)
{
	stack_depth = TASK_STACK_SIZE; //XXX: fixed size for now

	/* initialize task name */
	int i;
	for(i = 0; i < TASK_NAME_LEN_MAX; i++) {
		tcb->task_name[i] = task_name[i];

		if(task_name[i] == '\0') {
			break;
		}
	}
	tcb->task_name[TASK_NAME_LEN_MAX - 1] = '\0';
	
	/*===========================*
	 * initialize the task stack *
         *===========================*/
	stack_type_t *top_of_stack = tcb->stack + stack_depth - 1;

	/* initialize xpsr register */
	*top_of_stack = INITIAL_XPSR;
	top_of_stack--;

	/* initialize pc register */
	*top_of_stack = (stack_type_t)task_func & START_ADDR_MASK;
	top_of_stack--;

	/* initialize lr register */
	*top_of_stack = (stack_type_t)task_exit_error;

	/* initialize r12, r3, r2 and r1 registers as zero */
	top_of_stack -= 5;

	/* initialize r0 register with task function parameters */
	*top_of_stack = (stack_type_t)task_params;
	
	/* initialize r11-r4 registers as zero */
	top_of_stack -= 8;
}

void select_task(void)
{
}

void os_start(void)
{
}
