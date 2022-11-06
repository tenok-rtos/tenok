#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f4xx.h"
#include "kernel.h"
#include "syscall.h"
#include "kconfig.h"
#include "list.h"
#include "mpool.h"
#include "file.h"
#include "fifo.h"

#define HANDLER_MSP  0xFFFFFFF1
#define THREAD_MSP   0xFFFFFFF9
#define THREAD_PSP   0xFFFFFFFD

#define INITIAL_XPSR 0x01000000

void sys_yield(void);
void sys_set_irq(void);
void sys_fork(void);
void sys_sleep(void);
void sys_open(void);
void sys_close(void);
void sys_read(void);
void sys_write(void);
void sys_getpriority(void);
void sys_setpriority(void);
void sys_getpid(void);
void sys_mknod(void);
void sys_mkdir(void);
void sys_rmdir(void);
void sys_sem_init(void);
void sys_sem_post(void);
void sys_sem_wait(void);

/* task lists */
list_t ready_list[TASK_MAX_PRIORITY+1];
list_t sleep_list;

/* tasks */
tcb_t tasks[TASK_NUM_MAX];
tcb_t *running_task = NULL;
int task_nums = 0;

/* memory pool */
struct memory_pool mem_pool;

/* files */
struct file *files[FILE_CNT_LIMIT];
int file_count = 0;

/* system call table */
syscall_info_t syscall_table[] = {
	/* non-posix syscall: */
	DEF_SYSCALL(yield, 0),
	DEF_SYSCALL(set_irq, 1),
	/* posix syscall: */
	DEF_SYSCALL(fork, 2),
	DEF_SYSCALL(sleep, 3),
	DEF_SYSCALL(open, 4),
	DEF_SYSCALL(close, 5),
	DEF_SYSCALL(read, 6),
	DEF_SYSCALL(write, 7),
	DEF_SYSCALL(getpriority, 8),
	DEF_SYSCALL(setpriority, 9),
	DEF_SYSCALL(getpid, 10),
	DEF_SYSCALL(mknod, 11),
	DEF_SYSCALL(mkdir, 12),
	DEF_SYSCALL(rmdir, 13),
};

void task_create(task_func_t task_func, uint8_t priority)
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
	list_t *list_itr;
	tcb_t  *task;

	/* update sleep timers */
	list_itr = sleep_list.next;
	while(list_itr != &sleep_list) {
		list_t *next = list_itr->next;
		task = list_entry(list_itr, tcb_t, list);

		if(task->remained_ticks > 0) {
			/* update the remained ticks */
			task->remained_ticks--;
		} else if(task->remained_ticks == 0) {
			/* task is ready, push it into the ready list according to its priority */
			task->status = TASK_READY;
			list_remove(list_itr); //remove the task from the sleep list
			list_push(&ready_list[task->priority], list_itr);
		}

		list_itr = next;
	}

	/* freeze the current task */
	if(running_task->status == TASK_RUNNING) {
		/* syscall may change the task status and put it into a list (e.g., semaphore),
		 * if not, place it into the sleep list and change the status */
		running_task->status = TASK_WAIT;
		list_push(&sleep_list, &running_task->list);
	}

	/* find a ready list that contains runnable tasks */
	int pri;
	for(pri = TASK_MAX_PRIORITY; pri >= 0; pri--) {
		if(list_is_empty(&ready_list[pri]) == false) {
			break;
		}
	}

	/* select a task from the ready list */
	list_t *next = list_pop(&ready_list[pri]);
	running_task = list_entry(next, tcb_t, list);
	running_task->status = TASK_RUNNING;
}

void sys_yield(void)
{
	return;
}

void sys_set_irq(void)
{
	uint32_t state = running_task->stack_top->r0;
	if(state) {
		asm volatile ("cpsie i"); //enable all irq
	} else {
		asm volatile ("cpsid i"); //disable all irq
	}
}

void sys_fork(void)
{
	if(task_nums > TASK_NUM_MAX) {
		running_task->stack_top->r0 = -1; //set failed retval
	}

	/* calculate the used space of the parent task's stack */
	uint32_t *parent_stack_end = running_task->stack + running_task->stack_size;
	uint32_t stack_used = parent_stack_end - (uint32_t *)running_task->stack_top;

	tasks[task_nums].pid = task_nums;
	tasks[task_nums].priority = running_task->priority;
	tasks[task_nums].stack_size = running_task->stack_size;
	tasks[task_nums].stack_top = (user_stack_t *)(tasks[task_nums].stack + tasks[task_nums].stack_size - stack_used);

	/* put the forked task into the sleep list */
	tasks[task_nums].status = TASK_WAIT;
	list_push(&sleep_list, &tasks[task_nums].list);

	/* copy the stack of the used part only */
	memcpy(tasks[task_nums].stack_top, running_task->stack_top, sizeof(uint32_t)*stack_used);

	/* set retval */
	running_task->stack_top->r0 = task_nums;            //return to child pid the parent task
	tasks[task_nums].stack_top->r0 = running_task->pid; //return to 0 the child task

	task_nums++;
}

void sys_sleep(void)
{
	/* reconfigure the timer for sleeping */
	running_task->remained_ticks = running_task->stack_top->r0;

	/* put the task into the sleep list and change the status */
	running_task->status = TASK_WAIT;
	list_push(&sleep_list, &(running_task->list));

	/* set retval */
	running_task->stack_top->r0 = 0;
}

void sys_open(void)
{
}

void sys_close(void)
{
}

void sys_read(void)
{
	int fd = running_task->stack_top->r0;
	uint8_t *buf = (uint8_t *)running_task->stack_top->r1;
	size_t count = running_task->stack_top->r2;

	struct file *filp = files[fd];
	ssize_t retval = filp->f_op->read(filp, buf, count, 0);

	running_task->stack_top->r0 = retval; //pass return value
}

void sys_write(void)
{
	int fd = running_task->stack_top->r0;
	uint8_t *buf = (uint8_t *)running_task->stack_top->r1;
	size_t count = running_task->stack_top->r2;

	struct file *filp = files[fd];
	ssize_t retval = filp->f_op->write(filp, buf, count, 0);

	running_task->stack_top->r0 = retval; //pass return value
}

void sys_getpriority(void)
{
	/* return the task priority with r0 */
	running_task->stack_top->r0 = running_task->priority;
}

void sys_setpriority(void)
{
	int which = running_task->stack_top->r0;
	int who = running_task->stack_top->r1;
	int priority = running_task->stack_top->r2;

	/* unsupported `which' type */
	if(which != PRIO_PROCESS) {
		running_task->stack_top->r0 = -1; //set retval to -1
		return;
	}

	/* compare pid and set new priority */
	int i;
	for(i = 0; i < task_nums; i++) {
		if(tasks[i].pid == who) {
			tasks[i].priority = priority;
			running_task->stack_top->r0 = 0;  //set retval to 0
			return;
		}
	}

	/* process not found */
	running_task->stack_top->r0 = -1; //set retval to -1
}

void sys_getpid(void)
{
	/* return the task pid with r0 */
	running_task->stack_top->r0 = running_task->pid;
}

void sys_mknod(void)
{
	char *pathname = (char *)running_task->stack_top->r0;
	//_mode_t mode = (_mode_t)running_task->stack_top->r1;
	_dev_t dev = (_dev_t)running_task->stack_top->r2;

	/* if file count reached the limit */
	if(file_count >= FILE_CNT_LIMIT) {
		running_task->stack_top->r0 = -1;
		return;
	}

	int result;

	/* create new file according to its type */
	switch(dev) {
	case S_IFIFO:
		result = fifo_init(file_count, (struct file **)&files, &mem_pool);
		break;
	case S_IFCHR:
		result = -1;
		break;
	case S_IFBLK:
		result = -1;
		break;
	case S_IFREG:
		result = -1;
		break;
	default:
		result = -1;
		return;
	}

	/* set the return value */
	if(result == 0) {
		running_task->stack_top->r0 = 0;
	} else {
		running_task->stack_top->r0 = -1;
	}
}

void sys_mkdir(void)
{
}

void sys_rmdir(void)
{
}

uint32_t get_proc_mode(void)
{
	/* get the 9 bits isr number from the ipsr register,
	 * check "exception types" of the ARM CM3 for details */
	volatile unsigned int mode;
	asm volatile ("push {r0}     \n"
	              "mrs  r0, ipsr \n"
	              "str  r0, [%0] \n"
	              "pop  {r0}     \n"
	              :: "r"(&mode));
	return mode & 0x1ff; //return ipsr[8:0]
}

void preempt_disable(void)
{
	if(get_proc_mode() == 0) {
		/* unprivileged thread mode (assumed to be called in the user program) */
		set_irq(0);
	} else {
		/* handler mode (i.e., interrupt, which is always privileged) */
		asm volatile ("cpsid i");
	}
}

void preempt_enable(void)
{
	if(get_proc_mode() == 0) {
		/* unprivileged thread mode (assumed to be called in the user program) */
		set_irq(1);
	} else {
		/* handler mode (i.e., interrupt, which is always privileged) */
		asm volatile ("cpsie i");
	}
}

void os_start(task_func_t first_task)
{
	uint32_t stack_empty[32]; //a dummy stack for os enviromnent initialization
	os_env_init((uint32_t)(stack_empty + 32) /* point to the top */);

	/* initialize the memory pool */
	uint8_t mem_pool_buf[MEM_POOL_SIZE];
	memory_pool_init(&mem_pool, mem_pool_buf, MEM_POOL_SIZE);

	/* initialize task ready lists */
	int i;
	for(i = 0; i <= TASK_MAX_PRIORITY; i++) {
		list_init(&ready_list[i]);
	}

	/* initialize sleep list */
	list_init(&sleep_list);

	task_create(first_task, 0);

	/* initialize systick timer */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	SysTick_Config(SystemCoreClock / OS_TICK_FREQUENCY);

	int syscall_table_size = sizeof(syscall_table) / sizeof(syscall_info_t);

	/* launch the first task */
	running_task = &tasks[0];
	tasks[0].status = TASK_RUNNING;

	while(1) {
		/* execute the task if no pending syscall service */
		if(running_task->syscall_pending == false) {
			running_task->stack_top =
			        (user_stack_t *)jump_to_user_space((uint32_t)running_task->stack_top);
		}

		/* system call handler */
		for(i = 0; i < syscall_table_size; i++) {
			if(running_task->stack_top->_r7 == syscall_table[i].num) {
				/* execute the syscall service */
				syscall_table[i].syscall_handler();

				/* clear the syscall number if the service is complete */
				if(running_task->syscall_pending == false) {
					running_task->stack_top->_r7 = 0;
				}

				break;
			}
		}

		schedule();
	}
}
