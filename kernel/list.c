#include <stddef.h>
#include "list.h"

void list_init(struct list *list)
{
    if(list != NULL) {
        list->prev = list;
        list->next = list;
    }
}

int list_is_empty(struct list *list)
{
    return list->next == list;
}

void list_remove(struct list *list)
{
    if(list != NULL) {
        list->next->prev = list->prev;
        list->prev->next = list->next;
    }
}

void list_push(struct list *list, struct list *new)
{
    if(list != NULL && new != NULL) {
        /* connect new into the list */
        new->prev = list->prev;
        new->next = list;
        list->prev->next = new;
        list->prev = new;
    }
}

struct list *list_pop(struct list *list)
{
    //the second position of the list stores the first item
    struct list *first = list->next;

    if(list == first) {
        return NULL;
    }

    list->next = first->next;
    list->next->prev = list;

    return first;
}
