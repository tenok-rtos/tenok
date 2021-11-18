#include <stddef.h>
#include <stdbool.h>
#include "stm32f4xx.h"
#include "task.h"
#include "config_rtos.h"

#define INITIAL_XPSR (0x01000000)

tcb_t *tasks[OS_MAX_PRIORITY] = {NULL};
int task_nums[OS_MAX_PRIORITY] = {0};

tcb_t *curr_tcb = NULL;

tcb_t tcb_idle_task;

void task_exit_error(void)
{
	/* should never be happened */
	while(1);
}

void task_idle(void *param)
{
	/* idle task is a task with the lowest priority.
         * when the os has nothing to do, the idle task
         * will be executed. */
	while(1);
}

void task_register(task_function_t task_func,
                   const char *task_name,
                   uint32_t stack_depth,
                   void * const task_params,
                   uint32_t priority,
                   tcb_t *new_tcb)
{
	/* check if the task number exceeded the limit */
	uint32_t total_task_num = 0;

	int i;
	for(i = 0; i < OS_MAX_PRIORITY; i++) {
		total_task_num += task_nums[i];
	}

	if(total_task_num >= TASK_NUM_MAX) {
		return;
	}

	//increased the task number according to the priority
	task_nums[priority]++;

	/* no task in the list */
	if(tasks[priority] == NULL) {
		tasks[priority] = new_tcb;
		tasks[priority]->next = new_tcb;
		tasks[priority]->last = new_tcb;
	} else {
		/* append new task at the end of the list */
		new_tcb->next = tasks[priority];
		new_tcb->last = tasks[priority]->last;
		tasks[priority]->last->next = new_tcb;
		tasks[priority]->last = new_tcb;
	}

	stack_depth = TASK_STACK_SIZE; //XXX: fixed size for now
	new_tcb->status = TASK_READY;

	/* initialize task name */
	for(i = 0; i < TASK_NAME_LEN_MAX; i++) {
		new_tcb->task_name[i] = task_name[i];

		if(task_name[i] == '\0') {
			break;
		}
	}
	new_tcb->task_name[TASK_NAME_LEN_MAX - 1] = '\0';
	
	/*===========================*
	 * initialize the task stack *
         *===========================*/
	stack_type_t *top_of_stack = new_tcb->stack + stack_depth - 1;

	/* initialize xpsr register */
	*top_of_stack = INITIAL_XPSR;
	top_of_stack--;

	/* initialize pc register */
	*top_of_stack = (stack_type_t)task_func;
	top_of_stack--;

	/* initialize lr register */
	*top_of_stack = (stack_type_t)task_exit_error;

	/* initialize r12, r3, r2 and r1 registers as zero */
	top_of_stack -= 5;

	/* initialize r0 register with task function parameters */
	*top_of_stack = (stack_type_t)task_params;
	
	/* initialize r11-r4 registers as zero */
	top_of_stack -= 8;

	new_tcb->top_of_stack = top_of_stack;
}

void select_task(void)
{
	int i, j;
	tcb_t *tcb;

	bool change_task = false;
	bool task_selected = false;

	/* current task is no longer in the running state,
         * we should select next ready task */
	if(curr_tcb->status != TASK_RUNNING || curr_tcb == &tcb_idle_task) {
		change_task = true;
	}

	/* find the next ready task with the highest priority */
	for(i = OS_MAX_PRIORITY - 1; i >= 0; i--) {
		/* current task list is empty, skip it */
		if(tasks[i] == NULL) {
			continue;
		}

		/* initialize the tcb iterator */
		tcb = tasks[i];

		/* iterate through the current task list */
		for(j = 0; j < task_nums[i]; j++) {
			if((curr_tcb->priority < tcb->priority || change_task == true) &&
                           tcb != curr_tcb &&
                           tcb->status == TASK_READY &&
			   task_selected == false) {
				//change status of the current task
				if(curr_tcb->status == TASK_RUNNING) {
					curr_tcb->status = TASK_READY;
				}

				//next task to run is selected
				task_selected = true;
				curr_tcb = tcb;
				curr_tcb->status = TASK_RUNNING;
			}

			/* update remained delay ticks */
			if(tcb->ticks_to_delay > 0) {
				tcb->ticks_to_delay--;

				if(tcb->ticks_to_delay == 0) {
					tcb->status = TASK_READY;
				}
			}

			/* next task */
			tcb = tcb->next;
		}
	}
}

void os_start(void)
{
	/* register a idle task that do nothing */
	task_register(task_idle, "idle task", 0, NULL, 0, &tcb_idle_task);

	/* find a task with the highest priority to start */
	int i;
	for(i = OS_MAX_PRIORITY - 1; i >= 0; i--) {
		if(tasks[i] != NULL) {
			curr_tcb = tasks[i];
			curr_tcb->status = TASK_RUNNING;
			break;
		}
	}

	/* initialize systick timer */
	SysTick_Config(SystemCoreClock / OS_TICK_FREQUENCY);

	/* trigger svc handler to jump to the first task */
	asm("svc 0 \n"
            "nop   \n");
}

__attribute__((naked)) void SVC_Handler(void)
{
	asm("ldr r3, =curr_tcb   \n"   //r3 <- &curr_tcb
            "ldr r1, [r3]        \n"   //r1 <- curr_tcb
            "ldr r0, [r1]        \n"   //r0 <- curr_tcb->top_of_stack
            "ldmia r0!, {r4-r11} \n"   //load r4-r11 from the stack pointed by the r0
            "msr psp, r0         \n"   //psp <- r0
            "mov r0, #0          \n"   //r0 <- #0
            "msr basepri, r0     \n"   //basepri <- #0 (disable all interrupts)
            "orr r14, #0xd       \n"   //logic or the r14 with 0xd (EXC_RETURN)
            "bx r14              \n"); //trigger EXC_RETURN and jump to the address by pc
}

void task_yield(void)
{
	/* set NVIC PENDSV set bit */
	*((volatile uint32_t *)0xe000ed04) = (1UL << 28UL);
}

void task_delay(uint32_t ticks_to_delay)
{
	curr_tcb->ticks_to_delay = ticks_to_delay;
	curr_tcb->status = TASK_WAIT;
	task_yield();
}

#define CONTEXT_SWITCH() \
	asm(/* save context of the current task */ \
            "mrs r0, psp          \n"    /*r0 <- psp */ \
            "ldr r3, =curr_tcb    \n"    /*r3 <- &curr_tcb */ \
            "ldr r2, [r3]         \n"    /*r2 <- curr_tcb */ \
            "stmdb r0!, {r4-r11}  \n"    /*store r4-r11 to the stack pointed by the r0 */ \
            "str r0, [r2]         \n"    /*curr_tcb->top_of_stack (r2) <- r0 (update stack pointer) */ \
            "stmdb sp!, {r3, r14} \n"    /*temporarily store r3 and r14 to the msp */ \
            "mov r0, %0           \n"    /*set masked interrupt priority */ \
            "msr basepri, r0      \n"    /*interrupt with priority less than r0 will be disable */ \
            /* select tcb of the next task */ \
            "bl select_task       \n"    /*jump to the kernel space */ \
            /* switch the context to the next task */ \
            "mov r0, #0           \n"    /*r0 <- #0 */ \
            "msr basepri, r0      \n"    /*enable the interrupts */ \
            "ldmia sp!, {r3, r14} \n"    /*reload r3 and r14 from the msp */ \
            "ldr r1, [r3]         \n"    /*r1 <- curr_tcb */ \
            "ldr r0, [r1]         \n"    /*r0 <- curr_tcb->top_of_stack */ \
            "ldmia r0!, {r4-r11}  \n"    /*load r4-r11 from the stack pointed by the r0 */ \
            "msr psp, r0          \n"    /*replace psp to the stack of the next tcb (r0) */ \
            "bx r14               \n"    /*trigger EXC_RETURN (0xfffffffd) and jump to the address by the pc */ \
            "nop                  \n" :: /*instruction pipe synchronization */ \
            "i"(SYSCALL_INT_PRIORITY_MAX))

__attribute__((naked)) void SysTick_Handler(void)
{
	CONTEXT_SWITCH();
}
__attribute__((naked)) void PendSV_Handler(void)
{
	CONTEXT_SWITCH();
}
