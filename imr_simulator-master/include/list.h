#pragma once

#include <stddef.h>

struct list_head
{
    struct list_head *next, *prev;
};

#ifdef CONFIG_DEBUG_LIST
extern bool __list_add_valid(struct list_head *new,
                             struct list_head *prev,
                             struct list_head *next);
extern bool __list_del_entry_valid(struct list_head *entry);
#else
static inline bool __list_add_valid(struct list_head *new,
                                    struct list_head *prev,
                                    struct list_head *next)
{
    return true;
}
static inline bool __list_del_entry_valid(struct list_head *entry)
{
    return true;
}
#endif

/* Use NULL for simplicity */
#define LIST_POISON1 ((struct list_head *)(void *)0x00100100)
#define LIST_POISON2 ((struct list_head *)(void *)0x00200200)

#define LIST_HEAD_INIT(name) \
    {                        \
        &(name), &(name)     \
    }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_add(struct list_head *new_head,
                              struct list_head *prev,
                              struct list_head *next)
{
    if (!__list_add_valid(new_head, prev, next))
        return;

    next->prev = new_head;
    new_head->next = next;
    new_head->prev = prev;
    prev->next = new_head;
}

/**
 * list_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void list_add(struct list_head *new_head, struct list_head *head)
{
    __list_add(new_head, head, head->next);
}

/**
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void list_add_tail(struct list_head *new_head,
                                 struct list_head *head)
{
    __list_add(new_head, head->prev, head);
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void __list_del_entry(struct list_head *entry)
{
    if (!__list_del_entry_valid(entry))
        return;

    __list_del(entry->prev, entry->next);
}

static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = LIST_POISON1;
    entry->prev = LIST_POISON2;
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

/**
 * list_move - delete from one list and add as another's head
 * @list: the entry to move
 * @head: the head that will precede our entry
 */
static inline void list_move(struct list_head *list, struct list_head *head)
{
    __list_del_entry(list);
    list_add(list, head);
}

/**
 * list_move_tail - delete from one list and add as another's tail
 * @list: the entry to move
 * @head: the head that will follow our entry
 */
static inline void list_move_tail(struct list_head *list,
                                  struct list_head *head)
{
    __list_del_entry(list);
    list_add_tail(list, head);
}

/**
 * list_for_each	-	iterate over a list
 * @pos:	the &struct list_head to use as a loop cursor.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define container_of(ptr, type, member)                    \
    ({                                                     \
        const typeof(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - offsetof(type, member)); \
    })

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 */

#ifdef __cplusplus
#define list_entry(ptr, type, member)                                      \
    (reinterpret_cast<type *>(                                             \
        (char *)(ptr) - (char *)(&(reinterpret_cast<type *>(1)->member)) + \
        1))
#else
#define list_entry(ptr, type, member) container_of(ptr, type, member)
#endif