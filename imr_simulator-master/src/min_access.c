#include <stdio.h>
#include "lba.h"
#include "min_access.h"

struct min_access_head ma_table;

bool ma_table_bigger_than_rear(struct min_access_head *h, int count)
{
    return count > h->table[h->rear].count;
}

bool ma_table_is_full(struct min_access_head *h)
{
    return ((h->front + 1) % h->capacity) == h->rear;
}

bool ma_table_is_empty(struct min_access_head *h)
{
    return h->front == h->rear;
}

void ma_table_swap(struct min_access *a, struct min_access *b)
{
    unsigned long pba = a->pba;
    int count = a->count;
    a->pba = b->pba;
    a->count = b->count;
    b->pba = pba;
    b->count = count;
}

void ma_table_sort(struct min_access_head *h)
{
    long capacity = h->capacity;
    for (long i = h->rear; i != h->front; i = ((i - 1 + capacity) % capacity))
    {
        struct min_access *current = &h->table[i];
        long pre_index = (i - 1 + capacity) % capacity;
        struct min_access *previous = &h->table[pre_index];
        if (current->count <= previous->count)
            break;
        ma_table_swap(current, previous);
    }
}

void ma_table_enqueue(struct min_access_head *h, unsigned long pba, int count)
{
    if (ma_table_is_full(h))
    {
        fprintf(stderr, "Error: Failed to enqueue. It's full.\n");
        exit(EXIT_FAILURE);
    }
    h->rear = (h->rear + 1) % h->capacity;
    long rear = h->rear;
    h->table[rear].pba = pba;
    h->table[rear].count = count;
    ma_table_sort(h);
}

unsigned long ma_table_dequeue(struct min_access_head *h)
{
    if (ma_table_is_empty(h))
    {
        fprintf(stderr, "Error: Failed to dequeue. It's empty.\n");
        exit(EXIT_FAILURE);
    }
    long front = (h->front + 1) % h->capacity;
    h->front = front;
    return h->table[front].pba;
}

void end_min_access_table(struct disk *d)
{
    free(ma_table.table);
    ma_table.capacity = 0;
    ma_table.rear = -1;
    ma_table.front = -1;
}

void ma_table_sort_index(struct min_access_head *h, long index)
{
    long capacity = h->capacity;
    for (long i = index; i != h->front; i = ((i - 1 + capacity) % capacity))
    {
        struct min_access *current = &h->table[i];
        long pre_index = (i - 1 + capacity) % capacity;
        struct min_access *previous = &h->table[pre_index];
        if (current->count <= previous->count)
            break;
        ma_table_swap(current, previous);
    }
}

void ma_table_replace_rear(struct min_access_head *h, unsigned long pba, int count)
{
    h->table[h->rear].pba = pba;
    h->table[h->rear].count = count;
    ma_table_sort(h);
}

void iterate_block(struct disk *d)
{
    for (size_t i = 0; i < d->report.max_block_num; i++)
    {
        struct block *b = &d->storage[i];
        if (b->status != status_in_use)
            continue;
        if (!ma_table_is_full(&ma_table))
            ma_table_enqueue(&ma_table, i, b->count);
        else
        {
            if (ma_table_bigger_than_rear(&ma_table, b->count))
            {
                ma_table_replace_rear(&ma_table, i, b->count);
            }
        }
    }
}

bool ma_table_in_table(struct min_access_head *h, unsigned long pba, long *index)
{
    long capacity = h->capacity;
    long i = h->front;
    while (i != h->rear)
    {
        i = (i + 1) % capacity;
        struct min_access *e = &h->table[i];
        if (e->pba == pba)
        {
            *index = i;
            return true;
        }
    }
    return false;
}

void ma_table_update(struct min_access_head *h, unsigned long pba, int count)
{
    long index;
    if (ma_table_in_table(h, pba, &index))
        ma_table_sort_index(h, index);
    else if (ma_table_is_full(h))
        ma_table_replace_rear(h, pba, count);
    else
        ma_table_enqueue(h, pba, count);
}

int init_min_access_table(struct disk *d)
{
    long capacity = d->report.max_top_buffer_num;
    if (!(ma_table.table = (struct min_access *)malloc(capacity)))
    {
        fprintf(stderr, "Error: Failed to malloc min_access.\n");
        exit(EXIT_FAILURE);
    }
    // set first item to 0 to be able to compare to.
    ma_table.table[0].count = 0;
    ma_table.capacity = capacity;
    ma_table.rear = -1;
    ma_table.front = -1;
    ma_table.is_empty = ma_table_is_empty;
    ma_table.is_full = ma_table_is_full;
    ma_table.enqueue = ma_table_enqueue;
    ma_table.dequeue = ma_table_dequeue;
    ma_table.big_than_rear = ma_table_bigger_than_rear;
    ma_table.swap = ma_table_swap;
    ma_table.sort = ma_table_sort;
    ma_table.update = ma_table_update;

    iterate_block(d);
    return 0;
}
