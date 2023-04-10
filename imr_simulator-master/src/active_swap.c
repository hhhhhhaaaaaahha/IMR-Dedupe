#include <assert.h>
#include "block_swap.h"
#include "lba.h"
#include "rw.h"
bool is_bottom_has_trimed_track = false;
void block_info_copy(struct block *dest, struct block *src)
{
    dest->count = src->count;
    strcpy(dest->hash, src->hash);
    dest->lba_capacity = src->lba_capacity;
    dest->referenced_count = src->referenced_count;
    dest->status = src->status;
    dest->lba = src->lba;
}
void block_info_swap(struct disk *d, unsigned long from, unsigned long to)
{
    struct block temp;
    struct block *from_track = &d->storage[from];
    struct block *to_track = &d->storage[to];
    block_info_copy(&temp, to_track);
    block_info_copy(to_track, from_track);
    block_info_copy(from_track, &temp);
}
void print_info(struct disk *d, unsigned long bba, unsigned long tba)
{
    struct block *bottom_track = &d->storage[bba];
    struct block *top_track = &d->storage[tba];
    printf("bottom track's hash = %s\n", bottom_track->hash);
    printf("bottom track's referenced count = %d\n", bottom_track->referenced_count);
    printf("top track's hash = %s\n", top_track->hash);
    printf("top track's referenced count = %d\n", top_track->referenced_count);
}
void ltp_table_swap(struct disk *d, unsigned long old_pba, unsigned new_pba)
{
    struct ltp_entry *entry = d->ltp_table_head->table;
    for (unsigned long i = 0; i < d->storage[old_pba].referenced_count + 1; i++)
    {
        unsigned long lba = d->storage[old_pba].lba[i];
        entry[lba].pba = new_pba;
    }
    for (unsigned long i = 0; i < d->storage[new_pba].referenced_count + 1; i++)
    {
        unsigned long lba = d->storage[new_pba].lba[i];
        entry[lba].pba = old_pba;
    }
}
void DEDU_createBlockSwap(struct disk *d, unsigned long from, unsigned long to)
{
    d->report.current_block_swap_count++;
    rw_block(d, from, to); // 是否可以調整?
    ltp_table_swap(d, from, to);
    block_info_swap(d, to, from);
}
unsigned long find_unused_bottom_block(struct disk *d)
{
    uint64_t max = d->report.max_block_num;
    if (is_bottom_has_trimed_track)
    {
        for (uint64_t i = 0; i < max; i += 2)
        {
            if (d->storage[i].status == status_trimed)
                return i;
        }
        is_bottom_has_trimed_track = false;
    }
    // for (uint64_t i = 0; i < max; i += 2) // 註解掉
    // {
    //     if (d->storage[i].referenced_count == 0)
    //         return i;
    // }
    return -1;
}
bool is_top_should_be_swap(struct disk *d, unsigned long tba)
{
    if (d->storage[tba].status == status_in_use && d->storage->referenced_count > 0)
        return true;
    return false;
}
void inversion_swap(struct disk *d, unsigned long tba, unsigned long bba)
{
    // printf("inversion_swap_is_called\n");
    if (is_top_should_be_swap(d, tba))
    {
        unsigned long swap_bba = find_unused_bottom_block(d);
        printf("swap_bba = %lu\n", swap_bba);
        printf("bba = %lu\n", bba);
        if (swap_bba != bba && (signed)swap_bba != -1)
        {
            DEDU_createBlockSwap(d, tba, swap_bba);
            d->report.reversion_swap_count++;
        }
    }
}
unsigned long DEDU_run_block_swap(struct disk *d, unsigned long bba)
{
    // printf("block swap is called\n");
    // printf("bba = %lu\n", bba);
#ifdef TRIM
    unsigned long next_tba = bba + 1; // 該 bottom track 的後面一個 top track
    if (bba != 0)
    {
        unsigned long pre_tba = bba - 1; // 該 bottom track 的前面一個 top track
        inversion_swap(d, pre_tba, bba);
    }
    inversion_swap(d, next_tba, bba);
#endif
    unsigned long tba = find_empty_block_swap(d);
    if ((signed)tba != -1)
        DEDU_createBlockSwap(d, tba, bba);
    return tba;
}