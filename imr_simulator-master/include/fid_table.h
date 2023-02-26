#pragma once

#include <stdbool.h>
#include "virtual_groups.h"

#define FID_TALBE_SIZE 10
// Hot probability, unit = %
#define HOT_PROBABILITY 50

typedef struct _ft_head_t ft_head_t;
typedef struct _fte_t fte_t;

struct _fte_t
{
    unsigned long fid;
    file_attr_t attr;
};

struct _ft_head_t
{
    int count;
    int capacity;
    fte_t *table;
};

static inline bool ft_head_is_full(ft_head_t *h)
{
    return h->capacity == h->count;
}

int init_fid_table(ft_head_t *h);
void end_ft_head(ft_head_t *h);
file_attr_t get_attr(ft_head_t *h, unsigned long fid);
int add_fid(ft_head_t *h, unsigned long fid, file_attr_t attr);
file_attr_t generate_attr(unsigned long fid);

static inline bool in_fid_table(ft_head_t *h, unsigned long fid)
{
    return error_file_attr != get_attr(h, fid);
}
extern ft_head_t g_fid_table;