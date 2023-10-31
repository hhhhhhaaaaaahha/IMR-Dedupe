#include <assert.h>

#include "active_swap.h"
#include "batch.h"
#include "chs.h"
#include "lba.h"
#include "pba.h"
#include "rw.h"
#include "scp.h"

// 這個function是在找cold top track，如果top track更新次數小於10的話代表這個track是一個cold track
unsigned long find_empty_block_swap(struct disk *d)
{
#ifdef DEDU_WRITE
    int max_referced_count = 0;
    unsigned long pba = -1;
    for (size_t i = 1; i < d->report.max_track_num; i += 2)
    {
        if (d->storage[i].status != status_in_use)
        {
            continue;
        }
        if (d->storage[i].referenced_count > max_referced_count)
        {
            max_referced_count = d->storage[i].referenced_count;
            pba = i;
        }
    }
    // printf("pba = %llu\n", pba);
    // printf("referced_count = %d\n", d->storage[pba].referenced_count);
    // if (d->storage[pba].status == status_in_use)
    //     printf("yes\n");
    // else
    //     printf("no\n");
    return pba;
#else
    int count = 10;
    unsigned long pba = -1;
    for (size_t t = 1; t < d->report.max_track_num; t += 2)
    {
        unsigned long block = t;
        if (d->ptt_table_head->table[block].type != normal_type)
            continue;
        if (d->storage[block].status != status_in_use)
        {
            continue;
        }
        if (d->storage[block].count < count)
        {
            pba = block;
            count = d->storage[block].count;
        }
    }
    return pba;
#endif
}
void createBlockSwap(struct disk *d, unsigned long bba, unsigned long tba)
{
#ifndef DEDU_WRITE
    struct ptt_entry *ble, *tle;
    ble = &d->ptt_table_head->table[bba];
    tle = &d->ptt_table_head->table[tba];
    ble->tba = tba;
    ble->type = block_swap_type;
    ble->scp_pba = tba;
    tle->tba = bba;
    tle->type = block_swap_type;
    tle->scp_pba = bba;
#endif
    // FILE *e = fopen("report/block_swap_record.txt", "a");
    // fprintf(e, "%llu\n", bba);
    // fclose(e);
    d->report.current_block_swap_count++;

    assert(d->storage[tba].status == status_in_use);
    // assert(d->storage[tba].referenced_count > 0);
    // printf("tba = %llu\n", tba);
    // printf("bba = %llu\n", bba);
    // #ifdef TRIM
    //     if (d->storage[bba].status == status_trimed)
    //     {
    //         FILE *e = fopen("report/noswap.txt", "a");
    //         fprintf(e, "\nbottom track doesn't swap to top.\n");
    //         fclose(e);
    //         printf("\nbottom track doesn't swap to top.\n");
    //     }
    //     else
    //     {
    //         rw_block(d, bba, tba);
    //     }
    // #else
    //         rw_block(d, bba, tba);
    // #endif
    rw_block(d, tba, bba);
}

unsigned long run_block_swap(struct disk *d, unsigned long bba)
{
    unsigned long tba = find_empty_block_swap(d);
    if ((signed)tba != -1)
        DEDU_createBlockSwap(d, bba, tba);
    return tba;
}

int init_block_swap(struct disk *d)
{
    enable_block_swap = true;
    return 0;
}
