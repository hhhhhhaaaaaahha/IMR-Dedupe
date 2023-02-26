#include "ring_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

ring_buffer_head_t m_ring_buffer;

int init_ring_buffer(ring_buffer_head_t *h)
{
    if (!h)
    {
        fprintf(
            stderr,
            "Error: initializing ring_buffer_head_t failed. head is NULL.\n");
        return -1;
    }

    void *p = NULL;
    if ((p = malloc(sizeof(*h->ring_buffer) * (RING_BUFFER_SIZE + 1))) ==
        NULL)
    {
        fprintf(stderr,
                "Error: initializing ring_buffer_head_t failed. malloc table "
                "failed.\n");
        return -1;
    }
    h->ring_buffer = (size_t *)p;
    h->capacity = RING_BUFFER_SIZE + 1;
    h->front = 0;
    h->rear = RING_BUFFER_SIZE;
    h->avg = 0;
    h->insert = ring_buffer_insert;
    h->deleted = ring_buffer_delete;
    h->is_full = ring_buffer_is_full;
    h->is_empty = ring_buffer_is_empty;

    memset(p, 0, sizeof(*h->ring_buffer) * (RING_BUFFER_SIZE + 1));
    return 0;
}

void exit_ring_buffer(ring_buffer_head_t *h)
{
    if (!h)
    {
        fprintf(stderr,
                "Error: clearing ring_buffer_head_t failed. head is NULL.\n");
    }
    h->capacity = 0;
    free(h->ring_buffer);
    h->ring_buffer = NULL;
}
void ring_buffer_insert(ring_buffer_head_t *h, size_t len)
{
    if (ring_buffer_is_full(h))
    {
        fprintf(stderr, "Error: f_len_table is full\n");
        exit(EXIT_FAILURE);
    }
    h->rear = (h->rear + 1) % h->capacity;
    h->ring_buffer[h->rear] = len;
}
bool ring_buffer_is_empty(ring_buffer_head_t *h)
{
    return h->front == h->rear;
}

bool ring_buffer_is_full(ring_buffer_head_t *h)
{
    return h->front == (h->rear + 1) % h->capacity;
}

size_t ring_buffer_delete(ring_buffer_head_t *h)
{
    if (ring_buffer_is_empty(h))
    {
        fprintf(stderr, "Error: f_len_table is empty.\n");
        exit(EXIT_FAILURE);
    }
    h->front = (h->front + 1) % h->capacity;
    return h->ring_buffer[h->front];
}