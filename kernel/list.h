#ifndef __LIST_H__
#define __LIST_H__

#include <stddef.h>

#define container_of(ptr, type, member) \
        ((type *)((void *)ptr - offsetof(type, member)))

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define list_for_each(curr, list) \
	for ((curr) = (list)->next; (curr) != (list); (curr) = (curr)->next)

struct list {
    struct list *prev;
    struct list *next;
};

void list_init(struct list *list);
int list_is_empty(struct list *list);
void list_remove(struct list *list);
void list_push(struct list *list, struct list *new);
struct list *list_pop(struct list *list);

#endif
