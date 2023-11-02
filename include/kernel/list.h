#ifndef __LIST_H__
#define __LIST_H__

#include <stddef.h>

#define container_of(ptr, type, member) \
        ((type *)((void *)ptr - offsetof(type, member)))

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) \
        list_entry((ptr)->next, type, member)

#define list_prev_entry(curr, member) \
        list_entry((curr)->member.prev, typeof(*(curr)), member)

#define list_next_entry(curr, member) \
        list_entry((curr)->member.next, typeof(*(curr)), member)

#define list_entry_is_head(pos, head, member) \
        (&pos->member == (head))

#define list_for_each(curr, head) \
	for ((curr) = (head)->next; (curr) != (head); (curr) = (curr)->next)

#define list_for_each_safe(curr, _next, head) \
        for(curr = (head)->next, _next = (curr)->next; \
            (curr) != (head); \
            (curr) = _next, _next = (curr)->next)

#define list_for_each_entry(curr, head, member) \
        for(curr = list_first_entry(head, __typeof__(*curr), member); \
            &curr->member != (head); curr = list_next_entry(curr, member))

#define LIST_HEAD_INIT(name) \
    {.prev = (&name), .next = (&name)}

#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

struct list_head {
    struct list_head *next, *prev;
};

void INIT_LIST_HEAD(struct list_head *list);
int list_empty(struct list_head *head);
int list_is_last(const struct list_head *list, const struct list_head *head);
void list_add(struct list_head *new, struct list_head *head);
void list_del(struct list_head *entry);
void list_del_init(struct list_head *entry);
void list_move(struct list_head *list, struct list_head *new_head);

#endif
