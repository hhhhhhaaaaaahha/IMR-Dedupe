#pragma once

#include <stdlib.h>

struct disk;

struct min_access
{
    unsigned long pba;
    long count;
};

struct min_access_head
{
    struct min_access *table;
    long front, rear, capacity;
    bool (*is_full)(struct min_access_head *h);
    bool (*is_empty)(struct min_access_head *h);
    void (*enqueue)(struct min_access_head *h, unsigned long pba, int count);
    unsigned long (*dequeue)(struct min_access_head *h);
    bool (*big_than_rear)(struct min_access_head *h, int count);
    void (*swap)(struct min_access *a, struct min_access *b);
    void (*sort)(struct min_access_head *h);
    bool (*in_table)(struct min_access_head *h, unsigned long pba, int count, long *index);
    void (*replace_rear)(struct min_access_head *h, unsigned long pba, int count);
    void (*update)(struct min_access_head *h, unsigned long pba, int count);
};

extern struct min_access_head ma_table;

int init_min_access_table(struct disk *d);