#include <stddef.h>
#include "stm32f4xx.h"
#include "task.h"
#include "config_rtos.h"

#define INITIAL_XPSR (0x01000000)

int task_cnt;
tcb_t *task_list;
tcb_t *curr_tcb;

void task_exit_error(void)
{
	/* should never be happened */
	while(1);
}

void task_register(task_function_t task_func,
                   const char *task_name,
                   uint32_t stack_depth,
                   void * const task_params,
                   tcb_t *new_tcb)
{
	/* task number is less than the limit */
	if(task_cnt >= TASK_NUM_MAX) {
		return;
	}
	task_cnt++;

	/* no task in the list */
	if(task_list == NULL) {
		task_list = new_tcb;
		task_list->next = new_tcb;
		task_list->last = new_tcb;
	} else {
		/* append new task at the end of the list */
		new_tcb->next = task_list;
		new_tcb->last = task_list->last;
		task_list->last->next = new_tcb;
		task_list->last = new_tcb;
	}

	stack_depth = TASK_STACK_SIZE; //XXX: fixed size for now

	/* initialize task name */
	int i;
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
	curr_tcb = curr_tcb->next;
}

void os_start(void)
{
	while(task_cnt == 0);

	/* initialize first task to launch */
	curr_tcb = task_list;

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
