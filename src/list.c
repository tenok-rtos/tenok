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

void list_remove(list_t *list)
{
	if(list != NULL) {
		list->next->last = list->last;
		list->last->next = list->next;
	}
}

void list_push(list_t *list, list_t *new)
{
	if(list != NULL && new != NULL) {
		/* remove new from the original list */
		new->last->next = new->next;
		new->next->last = new->last;

		/* connect new into the list */
		list->last->next = new;
		new->last = list->last;
		new->next = list;
		list->last = new;
	}
}
