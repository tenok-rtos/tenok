#include <stddef.h>

#include "list.h"

void INIT_LIST_HEAD(struct list_head *list)
{
    if(list != NULL) {
        list->prev = list;
        list->next = list;
    }
}

int list_empty(struct list_head *head)
{
    return head->next == head;
}

void list_del(struct list_head *entry)
{
    if(entry != NULL) {
        entry->next->prev = entry->prev;
        entry->prev->next = entry->next;
    }
}

void list_push(struct list_head *list, struct list_head *new)
{
    if(list != NULL && new != NULL) {
        /* connect new into the list */
        new->prev = list->prev;
        new->next = list;
        list->prev->next = new;
        list->prev = new;
    }
}

struct list_head *list_pop(struct list_head *list)
{
    //the second position of the list stores the first item
    struct list_head *first = list->next;

    if(list == first) {
        return NULL;
    }

    list->next = first->next;
    list->next->prev = list;

    return first;
}

void list_move(struct list_head *list, struct list_head *new_head)
{
    list_del(list);
    list_push(new_head, list);
}
