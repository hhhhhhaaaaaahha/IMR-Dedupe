#pragma once

#include "chs.h"
struct disk;
typedef struct batch_table_head b_table_head_t;

void record_read(struct disk *d, b_table_head_t *table);
void record_read_physical_tracks(struct disk *d, long tracks);
void record_write(struct disk *d, b_table_head_t *table);
void record_write_physical_tracks(struct disk *d, long tracks);
void record_write_virtual_tracks(struct disk *d,
                                 long tracks,
                                 long num_phy_tracks);
void record_delete(struct disk *d, b_table_head_t *table);
void record_delete_physical_tracks(struct disk *d, long tracks);
void record_delete_virtual_tracks(struct disk *d,
                                  long tracks,
                                  long num_phy_tracks);