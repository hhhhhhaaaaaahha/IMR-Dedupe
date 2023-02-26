#ifndef BATCH_H
#define BATCH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct batch_block_entry
{
    unsigned long pba;
    bool isVirtual;
};

typedef struct batch_block_entry bb_entry_t;

struct batch_block_head
{
    bb_entry_t *table;
    size_t capacity;
    size_t size;
};

typedef struct batch_block_head bb_head_t;

struct batch_table_head
{
    bb_head_t block_head, extend_head;
};

typedef struct batch_table_head b_table_head_t;

struct disk;
void batch_add(struct disk *d, unsigned long pba, b_table_head_t *table);
void batch_clear(b_table_head_t *table);
int batch_sync(struct disk *d, b_table_head_t *table);
int batch_extend(struct disk *d, b_table_head_t *table);
int init_batch_table(b_table_head_t *table);
void end_batch_table(b_table_head_t *table);
int batch_write(struct disk *d, b_table_head_t *t);
int _batch_read(struct disk *d, b_table_head_t *t);
int batch_read(struct disk *d, b_table_head_t *t);
int batch_delete(struct disk *d, b_table_head_t *t);
int batch_invalid(struct disk *d, b_table_head_t *t);

extern b_table_head_t gbtable;
extern b_table_head_t mbtable;
#endif