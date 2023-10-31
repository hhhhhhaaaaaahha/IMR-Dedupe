#pragma once

#ifdef VIRTUAL_GROUPS
#include "virtual_groups.h"
#undef NATIVE
#undef ZALLOC
#undef TOP_BUFFER
#undef BLOCK_SWAP
#endif

#define DEDU_ORIGIN

// #define NO_DEDU
// #define DEDU_WRITE

// #define BLOCK_SWAP
// #define DEDU_WRITE

#ifdef NO_DEDU
#define ZALLOC
#define BLOCK_SWAP
#define TOP_BUFFER
#endif

#ifdef DEDU_WRITE
#define ZALLOC
#endif

#ifdef BLOCK_SWAP
#define TOP_BUFFER
#define ZALLOC
#endif

#ifdef TOP_BUFFER
#define ZALLOC
#endif

#ifdef NATIVE
#undef ZALLOC
#undef TOP_BUFFER
#undef BLOCK_SWAP
#undef VIRTUAL_GROUPS
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KILOBYTE 1024UL
#define MEGABYTE (KILOBYTE * KILOBYTE)
#define GIGABYTE (KILOBYTE * GIGABYTE)
#define MAX_LENS 80UL
#define SECTOR_PER_TRACK 512UL
#define SECTOR_SIZE 4096UL
#define TRACK_SIZE (SECTOR_SIZE * SECTOR_PER_TRACK)

#define BLOCK_SIZE TRACK_SIZE

#define P1_BUF_NUM 32

#define GB_TO_BYTE(n) ((uint64_t)(n) << 30)

#define SECTOR_BITS (64 - __builtin_clzl(SECTOR_PER_TRACK) - 1)
#define SECTOR_MASK ((0x1ULL << SECTOR_BITS) - 1)
#define TRACK_MASK (~SECTOR_MASK)
#define CLEAR_SECTOR(x) ((x) & ~SECTOR_MASK)

#define lba_to_track(lba) (lba)

typedef enum
{
    lbaread,
    lbawrite,
    lbadelete
} lba_command;

typedef enum
{
    zalloc_phase1,
    zalloc_phase2,
    zalloc_phase3,
#ifdef DEDU_WRITE
    zalloc_phase4
#endif
} zalloc_phases;

typedef enum
{
    status_free,
    status_booked,
    status_in_use,
    status_trimed,
    status_invalid,
    status_end
} block_status_t;

struct disk;
struct track;
struct block;
struct buffer_block;
struct disk_operations;
struct ltp_table_head;
struct ltp_entry;
struct ptt_table_head;
struct ptt_entry;
#ifdef DEDU_ORIGIN
struct dedu_info
{
    unsigned long current_track;
};
#endif
struct zalloc_info
{
    unsigned long phase1_start;
    unsigned long phase1_end;
    bool phase1_is_full;
    unsigned long phase2_start;
    unsigned long phase2_end;
    unsigned long phase3_start;
    unsigned long phase3_end;
    unsigned long current_track;
#ifdef DEDU_WRITE
    unsigned long phase4_start;
    unsigned long phase4_end;
#endif
    zalloc_phases phases;
};
struct time_size
{
    uint64_t total_read_time;
    uint64_t total_write_time;
    uint64_t total_write_size;
    uint64_t total_read_block_size;
    uint64_t total_write_block_size;
};

struct report
{
#ifdef TOP_BUFFER
    uint64_t max_top_buffer_num;
    uint64_t current_top_buffer_count;
    uint64_t total_write_top_buffer_size;
    uint64_t total_read_scp_size;
    int scp_count;
#endif
#ifdef BLOCK_SWAP
    uint64_t block_swap_boundary;
    uint64_t current_block_swap_count;
#elif defined DEDU_WRITE
    uint64_t block_swap_boundary;
    uint64_t max_top_buffer_num;
    uint64_t current_block_swap_count;
#endif
    struct time_size normal, journaling;
    uint64_t max_track_num;
    uint64_t max_block_num;
    uint64_t current_use_block_num;
    uint64_t total_access_time;
    /* delete information */
    uint64_t total_delete_time;
    uint64_t total_delete_write_block_size;
    uint64_t total_delete_write_size;
    uint64_t total_delete_rewrite_size;
    uint64_t total_delete_reread_size;
    uint64_t total_read_size;
    uint64_t total_write_size;
    uint64_t total_rewrite_size;
    uint64_t total_reread_size;
    uint64_t ins_count;
    uint64_t read_ins_count;
    uint64_t write_ins_count;
    uint64_t delete_ins_count;
    uint64_t num_invalid_read;
    uint64_t num_invalid_write;
    /* phase1 buffer */
    uint64_t next_top_to_write;
    uint64_t next_bottom_to_write;
    uint32_t used_buffer_count;
    bool buffer_is_full;
    bool buffer_flushed;
#ifdef VIRTUAL_GROUPS
    uint64_t dual_swap_count;
#endif
#ifdef DEDU_ORIGIN
    uint64_t max_logical_block_num;
    uint64_t reversion_swap_count;
#endif
};

struct disk
{
    struct block *storage;
    struct disk_operations *d_op;
    struct ltp_table_head *ltp_table_head;
    /* top-buffer */
    struct ptt_table_head *ptt_table_head;

    struct buffer_block *phase1_buf;

#ifdef DEDU_ORIGIN
    struct dedu_info dinfo;
#endif
#ifdef ZALLOC
    struct zalloc_info zinfo;
#endif
#ifdef VIRTUAL_GROUPS
    struct _virtual_groups_head_t vgh;
#endif
    struct report report;
};

struct block
{
    char hash[20];
    unsigned long *lba;
    block_status_t status; /* block status */
    unsigned lba_capacity;
    unsigned count;
    unsigned referenced_count;
};

struct buffer_block
{
    char hash[20];
    unsigned long *lba;
    block_status_t status; /* block status */
    unsigned lba_capacity;
    unsigned referenced_count;
    bool dedupe;
};

static inline bool storage_is_free(struct disk *d, unsigned long pba)
{
#ifdef NO_DEDU
    return (d->storage[pba].status == status_invalid) || (d->storage[pba].status == status_free) || (d->storage[pba].status == status_trimed);
#else
    return (d->storage[pba].status == status_invalid) || (d->storage[pba].status == status_free);
#endif
}

static inline bool storage_is_in_use(struct disk *d, unsigned long pba)
{
    return (d->storage[pba].status == status_booked) || (d->storage[pba].status == status_in_use);
}

struct disk_operations
{
    int (*read)(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
    int (*write)(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
    int (*remove)(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
    int (*journaling_write)(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
    int (*invalid)(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
    int (*DEDU_write)(struct disk *d, unsigned long lba, size_t n, char *hash, int line_cnt);
    void (*DEDU_remove)(struct disk *d, unsigned long lba, size_t n, char *hash);
    void (*new_alloc)(struct disk *d, unsigned long lba, size_t n, char *hash, int line_cnt);
};

struct ltp_entry
{
    unsigned long pba;
    unsigned long fid;
    char hash[20];
    bool valid;
    bool trim;
};

typedef enum
{
    normal_type,
    top_buffer_type,
    buffered_type,
    block_swap_type,
    end_type
} block_type_t;

struct ptt_entry
{
    unsigned long tba;     /* top-buffer address */
    unsigned long scp_pba; /* scp write-back address */
    block_type_t type;
};

struct ltp_table_head
{
    struct ltp_entry *table;
    size_t count;
};

struct ptt_table_head
{
    struct ptt_entry *table;
    size_t count;
};

bool is_block_data_valid(struct disk *d, unsigned long lba, unsigned long fid);
bool is_ltp_mapping_valid(struct disk *d, unsigned long lba, unsigned long fid);
void clear_info(struct disk *d);
int init_disk(struct disk *disk, int physical_size, int logical_size);
void end_disk(struct disk *disk);

int lba_read(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
int lba_write(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
int lba_delete(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
int journaling_write(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
int lba_invalid(struct disk *d, unsigned long lba, size_t n, unsigned long fid);

int vg_lba_delete(struct disk *d, unsigned long lba, size_t n, unsigned long fid);
#ifdef DEDU_ORIGIN
bool DEDU_is_ltp_mapping_valid(struct disk *d, unsigned long lba, char *hash);
int dedupe_lba_write(struct disk *d, unsigned long lba, size_t n, char *hash, int line_cnt);
void DEDU_Trim(struct disk *d, unsigned long lba, size_t n, char *hash);
void delete_all_bottom_track(struct disk *d);
#endif
void dedupe_lba_write_p1_buf(struct disk *d, unsigned long lba, size_t n, char *hash, int line_cnt);
void buffer_write(struct disk *d, unsigned long lba, char *hash);
void buffer_flush(struct disk *d);
extern bool enable_top_buffer;
extern bool enable_block_swap;