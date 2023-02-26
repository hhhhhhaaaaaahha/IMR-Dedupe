#include "fid_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "ring_buffer.h"
#include "virtual_groups.h"

ft_head_t g_fid_table;

int init_fid_table(ft_head_t *h)
{
    srand(time(0));
    if (!h)
    {
        fprintf(stderr,
                "Error: initializing ft_head_t failed. head is NULL.\n");
        return -1;
    }

    void *p = NULL;
    if ((p = malloc(sizeof(*h->table) * FID_TALBE_SIZE)) == NULL)
    {
        fprintf(stderr,
                "Error: initializing ft_head_t failed. malloc table failed.\n");
        return -1;
    }
    h->capacity = FID_TALBE_SIZE;
    h->count = 0;
    h->table = (fte_t *)p;

#ifdef ATTR_HISTORY
    if (init_ring_buffer(&m_ring_buffer))
    {
        fprintf(stderr, "Error: init ring buffer failed\n");
        exit(EXIT_FAILURE);
    }
#endif
    return 0;
}

void end_ft_head(ft_head_t *h)
{
    if (!h)
    {
        fprintf(stderr, "Error: clearing ft_head_t failed. head is NULL.\n");
    }
#ifdef ATTR_HISTORY
    exit_ring_buffer(&m_ring_buffer);
#endif
    h->capacity = 0;
    h->count = 0;
    free(h->table);
    h->table = NULL;
}

void inc_ft(ft_head_t *h)
{
    if ((h->table = (fte_t *)realloc(
             h->table, sizeof(*h->table) * h->capacity * 2)) == NULL)
    {
        fprintf(stderr, "Error: increasing ft failed.\n");
        exit(EXIT_FAILURE);
    }
    h->capacity *= 2;
}

int add_fid(ft_head_t *h, unsigned long fid, file_attr_t attr)
{
    if (error_file_attr != get_attr(h, fid))
        return -1;
    if (ft_head_is_full(h))
        inc_ft(h);
    fte_t *e = &h->table[h->count];
    e->fid = fid;
    e->attr = attr;
    h->count++;
    return 0;
}

file_attr_t get_attr(ft_head_t *h, unsigned long fid)
{
    for (int i = 0; i < h->count; i++)
    {
        fte_t *e = &h->table[i];
        if (e->fid == fid)
            return e->attr;
    }
    return error_file_attr;
}

#ifdef ATTR_HISTORY
extern unsigned long long bytes;
file_attr_t generate_attr(unsigned long fid)
{
    file_attr_t attr;
    size_t old_len;
    attr = (bytes < m_ring_buffer.avg) ? hot_file_attr : cold_file_attr;
    old_len = m_ring_buffer.deleted(&m_ring_buffer);
    m_ring_buffer.total = m_ring_buffer.total - old_len + bytes;
    m_ring_buffer.avg = m_ring_buffer.total / RING_BUFFER_SIZE;
    m_ring_buffer.insert(&m_ring_buffer, bytes);
    return attr;
}
#elif 0
file_attr_t generate_attr(unsigned long fid)
{
    int rand_num = rand();
    int i = rand_num % 100;
    file_attr_t attr = (i < HOT_PROBABILITY ? hot_file_attr : cold_file_attr);
    add_fid(&g_fid_table, fid, attr);
    return attr;
}
#else
file_attr_t generate_attr(unsigned long fid)
{
    static int count = 0;
    file_attr_t attr = (count % 2 ? hot_file_attr : cold_file_attr);
    count++;
    add_fid(&g_fid_table, fid, attr);
    return attr;
}
#endif