#include <stddef.h>
#include "kernel.h"
#include "list.h"
#include "mutex.h"
#include "syscall.h"

extern tcb_t *running_task;

int pthread_mutex_init(_pthread_mutex_t *mutex, const pthread_mutex_attr_t *attr)
{
	mutex->owner = NULL;
	list_init(&mutex->wait_list);

	return 0;
}

int pthread_mutex_unlock(_pthread_mutex_t *mutex)
{
	spin_lock_irq(&mutex->lock);

	int retval = 0;

	/* check mutex owner */
	if(mutex->owner == running_task) {
		/* release the mutex */
		mutex->owner = NULL;

		/* check the mutex waiting list */
		if(!list_is_empty(&mutex->wait_list)) {
			/* wake up a task from the mutex waiting list */
			wake_up(&mutex->wait_list);
		}
	} else {
		retval = EPERM;
	}

	spin_unlock_irq(&mutex->lock);

	return retval;
}

int pthread_mutex_lock(_pthread_mutex_t *mutex)
{
	spin_lock_irq(&mutex->lock);

	if(mutex->owner != NULL) {
		/* put the current task into the mutex waiting list */
		prepare_to_wait(&mutex->wait_list, &running_task->list, TASK_WAIT);

		spin_unlock_irq(&mutex->lock);

		sched_yield(); //yield the cpu and back to the kernel
	} else {
		/* set new owner of the mutex */
		mutex->owner = running_task;

		spin_unlock_irq(&mutex->lock);
	}

	return 0;
}
