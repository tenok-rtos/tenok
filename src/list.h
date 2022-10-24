#ifndef __LIST_H__
#define __LIST_H__

typedef struct list {
	struct list *last;
	struct list *next;
} list_t;

#endif
