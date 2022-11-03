#ifndef __PORTING_H__
#define __PORTING_H__

#define irq_disable() \
	asm("cpsid i");

#define irq_enable() \
	asm("cpsie i");

#endif
