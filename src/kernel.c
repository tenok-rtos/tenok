#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f4xx.h"
#include "kernel.h"
#include "syscall.h"
#include "os_config.h"
#include "list.h"

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

list_t ready_list[TASK_MAX_PRIORITY+1];

tcb_t tasks[TASK_NUM_MAX];
int task_nums = 0;

tcb_t *curr_task = NULL;

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

void task_create(task_function_t task_func, uint8_t priority)
{
	if(task_nums > TASK_NUM_MAX) {
		return;
	}

	tasks[task_nums].pid = task_nums;
	tasks[task_nums].status = TASK_WAIT;
	tasks[task_nums].priority = priority;
	tasks[task_nums].stack_size = TASK_STACK_SIZE; //TODO: variable size?

	/* stack design contains three parts:
	 * xpsr, pc, lr, r12, r3, r2, r1, r0, (for setting exception return),
	 * _r7 (for passing system call number), and
	 * _lr, r11, r10, r9, r8, r7, r6, r5, r4 (for context switch) */
	uint32_t *stack_top = tasks[task_nums].stack + tasks[task_nums].stack_size - 18;
	stack_top[17] = INITIAL_XPSR;
	stack_top[16] = (uint32_t)task_func; // lr = task_entry
	stack_top[8]  = THREAD_PSP;          //_lr = 0xfffffffd
	tasks[task_nums].stack_top = (user_stack_t *)stack_top;

	task_nums++;
}

void schedule(void)
{
	/* update sleep timer */
	int i;
	for(i = 0; i < task_nums; i++) {
		if(tasks[i].status == TASK_WAIT) {
			/* update remained ticks */
			if(tasks[i].remained_ticks > 0) {
				tasks[i].remained_ticks--;
			}

			/* task is ready */
			if(tasks[i].remained_ticks == 0) {
				tasks[i].status = TASK_READY;

				/* push the task into the ready list with respect to its priority */
				list_push(&ready_list[tasks[i].priority], &tasks[i].list);
			}
		}
	}

	/* freeze the current task */
	curr_task->status = TASK_WAIT;

	/* find a ready list that contains runnable tasks */
	int pri;
	for(pri = TASK_MAX_PRIORITY; pri >= 0; pri--) {
		if(list_is_empty(&ready_list[pri]) == false) {
			break;
		}
	}

	/* select a task from the ready list */
	list_t *next = list_pop(&ready_list[pri]);
	curr_task = container_of(next, tcb_t, list);
	curr_task->status = TASK_RUNNING;
}

void sys_fork(void)
{
	if(task_nums > TASK_NUM_MAX) {
		curr_task->stack_top->r0 = -1; //set failed retval
	}

	/* calculate the used space of the parent task's stack */
	uint32_t *parent_stack_end = curr_task->stack + curr_task->stack_size;
	uint32_t stack_used = parent_stack_end - (uint32_t *)curr_task->stack_top;

	tasks[task_nums].pid = task_nums;
	tasks[task_nums].status = TASK_WAIT;
	tasks[task_nums].priority = curr_task->priority;
	tasks[task_nums].stack_size = curr_task->stack_size;
	tasks[task_nums].stack_top = (user_stack_t *)(tasks[task_nums].stack + tasks[task_nums].stack_size - stack_used);

	/* copy the stack of the used part only */
	memcpy(tasks[task_nums].stack_top, curr_task->stack_top, sizeof(uint32_t)*stack_used);

	/* set retval */
	curr_task->stack_top->r0 = task_nums;            //return to child pid the parent task
	tasks[task_nums].stack_top->r0 = curr_task->pid; //return to 0 the child task

	task_nums++;
}

void sys_sleep(void)
{
	/* setup the delay timer and change the task status */
	curr_task->remained_ticks = curr_task->stack_top->r0;
	curr_task->status = TASK_WAIT;

	/* set retval */
	curr_task->stack_top->r0 = 0;
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
	/* return the task priority with r0 */
	curr_task->stack_top->r0 = curr_task->priority;
}

void sys_setpriority(void)
{
	int which = curr_task->stack_top->r0;
	int who = curr_task->stack_top->r1;
	int priority = curr_task->stack_top->r2;

	/* unsupported `which' type */
	if(which != PRIO_PROCESS) {
		curr_task->stack_top->r0 = -1; //set retval to -1
		return;
	}

	/* compare pid and set new priority */
	int i;
	for(i = 0; i < task_nums; i++) {
		if(tasks[i].pid == who) {
			tasks[i].priority = priority;
			curr_task->stack_top->r0 = 0;  //set retval to 0
			return;
		}
	}

	/* process not found */
	curr_task->stack_top->r0 = -1; //set retval to -1
}

void sys_getpid(void)
{
	/* return the task pid with r0 */
	curr_task->stack_top->r0 = curr_task->pid;
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

	int i;

	/* initialize task ready lists */
	for(i = 0; i <= TASK_MAX_PRIORITY; i++) {
		list_init(&ready_list[i]);
	}

	/* create a idle task that do nothing */
	task_create(task_idle, 0);

	/* initialize systick timer */
	SysTick_Config(SystemCoreClock / OS_TICK_FREQUENCY);

	int syscall_table_size = sizeof(syscall_table) / sizeof(syscall_info_t);

	while(1) {
		schedule();

		curr_task->stack_top = (user_stack_t *)jump_to_user_space((uint32_t)curr_task->stack_top);

		/* system call handler */
		for(i = 0; i < syscall_table_size; i++) {
			if(curr_task->stack_top->_r7 == syscall_table[i].num) {
				/* execute the syscall service */
				syscall_table[i].syscall_handler();

				/* clear the syscall number */
				curr_task->stack_top->_r7 = 0;

				break;
			}
		}
	}
}
