#include "top_buffer.h"

#include "batch.h"
#include "chs.h"
#include "lba.h"
#include "output.h"
#include "scp.h"

// Start from the end of journaling area top track
unsigned long top_buffer_start;
unsigned long current_top_buffer_track;
const size_t Bmax = 2;

int init_top_buffer(struct disk *d)
{
#if defined(JFS) && !defined(ZALLOC_JOURNAL_VIRTUAL)
    top_buffer_start = d->report.max_track_num / 100 * 2 + 1;
#else
    top_buffer_start = 1;
#endif
    current_top_buffer_track = top_buffer_start;
    return 0;
}
// 找到下一個空的top buffer中的block
bool find_empty_top_buffer(struct disk *d, unsigned long *p)
{
    // 從目前使用到的top buffer的index來找下一個，找到zinfo.phase2_start為止(因為zinfo.phase2_start是最後一個top track的index)
    for (unsigned long pba = current_top_buffer_track; pba <= d->zinfo.phase2_start; pba += 2)
    {
        // 檢查該block是否為free，是的話就回傳true代表有找到，且透過p回傳該pba
        if (storage_is_free(d, pba))
        {
            current_top_buffer_track = pba;
            *p = pba;
            return true;
        }
    }
    // 從top buffer的頭開始找下一個，找到目前使用的那個(current_top_buffer_track)為止
    for (unsigned long pba = top_buffer_start; pba < current_top_buffer_track; pba += 2)
    {
        if (storage_is_free(d, pba))
        {
            current_top_buffer_track = pba;
            *p = pba;
            return true;
        }
    }
    return false;
}
// 將bba(也就是bottom track的pba)跟要對應的tba建立關西
void create_top_buffer(struct disk *d, unsigned long bba, unsigned long tba)
{
    struct ptt_entry *bba_entry = &d->ptt_table_head->table[bba];
    struct ptt_entry *tba_entry = &d->ptt_table_head->table[tba];
    /* create relation */
    bba_entry->tba = tba;
    // 在bottom track有對應的tba的block type會變成top_buffer_type
    bba_entry->type = top_buffer_type;
    tba_entry->tba = tba;
    // 在tba對應到的block中的scp_pba建立對應原本在bottom track的pba，這樣才知道做scp的時候要做回去的pba在哪裡
    tba_entry->scp_pba = bba;
    // top buffer如果有對應到的話，該block的type就會變成buffered_type
    tba_entry->type = buffered_type;
    d->storage[tba].status = status_booked;
#ifdef TOP_BUFFER
    d->report.current_top_buffer_count++;
    d->report.total_write_top_buffer_size += SECTOR_SIZE;
#endif
}

/*
 * The track of pba is bottom track.
 */
unsigned long run_top_buffer(struct disk *d, unsigned long bba)
{
    // printf("run_top_buffer is called\n");
    unsigned long tba;
    // 如果top buffer是滿了的話就要執行scp
#ifdef TOP_BUFFER
    if (is_top_buffer_full(d))
    {
        tba = scp(d);
        create_top_buffer(d, bba, tba);
        return tba;
    }
#endif
    // 如果傳進來的pba對應到的tba的type不是normal_type的話
    if (d->ptt_table_head->table[bba].type != normal_type)
    {
        fprintf(stderr, "Error: a bottom block is top_buffered in %s.c\n", __FUNCTION__);
        fprintf(stderr, "bba: %lu, type: %d\n", bba, d->ptt_table_head->table[bba].type);
        exit(EXIT_FAILURE);
    }
    // 如果在top buffer中找不到下一個空的block
    if (!find_empty_top_buffer(d, &tba))
    {
#ifdef TOP_BUFFER
        printf("current_top_buffer_count = %llu\n", d->report.current_top_buffer_count);
        printf("max_top_buffer_num = %llu\n", d->report.max_top_buffer_num);
#endif
        fprintf(stderr, "Error: can't find any free top block.\n");
        output_disk_info(d);
        output_ltp_table(d);
        output_ptt_table(d);
        exit(EXIT_FAILURE);
    }
    create_top_buffer(d, bba, tba);
    return tba;
}
// 用來判斷top buffer是不是滿的
#ifdef TOP_BUFFER
bool is_top_buffer_full(struct disk *d)
{
    return d->report.max_top_buffer_num <= d->report.current_top_buffer_count;
}
#endif
bool could_trigger_topbuffer(struct disk *d, unsigned long track)
{
    return track >= (d->report.max_block_num - d->report.max_top_buffer_num);
}