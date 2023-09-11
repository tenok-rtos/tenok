#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include <stdint.h>

#define spin_lock_irq(lock) \
	do { \
	preempt_disable(); \
	spin_lock(lock); \
	} while(0)

#define spin_unlock_irq(lock) \
	do { \
	spin_unlock(lock); \
	preempt_enable(); \
	} while(0)

typedef uint32_t spinlock_t;

void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);

#endif
