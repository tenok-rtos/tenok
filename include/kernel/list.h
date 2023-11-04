/**
 * @file
 */
#ifndef __LIST_H__
#define __LIST_H__

#include <stddef.h>

/**
 * @brief  Get the object entry by giving its member
 * @param  ptr: Pointer to the structure member.
 * @param  type: Type of the object.
 * @param  member: Name of structure member.
 * @retval &obj: Adress pointing to the object.
 */
#define container_of(ptr, type, member) \
        ((type *)((void *)ptr - offsetof(type, member)))

/**
 * @brief  Get the object entry
 * @param  ptr: Pointer to the object list item.
 * @param  type: Type of the object.
 * @param  member: Structure member name of the list item.
 * @retval &obj: Adress pointing to the object.
 */
#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

/**
 * @brief  Get the first object entry
 * @param  ptr: Head of the list.
 * @param  type: Type of the object.
 * @param  member: Structure member name of the list item.
 * @retval &obj: Adress pointing to the first object.
 */
#define list_first_entry(ptr, type, member) \
        list_entry((ptr)->next, type, member)

/**
 * @brief  Get the previous object entry
 * @param  pos: Current object entry position.
 * @param  member: Structure member name of the list item.
 * @retval &obj: Adress pointing to the previous object.
 */
#define list_prev_entry(pos, member) \
        list_entry((pos)->member.prev, typeof(*(pos)), member)

/**
 * @brief  Get the next object entry
 * @param  pos: Current object entry position.
 * @param  member: Structure member name of the list item.
 * @retval &obj: Adress pointing to the next object.
 */
#define list_next_entry(pos, member) \
        list_entry((pos)->member.next, typeof(*(pos)), member)

/**
 * @brief  Check if the list entry is the list head
 * @param  pos: Object entry to check.
 * @param  member: Structure member name of the list item.
 * @retval bool: true or false.
 */
#define list_entry_is_head(pos, head, member) \
        (&pos->member == (head))

/**
 * @brief  Iterate the list item through the whole list
 * @param  pos: Current list item position.
 * @param  head: Head of the list.
 * @retval None
 */
#define list_for_each(pos, head) \
	for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

/**
 * @brief  Iterate the list item through the whole list safely.
 * @param  pos: Current list item position.
 * @param  head: Head of the list.
 * @retval None
 */
#define list_for_each_safe(pos, _next, head) \
        for(pos = (head)->next, _next = (pos)->next; \
            (pos) != (head); \
            (pos) = _next, _next = (pos)->next)

/**
 * @brief  Iterate the object entry through the whole list
 * @param  pos: Current object entry position.
 * @param  head: Head of the list.
 * @param  member: Structure member name of the list item.
 * @retval None
 */
#define list_for_each_entry(pos, head, member) \
        for(pos = list_first_entry(head, __typeof__(*pos), member); \
            &pos->member != (head); pos = list_next_entry(pos, member))

/**
 * @brief  Initialize the list head statically
 * @param  name: List head to initialize.
 * @retval None
 */
#define LIST_HEAD_INIT(name) \
    {.prev = (&name), .next = (&name)}

/**
 * @brief  Declare and initialize a new list head
 * @param  name: Name of the new list head.
 * @retval None
 */
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

struct list_head {
    struct list_head *next, *prev;
};

/**
 * @brief  Initialize the list head
 * @param  list: List head to initialize.
 * @retval None
 */
void INIT_LIST_HEAD(struct list_head *list);

/**
 * @brief  Check if the list is empty
 * @param  head: Head of the list.
 * @retval int: 0 as false, else as true.
 */
int list_empty(struct list_head *head);

/**
 * @brief  Check if the list item is the last item in the list
 * @param  list: List item to check.
 * @param  head: Head of the list.
 * @retval int: 0 as false, else as true.
 */
int list_is_last(const struct list_head *list, const struct list_head *head);

/**
 * @brief  Add list item into the list.
 * @param  new: New list item to add.
 * @param  head: Head of the list.
 * @retval None
 */
void list_add(struct list_head *new, struct list_head *head);

/**
 * @brief  Delete a list item from the list.
 * @param  entry: The list item to delete.
 * @retval None
 */
void list_del(struct list_head *entry);

/**
 * @brief  Delete a list item from the list and reinitialize it.
 * @param  entry: The list item to delete.
 * @retval None
 */
void list_del_init(struct list_head *entry);

/**
 * @brief  Move list item from current list to another
 * @param  list: The list item to move.
 * @param  new_head: Head of the new list.
 * @retval None
 */
void list_move(struct list_head *list, struct list_head *new_head);

#endif
