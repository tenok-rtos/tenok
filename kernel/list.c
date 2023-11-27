#include <list.h>

void INIT_LIST_HEAD(struct list_head *list)
{
    list->prev = list;
    list->next = list;
}

int list_empty(struct list_head *head)
{
    return head->next == head;
}

int list_is_last(const struct list_head *list, const struct list_head *head)
{
    return list->next == head;
}

void list_del(struct list_head *entry)
{
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
}

void list_del_init(struct list_head *entry)
{
    list_del(entry);
    INIT_LIST_HEAD(entry);
}

void list_add(struct list_head *new, struct list_head *head)
{
    new->prev = head->prev;
    new->next = head;
    head->prev->next = new;
    head->prev = new;
}

void list_move(struct list_head *list, struct list_head *new_head)
{
    list_del(list);
    list_add(list, new_head);
}
