#include <stddef.h>
#include <stdbool.h>
#include "stm32f4xx.h"
#include "kernel.h"
#include "syscall.h"
#include "os_config.h"

#define HANDLER_MSP  0xFFFFFFF1
#define THREAD_MSP   0xFFFFFFF9
#define THREAD_PSP   0xFFFFFFFD

#define INITIAL_XPSR 0x01000000

void sys_fork(void);
void sys_sleep(void);
void sys_open(void);
void sys_close(void);
void sys_read(void);
void sys_write(void);
void sys_getpriority(void);
void sys_setpriority(void);
void sys_getpid(void);
void sys_mkdir(void);
void sys_rmdir(void);

tcb_t tasks[TASK_NUM_MAX];
int task_nums = 0;

tcb_t *curr_task = NULL;
int curr_task_index = 0;

syscall_info_t syscall_table[] = {
	DEF_SYSCALL(fork, 1),
	DEF_SYSCALL(sleep, 2),
	DEF_SYSCALL(open, 3),
	DEF_SYSCALL(close, 4),
	DEF_SYSCALL(read, 5),
	DEF_SYSCALL(write, 6),
	DEF_SYSCALL(getpriority, 7),
	DEF_SYSCALL(setpriority, 8),
	DEF_SYSCALL(getpid, 9),
	DEF_SYSCALL(mkdir, 10),
	DEF_SYSCALL(rmdir, 11)
};

void task_idle(void *param)
{
	while(1);
}

void task_create(task_function_t task_func, uint32_t priority)
{
	if(task_nums > TASK_NUM_MAX) {
		return;
	}

	tasks[task_nums].status = TASK_READY;
	tasks[task_nums].priority = priority;

	uint32_t stack_depth = TASK_STACK_SIZE; //TODO: variable size?

	/* stack design contains three parts:
	 * xpsr, pc, lr, r12, r3, r2, r1, r0, (for setting exception return),
	 * _r7 (for passing system call number), and
	 * _lr, r11, r10, r9, r8, r7, r6, r5, r4 (for context switch) */
	uint32_t *stack_top = tasks[task_nums].stack + stack_depth - 18;
	stack_top[17] = INITIAL_XPSR;
	stack_top[16] = (uint32_t)task_func; // lr = task_entry
	stack_top[8]  = THREAD_PSP;          //_lr = 0xfffffffd
	tasks[task_nums].stack_top = (user_stack_t *)stack_top;

	task_nums++;
}

void schedule(void)
{
	int i;

	/* update sleep timers */
	for(i = 0; i < task_nums; i++) {
		if(tasks[i].ticks_to_delay > 0) {
			tasks[i].ticks_to_delay--;

			/* task is ready */
			if(tasks[i].ticks_to_delay == 0) {
				tasks[i].status = TASK_READY;
			}
		}
	}

	/* change status of the current task */
	if(curr_task->status == TASK_RUNNING) {
		curr_task->status = TASK_READY;
	}

	/* round robin */
	while(1) {
		curr_task_index++;
		if(curr_task_index >= task_nums) {
			curr_task_index = 0;
		}

		if(tasks[curr_task_index].status == TASK_READY) {
			break;
		}
	}

	/* assign the next task to be executed */
	curr_task = &tasks[curr_task_index];
	curr_task->status = TASK_RUNNING;
}

void sys_fork(void)
{
}

void sys_sleep(void)
{
	curr_task->ticks_to_delay = curr_task->stack_top->r0;
	curr_task->status = TASK_WAIT;

	curr_task->stack_top->_r7 = 0;
	curr_task->stack_top->r0 = 0; //retval of the syscall
}

void sys_open(void)
{
}

void sys_close(void)
{
}

void sys_read(void)
{
}

void sys_write(void)
{
}

void sys_getpriority(void)
{
}

void sys_setpriority(void)
{
}

void sys_getpid(void)
{
}

void sys_mkdir(void)
{
}

void sys_rmdir(void)
{
}

void os_start(void)
{
	uint32_t stack_empty[32]; //a dummy stack for os enviromnent initialization
	os_env_init((uint32_t)(stack_empty + 32) /* point to the top */);

	/* create a idle task that do nothing */
	task_create(task_idle, 0);

	/* initialize systick timer */
	SysTick_Config(SystemCoreClock / OS_TICK_FREQUENCY);

	int syscall_table_size = sizeof(syscall_table) / sizeof(syscall_info_t);

	int i;
	while(1) {
		schedule();

		curr_task->stack_top = (user_stack_t *)jump_to_user_space((uint32_t)curr_task->stack_top);

		/* system call handler */
		for(i = 0; i < syscall_table_size; i++) {
			if(curr_task->stack_top->_r7 == syscall_table[i].num) {
				syscall_table[i].syscall_handler();
				break;
			}
		}
	}
}
