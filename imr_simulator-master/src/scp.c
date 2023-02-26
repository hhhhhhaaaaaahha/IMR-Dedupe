#include <assert.h>
#include "batch.h"
#include "block_swap.h"
#include "chs.h"
#include "lba.h"
#include "pba.h"
#include "record_op.h"
#include "scp.h"

extern unsigned long top_buffer_start;
unsigned long current_scp_track;

b_table_head_t scp_top_mtable;
b_table_head_t scp_bottom_mtable;
bool scp_mode = false;

int init_scp(struct disk *d)
{
    current_scp_track = top_buffer_start;

    if (init_batch_table(&scp_top_mtable))
        goto done_scp_top_mtable;
    if (init_batch_table(&scp_bottom_mtable))
        goto done_scp_bottom_mtable;
    return 0;

done_scp_bottom_mtable:
    end_batch_table(&scp_top_mtable);
done_scp_top_mtable:
    return -1;
}

void end_scp()
{
    end_batch_table(&scp_top_mtable);
    end_batch_table(&scp_bottom_mtable);
}

size_t scp(struct disk *d)
{
    unsigned track;
    size_t count = 0;
    // 如果report.current_top_buffer_count == 0，代表沒有任何一個top buffer被使用
#ifdef TOP_BUFFER
    if (d->report.current_top_buffer_count == 0)
    {
        fprintf(stderr, "Error: Doing scp but no top buffer block found.\n");
        fprintf(stderr, "top buffer: %lu blocks.\n", d->report.current_top_buffer_count);
        exit(EXIT_FAILURE);
    }
#endif
    if (-1 == ((signed)(track = find_victim_track(d))))
    {
        fprintf(stderr, "Error: can't find victim track.\n");
        exit(EXIT_FAILURE);
    }
    scp_mode = true;
    count = run_scp(d, track);
    scp_mode = false;
#ifdef TOP_BUFFER
    d->report.total_read_scp_size += count * SECTOR_SIZE;
    d->report.scp_count++;
#endif
    return count;
}
unsigned long find_victim_track(struct disk *d)
{
    for (unsigned long i = current_scp_track; i < d->report.max_track_num; i += 2)
    {
        if (d->ptt_table_head->table[i].type == buffered_type)
        {
            current_scp_track = i;
            return i;
        }
    }
    for (unsigned long i = top_buffer_start; i < current_scp_track; i += 2)
    {
        if (d->ptt_table_head->table[i].type == buffered_type)
        {
            current_scp_track = i;
            return i;
        }
    }
    return -1;
}
size_t run_scp(struct disk *d, unsigned long track)
{
    unsigned long tba = track;
    struct ptt_entry *tba_entry = NULL;
    size_t count = 0;

    tba_entry = &d->ptt_table_head->table[tba];
    if (tba_entry->type == buffered_type)
    {
        unsigned long bba = tba_entry->scp_pba;
        if (storage_is_free(d, bba))
        {
            fprintf(stderr, "Error: bba = %lu is not in use.\n", bba);
            exit(EXIT_FAILURE);
        }
        block_type_t bba_type = d->ptt_table_head->table[bba].type;
        assert(bba_type == top_buffer_type);
        /* calculate time/size. read top block */
        batch_add(d, tba, &scp_top_mtable);
        /* calculate time/size. write bottom block */
        batch_add(d, bba, &scp_bottom_mtable);

        tba_entry->scp_pba = 0;
        tba_entry->type = normal_type;
        d->ptt_table_head->table[bba].type = normal_type;
        d->ptt_table_head->table[bba].tba = 0;
        unsigned long lba = d->storage[tba].lba[0];
        d->ltp_table_head->table[lba].pba = bba;
        d->report.current_use_block_num--;
#ifdef TOP_BUFFER
        d->report.current_top_buffer_count--;
#endif
#ifdef BLOCK_SWAP
        if (enable_block_swap)
        {
            run_block_swap(d, bba);
        }
#endif
        count++;
    }
    int read_count = _batch_read(d, &scp_top_mtable);
    batch_delete(d, &scp_top_mtable);
    // write bottom track
    int write_count = batch_write(d, &scp_bottom_mtable);
    if (read_count != write_count)
    {
        fprintf(stderr, "Error: num of block reading doesn't equal to num of block "
                        "writing in %s.\n",
                __FUNCTION__);
        exit(EXIT_FAILURE);
    }
    return count;
}