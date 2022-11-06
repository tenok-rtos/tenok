#include <stddef.h>
#include "kernel.h"
#include "list.h"
#include "semaphore.h"
#include "syscall.h"

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
	if(sem->count >= 0 && !list_is_empty(&sem->wait_list)) {
		/* wake up a task from the semaphore waiting list */
		wake_up(&sem->wait_list);
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
		prepare_to_wait(&sem->wait_list, &running_task->list, TASK_WAIT);

		spin_unlock_irq(&sem->lock);

		yield();
	} else {
		spin_unlock_irq(&sem->lock);
	}

	return 0;
}
