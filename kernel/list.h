#ifndef __LIST_H__
#define __LIST_H__

#include <stddef.h>
#include "util.h"

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

typedef struct list {
	struct list *last;
	struct list *next;
} list_t;

void list_init(list_t *list);
int list_is_empty(list_t *list);
void list_remove(list_t *list);
void list_push(list_t *list, list_t *new);
list_t* list_pop(list_t *list);

#endif
