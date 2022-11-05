#include <stddef.h>
#include "kernel.h"
#include "list.h"
#include "semaphore.h"
#include "porting.h"
#include "syscall.h"

extern list_t ready_list[TASK_MAX_PRIORITY+1];
extern tcb_t tasks[TASK_NUM_MAX];
extern tcb_t *running_task;

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
	sem->count = value;
	list_init(&sem->wait_list);

	return 0;
}

int sem_post(sem_t *sem)
{
	spin_lock_irq(&sem->lock);

	sem->count++;
	if(sem->count >= 0) {
		/* pop one task from the semaphore waiting list */
		list_t *waken_task_list = list_pop(&sem->wait_list);
		tcb_t *waken_task = list_entry(waken_task_list, tcb_t, list);

		/* put the task into the ready list */
		waken_task->status = TASK_READY;
		list_push(&ready_list[waken_task->priority], &waken_task->list);
	}

	spin_unlock_irq(&sem->lock);

	return 0;
}

int sem_wait(sem_t *sem)
{
	spin_lock_irq(&sem->lock);

	sem->count--;
	if(sem->count < 0) {
		/* put the current task into the semaphore waiting list */
		running_task->status = TASK_WAIT;
		list_push(&sem->wait_list, &running_task->list);

		spin_unlock_irq(&sem->lock);

		yield();
	} else {
		spin_unlock_irq(&sem->lock);
	}

	return 0;
}
