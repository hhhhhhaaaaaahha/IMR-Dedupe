#include "bug.h"
#include "list.h"

bool __list_add_valid(struct list_head *new_head,
                      struct list_head *prev,
                      struct list_head *next)
{
    if (CHECK_DATA_CORRUPTION(next->prev != prev,
                              "list_add corruption. next->prev should be prev "
                              "(%px), but was %px. (next=%px).\n",
                              prev, next->prev, next) ||
        CHECK_DATA_CORRUPTION(prev->next != next,
                              "list_add corruption. prev->next should be next "
                              "(%px), but was %px. (prev=%px).\n",
                              next, prev->next, prev) ||
        CHECK_DATA_CORRUPTION(
            new_head == prev || new_head == next,
            "list_add double add: new_head=%px, prev=%px, next=%px.\n",
            new_head, prev, next))
        return false;

    return true;
}

bool __list_del_entry_valid(struct list_head *entry)
{
    struct list_head *prev, *next;

    prev = entry->prev;
    next = entry->next;

    if (CHECK_DATA_CORRUPTION(
            next == LIST_POISON1,
            "list_del corruption, %px->next is LIST_POISON1 (%px)\n", entry,
            LIST_POISON1) ||
        CHECK_DATA_CORRUPTION(
            prev == LIST_POISON2,
            "list_del corruption, %px->prev is LIST_POISON2 (%px)\n", entry,
            LIST_POISON2) ||
        CHECK_DATA_CORRUPTION(
            prev->next != entry,
            "list_del corruption. prev->next should be %px, but was %px\n",
            entry, prev->next) ||
        CHECK_DATA_CORRUPTION(
            next->prev != entry,
            "list_del corruption. next->prev should be %px, but was %px\n",
            entry, next->prev))
        return false;

    return true;
}