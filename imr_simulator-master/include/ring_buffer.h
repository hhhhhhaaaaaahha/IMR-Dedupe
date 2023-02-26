#pragma once

#include <stdbool.h>
#include <stddef.h>

#define RING_BUFFER_SIZE 1000

typedef struct _ring_buffer_head ring_buffer_head_t;

struct _ring_buffer_head
{
    int capacity, front, rear;
    long avg, total;
    size_t *ring_buffer;
    void (*insert)(ring_buffer_head_t *h, size_t len);
    size_t (*deleted)(ring_buffer_head_t *h);
    bool (*is_full)(ring_buffer_head_t *h);
    bool (*is_empty)(ring_buffer_head_t *h);
};

int init_ring_buffer(ring_buffer_head_t *h);
void exit_ring_buffer(ring_buffer_head_t *h);
void ring_buffer_insert(ring_buffer_head_t *h, size_t len);
size_t ring_buffer_delete(ring_buffer_head_t *h);
bool ring_buffer_is_full(ring_buffer_head_t *h);
bool ring_buffer_is_empty(ring_buffer_head_t *h);

extern ring_buffer_head_t m_ring_buffer;