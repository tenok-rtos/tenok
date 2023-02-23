#include <stddef.h>
#include "list.h"

void list_init(list_t *list)
{
    if(list != NULL) {
        list->last = list;
        list->next = list;
    }
}

int list_is_empty(list_t *list)
{
    return list->next == list;
}

void list_remove(struct list *list)
{
    if(list != NULL) {
        list->next->last = list->last;
        list->last->next = list->next;
    }
}

void list_push(list_t *list, list_t *new)
{
    if(list != NULL && new != NULL) {
        /* connect new into the list */
        new->last = list->last;
        new->next = list;
        list->last->next = new;
        list->last = new;
    }
}

list_t* list_pop(list_t *list)
{
    //the second position of the list stores the first item
    list_t *first = list->next;

    if(list == first) {
        return NULL;
    }

    list->next = first->next;
    list->next->last = list;

    return first;
}
