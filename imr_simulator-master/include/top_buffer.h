#pragma once
#include <stdbool.h>
#include <stdlib.h>

struct disk;
void create_top_buffer(struct disk *d, unsigned long from, unsigned long to);
bool find_empty_top_buffer(struct disk *d, unsigned long *p);
unsigned long run_top_buffer(struct disk *d, unsigned long pba);
bool is_top_buffer_full(struct disk *d);
bool could_trigger_topbuffer(struct disk *d, unsigned long track);
int init_top_buffer(struct disk *d);
extern const size_t Bmax;
extern unsigned long current_top_buffer_track;
extern unsigned long top_buffer_start;
#define TOP_BUFFER_CAPACITY(x) (Bmax * ((x) / 100))