#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_LENS 80UL
/* track 0 is bottom track */
#define is_toptrack(t) ((t) % 2)

/* unit = ns */
#define L_SEEK 8000000UL
#define L_ROTATIONAL 8330000UL
#define L_TRANSFER 13443282UL

/* LBA to PBA */

struct disk;
typedef struct batch_table_head b_table_head_t;
int pba_read(struct disk *d, b_table_head_t *table);
int pba_write(struct disk *d, b_table_head_t *table);
int pba_delete(struct disk *d, b_table_head_t *table);
unsigned long pba_search(struct disk *d, unsigned long lba, unsigned long fid);
unsigned long find_next_pba(struct disk *d, unsigned long t, unsigned long fid);
unsigned long lba_to_pba(struct disk *d, unsigned long lba);
unsigned long lba_to_tba(struct disk *d, unsigned long lba);

unsigned long pba_search_jfs(struct disk *d, unsigned long lba, unsigned long fid);
#ifdef DEDU_ORIGIN
void DEDU_update_ltp_table(struct disk *d, unsigned long lba, unsigned pba, char *hash);
unsigned long DEDU_pba_search(struct disk *d, unsigned long lba, char *hash);
unsigned long DEDU_find_next_pba(struct disk *d, unsigned long t, char *hash);
void DEDU_delete_ltp_table(struct disk *d, unsigned long lba);
#endif