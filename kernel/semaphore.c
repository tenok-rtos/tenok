#include <stddef.h>
#include "kernel.h"
#include "list.h"
#include "semaphore.h"
#include "syscall.h"

extern tcb_t *running_task;

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
	sem->count = value;
	sem->lock = 0;
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
	return os_sem_wait(sem);
}
