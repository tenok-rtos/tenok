#ifndef __LIST_H__
#define __LIST_H__

#include <stddef.h>

#define container_of(ptr, type, member) \
        ((type *)((void *)ptr - offsetof(type, member)))

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define list_for_each(curr, list) \
	for ((curr) = (list)->next; (curr) != (list); (curr) = (curr)->next)

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
