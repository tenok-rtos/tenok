#ifndef __LIST_H__
#define __LIST_H__

#include <stddef.h>

#define container_of(ptr, type, member) \
        ((type *)((void *)ptr - offsetof(type, member)))

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) \
        list_entry((ptr)->next, type, member)

#define list_for_each(curr, head) \
	for ((curr) = (head)->next; (curr) != (head); (curr) = (curr)->next)

#define list_for_each_safe(curr, _next, head) \
        for(curr = (head)->next, _next = (curr)->next; \
            (curr) != (head); \
            (curr) = _next, _next = (curr)->next)

#define LIST_HEAD_INIT(name) \
    {.prev = (&name), .next = (&name)}

#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

struct list_head {
    struct list_head *next, *prev;
};

void INIT_LIST_HEAD(struct list_head *list);
int list_empty(struct list_head *head);
void list_del(struct list_head *entry);
void list_push(struct list_head *list, struct list_head *new);
struct list_head *list_pop(struct list_head *list);
void list_move(struct list_head *list, struct list_head *new_head);

#endif
