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

tcb_t *tasks[OS_MAX_PRIORITY] = {NULL};
int task_nums[OS_MAX_PRIORITY] = {0};

tcb_t *curr_tcb = NULL;

tcb_t tcb_idle_task;

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
                   tcb_t *new_task)
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

	/* append new task to the end of the list */
	if(tasks[priority] == NULL) {
		/* if the task list is empty */
		tasks[priority] = new_task;
		tasks[priority]->next = new_task;
		tasks[priority]->last = new_task;
	} else {
		/* append new task at the end of the list */
		new_task->next = tasks[priority];
		new_task->last = tasks[priority]->last;
		tasks[priority]->last->next = new_task;
		tasks[priority]->last = new_task;
	}
	task_nums[priority]++; //increase the task number

	stack_depth = TASK_STACK_SIZE; //TODO: variable size?
	new_task->status = TASK_READY;
	new_task->priority = priority;

	/* initialize task name */
	for(i = 0; i < TASK_NAME_LEN_MAX; i++) {
		new_task->task_name[i] = task_name[i];

		if(task_name[i] == '\0') {
			break;
		}
	}
	new_task->task_name[TASK_NAME_LEN_MAX - 1] = '\0';

	/*===========================*
	 * initialize the task stack *
	 *===========================*/

	/* stack design contains three parts:
	 * xpsr, pc, lr, r12, r3, r2, r1, r0, (for setting exception return),
         * _r7 (for passing system call number), and
	 * _lr, r11, r10, r9, r8, r7, r6, r5, r4 (for context switch) */

	uint32_t *stack_top = new_task->stack + stack_depth;
	stack_top -= 18;
	stack_top[17] = INITIAL_XPSR;
	stack_top[16] = (uint32_t)task_func; // lr = task_entry
	stack_top[8]  = THREAD_PSP;          //_lr = 0xfffffffd

	new_task->stack_top = (user_stack_t *)stack_top;
}

void schedule(void)
{
	int i, j;
	tcb_t *_task;

	bool change_task = false;   //flag that indicates the scheduler should select next task
	bool task_selected = false; //next task to be executed is decieded

	/* if current task is the idle task or it is no longer in running state then it should
	 * yield the cpu to other tasks */
	if(curr_tcb->status != TASK_RUNNING || curr_tcb == &tcb_idle_task) {
		change_task = true;
	}

	/* find the next ready task with the highest priority */
	for(i = OS_MAX_PRIORITY - 1; i >= 0; i--) {
		/* current task with prioity number i is empty */
		if(tasks[i] == NULL) {
			continue; //skip
		}

		/* initialize the task iterator */
		_task = tasks[i];

		/* iterate through the task list */
		for(j = 0; j < task_nums[i]; j++) {
			if((_task->priority > curr_tcb->priority || change_task == true) &&
			    _task != curr_tcb &&
			    _task->status == TASK_READY &&
			    task_selected == false) {
				//change status of the current task
				if(curr_tcb->status == TASK_RUNNING) {
					curr_tcb->status = TASK_READY;
				}

				//next task to run is selected
				task_selected = true;
				curr_tcb = _task;
				curr_tcb->status = TASK_RUNNING;
			}

			/* update the remained delay ticks */
			if(_task->ticks_to_delay > 0) {
				_task->ticks_to_delay--;

				/* delay is finished, change the task state to
				 * be ready */
				if(_task->ticks_to_delay == 0) {
					_task->status = TASK_READY;
				}
			}

			/* update the task iterator */
			_task = _task->next;
		}
	}
}

void sys_fork(void)
{
}

void sys_sleep(void)
{
	curr_tcb->ticks_to_delay = curr_tcb->stack_top->r0;
	curr_tcb->status = TASK_WAIT;

	curr_tcb->stack_top->_r7 = 0;
	curr_tcb->stack_top->r0 = 0; //retval of the syscall
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

	int syscall_table_size = sizeof(syscall_table) / sizeof(syscall_info_t);

	while(1) {
		/* system call handler */
		for(i = 0; i < syscall_table_size; i++) {
			if(curr_tcb->stack_top->_r7 == syscall_table[i].num) {
				syscall_table[i].syscall_handler();
				break;
			}
		}

		schedule();

		curr_tcb->stack_top = (user_stack_t *)jump_to_user_space((uint32_t)curr_tcb->stack_top);
	}
}
