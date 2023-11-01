#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "active_swap.h"
#include "batch.h"
#include "chs.h"
#include "fid_table.h"
#include "lba.h"
#include "list.h"
#include "op_mode.h"
#include "output.h"
#include "pba.h"
#include "scp.h"
#ifdef TOP_BUFFER
#include "top_buffer.h"
#endif
#ifdef BLOCK_SWAP
#include "block_swap.h"
#elif defined DEDU_WRITE
#include "block_swap.h"
#endif

bool initialized = false;
// bool phase1_is_full = false;
#ifdef DEDU_ORIGIN
extern FILE *fp;
#endif
extern jmp_buf env;

#ifdef ZALLOC
unsigned long zalloc_find_next_pba(struct disk *d, unsigned long t, unsigned long fid, bool dedupe)
{
    struct zalloc_info *zinfo = &d->zinfo;
    unsigned long track, pba;
    zalloc_phases phases;
    while (1)
    {
        track = zinfo->current_track;
        phases = zinfo->phases;
        switch (phases)
        {
        case zalloc_phase1:
#ifdef NEW_ALLOC
        {
            struct report *report = &d->report;
            /* sequential search */
            if (zinfo->phase1_is_full)
            {
                for (size_t i = zinfo->phase1_start; i <= zinfo->phase1_end; i += 2)
                {
                    pba = i;
                    if (storage_is_free(d, i))
                    {
                        d->storage[pba].status = status_booked;
                        zinfo->current_track = i;
                        zinfo->phase1_is_full = false;
                        return pba;
                    }
                }
                printf("\nPhase2!\n");
                fprintf(fp, "\nPhase2!\n");
                zinfo->phases = zalloc_phase2;
                zinfo->current_track = zinfo->phase2_start;
                enable_top_buffer = true;
            }
            if (report->next_bottom_to_write >= zinfo->phase1_end)
            {
                zinfo->phase1_is_full = true;
            }
            else
            {
                if (!dedupe && (report->next_top_to_write + 1 < report->next_bottom_to_write))
                {
                    /* can assign to top track */
                    track = report->next_top_to_write;
                    report->next_top_to_write += 2;
                }
                else
                {
                    track = report->next_bottom_to_write;
                    report->next_bottom_to_write += 2;
                }
            }
            break;
        }
#else
            /* sequential search */
            if (zinfo->phase1_is_full)
            {
                for (size_t i = zinfo->phase1_start; i <= zinfo->phase1_end; i += 2)
                {
                    pba = i;
                    if (storage_is_free(d, i))
                    {
                        d->storage[pba].status = status_booked;
                        zinfo->current_track = i;
                        zinfo->phase1_is_full = false;
                        return pba;
                    }
                }
                printf("\nPhase2!\n");
                fprintf(fp, "\nPhase2!\n");
                zinfo->phases = zalloc_phase2;
                zinfo->current_track = zinfo->phase2_start;
                enable_top_buffer = true;
            }
            if (track == zinfo->phase1_end)
            {
                zinfo->phase1_is_full = true;
            }
            else
            {
                zinfo->current_track += 2;
            }
            break;
#endif
        case zalloc_phase2:
            if (track == zinfo->phase2_end)
            {
                printf("\nPhase3!\n");
                fprintf(fp, "\nPhase3!\n");
                zinfo->current_track = zinfo->phase3_start;
                zinfo->phases = zalloc_phase3;
            }
            // 如果沒滿的話就往前兩個top track
            else
            {
                zinfo->current_track -= 4;
            }
            break;

        case zalloc_phase3:
            // 如果track>最大的track數量，代表disk已經滿了
            if (track > d->report.max_track_num)
            {
#ifdef DEDU_WRITE
                printf("\nPhase4!\n");
                fprintf(fp, "\nPhase4!\n");
                zinfo->current_track = zinfo->phase4_start;
                zinfo->phases = zalloc_phase4;
#else
#ifdef TOP_BUFFER
                if (d->report.current_top_buffer_count)
                {
                    scp(d);
                    d->zinfo.current_track = current_scp_track;
                    d->report.max_top_buffer_num--;
                    continue;
                }
#endif
                printf("current top buffer count: %llu\n", d->report.current_top_buffer_count);
                perror("Error: disk is full. Can't find next empty block.");
                output_disk_info(d);
                output_ltp_table(d);
                output_ptt_table(d);
                longjmp(env, 1);
#endif
            }
#ifndef DEDU_WRITE
#ifdef TOP_BUFFER
            else if (could_trigger_topbuffer(d, track) && !initialized)
            {
                enable_top_buffer = false;
                initialized = true;
#ifdef BLOCK_SWAP
                init_block_swap(d);
#endif
            }
#endif
#endif
            else
                zinfo->current_track += 4;
            break;
#ifdef DEDU_WRITE
        case zalloc_phase4:
            for (size_t i = zinfo->phase4_start; i <= zinfo->phase4_end; i += 2)
            {
                pba = i;
                if (storage_is_free(d, i))
                {
#ifdef TOP_BUFFER
                    if (could_trigger_topbuffer(d, track) && initialized == false)
                    {
                        enable_top_buffer = false;
                        initialized = true;
#ifdef BLOCK_SWAP
                        init_block_swap(d);
#endif
                    }
#endif
                    d->storage[pba].status = status_booked;
                    zinfo->current_track = i;
                    return pba;
                }
            }
#ifdef TOP_BUFFER
            // 如果有在使用的top_buffer_count>0的話代表要執行scp
            if (d->report.current_top_buffer_count)
            {
                scp(d);
                // d->zinfo.current_track = current_scp_track;
                continue;
            }
#endif
            perror("Error: disk is full. Can't find next empty block.");
            output_disk_info(d);
            output_ltp_table(d);
            output_ptt_table(d);
            longjmp(env, 1);
            break;
#endif
        default:
            perror("Error: unknown zalloc phases.\n");
            exit(EXIT_FAILURE);
            break;
        }
        pba = track;
#ifdef DEDU_WRITE
        if (phases == zalloc_phase2 || phases == zalloc_phase3)
        {
            unsigned long pre_track = track - 1;
            unsigned long next_track = track + 1;
            if (d->storage[pre_track].referenced_count > 1 && d->storage[next_track].referenced_count > 1)
            {
                if (storage_is_free(d, pba))
                {
                    d->storage[pba].status = status_booked;
                    return pba;
                }
            }
            else
                continue;
        }
#endif
        if (storage_is_free(d, pba) && d->ptt_table_head->table[pba].type == normal_type)
        {
            d->storage[pba].status = status_booked;
            return pba;
        }
    }
    return -1;
}
#endif

// 把當下要做的block的status改成status_booked，並把該block的index回傳
unsigned long native_find_next_pba(struct disk *d, unsigned long t, unsigned long fid)
{
    d->storage[t].status = status_booked;
    return t;
}

#ifdef VIRTUAL_GROUPS
unsigned long isd_select_block(struct disk *d, unsigned long t, unsigned long fid, file_attr_t attr)
{
    return vg_get_block(d, attr);
}

unsigned long isd_find_next_pba(struct disk *d, unsigned long t, unsigned long fid)
{
    file_attr_t attr;
    if (error_file_attr != (attr = get_attr(&g_fid_table, fid)))
    {
        return isd_select_block(d, t, fid, attr);
    }
    else
    {
        attr = generate_attr(fid);
        return isd_select_block(d, t, fid, attr);
    }
}
#endif
// 跟native_find_next_pba功能一樣
unsigned long native_journal_get_block(struct disk *d, unsigned long t, unsigned long fid)
{
    unsigned long pba = t;
    d->storage[pba].status = status_booked;
    return pba;
}

#ifdef ZALLOC
unsigned long zalloc_journal_get_block(struct disk *d, unsigned long t, unsigned long fid)
{
    unsigned long pba = t;
    block_status_t status = d->storage[pba].status;
    assert((status != status_free) || (status != status_invalid));
    d->storage[pba].status = status_booked;
    return pba;
}
#endif

unsigned long find_next_pba_jfs(struct disk *d, unsigned long t, unsigned long fid)
{
#ifdef VIRTUAL_GROUPS
    unsigned long pba = -1;
    do
    {
        if (-1 != (pba = vg_journal_get_block(d)))
            return pba;
    } while ((journal_switch_check_bottom()));
    fprintf(stderr,
            "Error: Can't find free block while journaling.\ntrack = %lu, fid "
            "= %lu\n",
            t, fid);
    exit(EXIT_FAILURE);
#elif defined(ZALLOC_JOURNAL_VIRTUAL)
    return zalloc_find_next_pba(d, t, fid, 0);
#elif defined(ZALLOC)
    return zalloc_journal_get_block(d, t, fid);
#else
    return native_journal_get_block(d, t, fid); //
#endif
}

unsigned long find_next_pba(struct disk *d, unsigned long t, unsigned long fid)
{
#ifdef ZALLOC
    return zalloc_find_next_pba(d, t, fid, 0);
#elif defined(VIRTUAL_GROUPS)
    return isd_find_next_pba(d, t, fid);
#else
    return native_find_next_pba(d, t, fid);
#endif
}

int pba_read(struct disk *d, b_table_head_t *h)
{
    size_t sum = 0;                                                                                         // 宣告sum = 0
    bb_head_t *bth = &h->block_head;                                                                        // bth = h->block_head
    sum = bth->size;                                                                                        // sum = h->block_head的大小(size)
    struct time_size *t_s = (recording_mode == normal_op_mode) ? &d->report.normal : &d->report.journaling; // 宣告一個t_s，如果recording_mode == normal_op_mode的話就 = d->report.normal，如果不是的話就是d->report.journaling
    t_s->total_read_block_size += sum * BLOCK_SIZE;                                                         // 把t_s中的total_read_actual_size加上sum * SECTOR_SIZE，也就是加上這次實際總共讀的大小
    return sum;                                                                                             // 最後把sum return
}

int pba_write(struct disk *d, b_table_head_t *h)
{
    /* clear block data */
    bb_head_t *bbh = &h->block_head;                                                                        // bbh = h->block_head
    size_t sum = bbh->size;                                                                                 // 宣告sum直接等於h->block_head的size
    struct time_size *t_s = (recording_mode == normal_op_mode) ? &d->report.normal : &d->report.journaling; // 宣告一個t_s，如果recording_mode == normal_op_mode的話就 = d->report.normal，如果不是的話就是d->report.journaling
    for (size_t i = 0; i < bbh->size; i++)                                                                  // 跑迴圈，跑所有的block table裡面的entry
    {
        unsigned long pba = bbh->table[i].pba; // pba = 當下這個迴圈的h->block_head.table的pba
        chs_write(d, pba);                     // 呼叫chs_write這個function
    }
    t_s->total_write_block_size += sum * BLOCK_SIZE; // 把t_s中的total_write_actual_size加上sum * SECTOR_SIZE，也就是加上這次實際總共寫的大小
    return sum;                                      // 最後把sum return
}

int pba_delete(struct disk *d, b_table_head_t *h)
{
    bb_head_t *bbh = &h->block_head;       // bbh = h->block_head
    size_t sum = bbh->size;                // 宣告sum直接等於h->block_head的size
    for (size_t i = 0; i < bbh->size; i++) // 跑迴圈，跑所有的block table裡面的entry
    {
        unsigned long pba = bbh->table[i].pba; // pba = 當下這個迴圈的h->block.head.table的pba
        // printf("pba=%llu, %s\n", pba, d->storage[pba].hash);
        chs_delete(d, pba); // 呼叫chs_write這個function
    }
    d->report.total_delete_write_block_size += sum * BLOCK_SIZE; // d->report.total_delete_write_actual_size再加上sum * SECTOR_SIZE，也就是加上這次實際總共刪除的大小
    return sum;                                                  // 最後把sum return
}
// 透過傳進去的pba來檢查該storage是否為valid
bool is_storage_data_valid(struct disk *d, unsigned long pba)
{
    return d->storage[pba].status == status_in_use; // 如果d->storage[pba].status == status_in_us，return 1
}
// 透過傳進去的lba、pba、fid來更新logic_to_physical_table
void update_ltp_table(struct disk *d, unsigned long lba, unsigned pba, unsigned long fid)
{
    d->ltp_table_head->table[lba].pba = pba;
    d->ltp_table_head->table[lba].valid = true;
    d->ltp_table_head->table[lba].fid = fid;
}
// 用pba來尋找top buffer address
unsigned long pba_to_tba(struct disk *d, unsigned long pba)
{
    return d->ptt_table_head->table[pba].tba;
}
// 檢查lba是不是valid，如果是valid的話透過*p來接找到的pba
bool is_lba_valid(struct disk *d, unsigned long lba, unsigned long fid, unsigned long *p)
{
    unsigned long pba = lba_to_pba(d, lba);                                 // 透過lba_to_pba這個function來找到對應的pba
    *p = pba;                                                               // 把pba回傳給p
    if (is_ltp_mapping_valid(d, lba, fid) && is_storage_data_valid(d, pba)) // 如果is_ltp_mapping_valid跟is_storage_data_valid這兩個function是true的話，這個function就return true
    {
#ifdef TOP_BUFFER
        block_type_t type = d->ptt_table_head->table[pba].type;
        unsigned long track = lba_to_track(pba);
        switch (type)
        {
        // 如果type是top buffer或者是block swap的話，把pba轉成對應的tba回傳給p
        case top_buffer_type:
        case block_swap_type:
            *p = pba_to_tba(d, pba);
            return true;
        // 如果是buffered_type的話回傳原本的pba
        case buffered_type:
            return true;
        case normal_type:
            // 如果該track是top track或者zalloc的phase在第一階段的話回傳原本的pba
            if (is_toptrack(track))
                return true;
            else if (d->zinfo.phases == zalloc_phase1)
                return true;
            // 如果enable_top_buffer==true的話
            else if (enable_top_buffer)
            {
                *p = run_top_buffer(d, pba);
                return true;
            }
            break;
        default:
            fprintf(stderr, "Error: block type is invalid.\n");
            exit(EXIT_FAILURE);
            break;
        }
#endif
        return true;
    }
    return false; // 不是的話就return false
}

unsigned long pba_search_jfs(struct disk *d, unsigned long lba, unsigned long fid)
{
    unsigned long pba;
    if (is_lba_valid(d, lba, fid, &pba)) // 如果is_lba_valid是true的話，就return pba
        return pba;
    pba = find_next_pba_jfs(d, lba, fid); // 如果is_lba_valid是false的話，透過lba_to_sector這個macro來找到lba對應的sector
    update_ltp_table(d, lba, pba, fid);   // 再透過update_ltp_table去更新lba,pba,fid的對應關西
    return pba;                           // 最後把pba return
}

unsigned long pba_search(struct disk *d, unsigned long lba, unsigned long fid)
{
    unsigned long pba;
    if (is_lba_valid(d, lba, fid, &pba)) // 如果is_lba_valid是true的話，就return pba
    {
#if defined(VIRTUAL_GROUPS)
        if (do_dual_swap(d, pba, &pba))
            update_ltp_table(d, lba, pba, fid);
#endif
        return pba;
    }
    pba = find_next_pba(d, lba, fid);   // 透過find_next_pba_jfs這個function來找到對應的pba
    update_ltp_table(d, lba, pba, fid); // 再透過update_ltp_table去更新lba,pba,fid的對應關西
    return pba;                         // 最後把pba return
}

unsigned long lba_to_tba(struct disk *d, unsigned long lba)
{
    unsigned long pba = d->ltp_table_head->table[lba].pba; // 透過lba去找到對應的pba
    unsigned long tba = d->ptt_table_head->table[pba].tba; // 透過pba去找到對應的tba
    if (d->ptt_table_head->table[pba].type == normal_type) // 用pba來去看ptt_table_head對應pba(index)的type是不是normal_type
        return pba;                                        // 如果是的話 return pba
    else
        return tba; // 如果不是的話就return tba
}
// lba轉pba
unsigned long lba_to_pba(struct disk *d, unsigned long lba)
{
    return d->ltp_table_head->table[lba].pba;
}
#ifdef DEDU_ORIGIN
bool DEDU_is_ltp_mapping_valid(struct disk *d, unsigned long lba, char *hash)
{
    struct ltp_entry *e = &d->ltp_table_head->table[lba];
    return e->valid;
    // return ((e->valid) && strcmp(hash, e->hash) == 0);
}
void DEDU_delete_ltp_table(struct disk *d, unsigned long lba)
{
    d->ltp_table_head->table[lba].pba = 0;
}
void dedupe_update_ltp_table(struct disk *d, unsigned long lba, unsigned pba, char *hash)
{
    d->ltp_table_head->table[lba].pba = pba;
    d->ltp_table_head->table[lba].valid = true;
    d->ltp_table_head->table[lba].trim = false;
    strcpy(d->ltp_table_head->table[lba].hash, hash);
}
unsigned long DEDU_native_find_next_pba(struct disk *d, unsigned long t, char *hash)
{
    d->storage[d->dinfo.current_track].status = status_booked;
    d->dinfo.current_track++;
    return d->dinfo.current_track - 1;
}
unsigned long dedupe_find_next_pba(struct disk *d, unsigned long t, char *hash, bool dedupe)
{

#ifdef ZALLOC
    return zalloc_find_next_pba(d, t, 0, dedupe);
#else
    return DEDU_native_find_next_pba(d, t, hash);
#endif
}
bool DEDU_is_lba_trimed(struct disk *d, unsigned long lba, char *hash, unsigned long *p)
{
    unsigned long pba = lba_to_pba(d, lba);
    *p = pba;
    if (d->ltp_table_head->table[lba].trim)
        return true;
    return false;
}
// 檢查lba是不是valid，如果是valid的話透過*p來接找到的pba
bool DEDU_is_lba_valid(struct disk *d, unsigned long lba, char *hash, unsigned long *p)
{
    unsigned long pba = lba_to_pba(d, lba); // 透過lba_to_pba這個function來找到對應的pba
    *p = pba;                               // 把pba回傳給p
    /*
    (pba == status_in_use) && (ltp.valid == true) && (ltp.hash == hash)
    */
    if (is_storage_data_valid(d, pba) && DEDU_is_ltp_mapping_valid(d, lba, hash))
    {
        // #ifdef TOP_BUFFER
        //         block_type_t type = d->ptt_table_head->table[pba].type;
        //         unsigned long track = pba;
        //         // unsigned long track = lba_to_track(pba);
        //         switch (type)
        //         {
        //         // 如果type是top buffer或者是block swap的話，把pba轉成對應的tba回傳給p
        //         case top_buffer_type:
        //         case block_swap_type:
        //             *p = pba_to_tba(d, pba);
        //             return true;
        //         // 如果是buffered_type的話回傳原本的pba
        //         case buffered_type:
        //             return true;
        //         case normal_type:
        //             // 如果該track是top track或者zalloc的phase在第一階段的話回傳原本的pba
        //             if (is_toptrack(track))
        //                 return true;
        //             else if (d->zinfo.phases == zalloc_phase1)
        //                 return true;
        //             // 如果enable_top_buffer==true的話
        //             else if (enable_top_buffer)
        //             {
        //                 *p = run_top_buffer(d, pba);
        //                 return true;
        //             }
        //             break;
        //         default:
        //             fprintf(stderr, "Error: block type is invalid.\n");
        //             exit(EXIT_FAILURE);
        //             break;
        //         }
        // #endif
        return true;
    }
    return false; // 不是的話就return false
}
// #ifndef NO_DEDU
bool is_in_storage(struct disk *d, char *hash, unsigned long *pba)
{
#ifdef ZALLOC
#ifdef NEW_ALLOC
    for (uint64_t i = 0; i < d->report.max_block_num; i++)
    {
        if (strcmp(d->storage[i].hash, hash) == 0)
        {
            *pba = i;
            return true;
        }
    }
    return false;
#else
    zalloc_phases phase = d->zinfo.phases;
    uint64_t phase1_total_block_num = (d->report.max_track_num % 2 == 0) ? (d->report.max_track_num / 2) : (d->report.max_track_num / 2 + 1);
    if (phase == zalloc_phase1)
    {
        for (size_t i = d->zinfo.phase1_start, num = 0; num < phase1_total_block_num && num < d->report.current_use_block_num; i += 2, num++)
        {
            // if (d->storage[i].status != status_in_use)
            //     continue;
            if (strcmp(d->storage[i].hash, hash) == 0)
            {
                *pba = i;
                return true;
            }
        }
    }
    else
    {
        for (uint64_t i = 0; i < d->report.max_block_num; i++)
        {
            // if (d->storage[i].status != status_in_use)
            //     continue;
            if (strcmp(d->storage[i].hash, hash) == 0)
            {
                *pba = i;
                return true;
            }
        }
    }
    return false;
#endif
#else
    for (uint64_t i = 0; i < d->report.current_use_block_num; i++)
    {
        if (d->storage[i].status != status_in_use)
            continue;
        if (strcmp(d->storage[i].hash, hash) == 0)
        {
            *pba = i;
            return true;
        }
    }
    return false;
#endif
}
// #endif

#ifdef ZALLOC
unsigned long find_swapped_pba(struct disk *d, char *hash, unsigned long pba, int line_cnt)
{
    zalloc_phases phase = d->zinfo.phases;
    uint64_t phase1_total_block_num = (d->report.max_track_num % 2 == 0) ? (d->report.max_track_num / 2) : (d->report.max_track_num / 2 + 1);
    if (phase == zalloc_phase1)
    {
        for (size_t i = d->zinfo.phase1_start, num = 0; num < phase1_total_block_num && num < d->report.current_use_block_num; i += 2, num++)
        {
            if (d->storage[i].status != status_in_use)
                continue;
            if (strcmp(d->storage[i].hash, hash) == 0)
            {
                return i;
            }
        }
    }
    else
    {
        if (line_cnt == 677947)
        {
            printf("find_swapped_pba_called.\n");
        }
        for (uint64_t i = 0; i < d->report.max_block_num; i++)
        {
            if (d->storage[i].status != status_in_use)
                continue;
            if (strcmp(d->storage[i].hash, hash) == 0)
            {
                if (line_cnt == 677947)
                {
                    printf("find_swapped_pba_successed.\n");
                }
                return i;
            }
        }
    }
    if (line_cnt == 677947)
    {
        printf("find_swapped_pba_failed.\n");
    }
    return d->report.max_block_num + 100;
}
#endif

unsigned long DEDU_update(struct disk *d, unsigned long lba, unsigned long pba, char *hash)
{
    if (!is_toptrack(pba)) // 如果是 bottom track 的話
    {
        unsigned long prev_track = pba - 1;
        unsigned long next_track = pba + 1;
        unsigned long tba;
        if (d->storage[prev_track].status == status_in_use || d->storage[next_track].status == status_in_use)
        {
            if (d->storage[pba].referenced_count == 0)
            // if (true)
            {
                tba = DEDU_run_block_swap(d, pba);
                if ((signed)tba != -1)
                {
                    dedupe_update_ltp_table(d, lba, tba, hash); // 改
                    return tba;
                }
            }
        }
    }
    return pba;
}
void do_some_top_buffer_stuff(struct disk *d, unsigned long lba, unsigned long *p)
{
#ifdef TOP_BUFFER
    unsigned long pba = lba_to_pba(d, lba);
    *p = pba;

    block_type_t type = d->ptt_table_head->table[pba].type;
    unsigned long track = pba;
    // unsigned long track = lba_to_track(pba);
    switch (type)
    {
    // 如果type是top buffer或者是block swap的話，把pba轉成對應的tba回傳給p
    case top_buffer_type:
    case block_swap_type:
        *p = pba_to_tba(d, pba);
        break;
    // 如果是buffered_type的話回傳原本的pba
    case buffered_type:
        break;
    case normal_type:
        // 如果該track是top track或者zalloc的phase在第一階段的話回傳原本的pba
        if (is_toptrack(track))
            break;
        else if (d->zinfo.phases == zalloc_phase1)
            break;
        // 如果enable_top_buffer==true的話
        else if (enable_top_buffer)
        {
            *p = run_top_buffer(d, pba);
            break;
        }
        break;
    default:
        fprintf(stderr, "Error: block type is invalid.\n");
        exit(EXIT_FAILURE);
        break;
    }
#endif
}

unsigned long dedupe_pba_search(struct disk *d, unsigned long lba, char *hash, bool *pass, int line_cnt, bool dedupe)
{
    // printf("line cnt= %d\n", line_cnt);
    unsigned long pba;
    /*
        以下 flow control 為判斷：
        如果是被刪除的資料或是已經存在的資料，則進行特定操作
        （刪除的資料復原，存在的資料更新）

        DEDU_is_lba_trimed: assign pba in ltp_table[lba] to pba, and return whether ltp_entry is trimed.
        DEDU_is_lba_valid:  assign pba in ltp_table[lba] to pba,
                            and return whether block[pba] is in use,
                            and ltp_entry is valid.
    */
    if (DEDU_is_lba_trimed(d, lba, hash, &pba) && !(DEDU_is_lba_valid(d, lba, hash, &pba)))
    {
#ifndef NO_DEDU
        // 若 input hash 與 ltp_entry 中的 hash 不同，則代表要重新寫入
        if (strcmp(d->ltp_table_head->table[lba].hash, hash) != 0)
        {
#ifdef DEDU_WRITE
            pba = DEDU_update(d, lba, pba, hash);

            // 如果 DEDU_update 沒有執行 block swap 的話，
            // 要另行將新的 hash 寫入 ltp_entry 中
            if (strcmp(d->ltp_table_head->table[lba].hash, hash) != 0)
            {
                dedupe_update_ltp_table(d, lba, pba, hash);
            }
#else
            do_some_top_buffer_stuff(d, lba, &pba);
            dedupe_update_ltp_table(d, lba, pba, hash);
#endif
        }
        else
        {
            // 檢查 lba 被刪除過後，該 lba 對應的 pba 是否有被 swap 過
            if (strcmp(hash, d->storage[pba].hash) != 0)
            {
                // 重新定位 lba 對應的 pba
                unsigned long temp = d->report.max_block_num;
                temp = find_swapped_pba(d, hash, temp, line_cnt);
                if (temp < d->report.max_block_num)
                {
                    pba = temp;
                }
                if (d->storage[pba].status != status_trimed)
                {
                    d->storage[pba].referenced_count++;
                }
            }
            dedupe_update_ltp_table(d, lba, pba, hash);
            return pba;
        }
#endif
        dedupe_update_ltp_table(d, lba, pba, hash);
        return pba;
    }
#ifdef DEDU_WRITE
    // 如果 ltp_entry 已有資料，且目前是 valid
    else if (!(DEDU_is_lba_trimed(d, lba, hash, &pba)) && DEDU_is_lba_valid(d, lba, hash, &pba))
    {
        // 若 input 與 ltp_entry 中的 hash 不同，代表要更新；反之，則略過
        if (strcmp(d->ltp_table_head->table[lba].hash, hash) != 0)
        {
            pba = DEDU_update(d, lba, pba, hash);

            // 如果 DEDU_update 沒有執行 block swap 的話，
            // 要另行將新的 hash 寫入 ltp_entry 中
            if (strcmp(d->ltp_table_head->table[lba].hash, hash) != 0)
            {
                dedupe_update_ltp_table(d, lba, pba, hash);
            }
        }
        else
        {
            *pass = true;
        }
        return pba;
    }
#else
    else if (!(DEDU_is_lba_trimed(d, lba, hash, &pba)) && DEDU_is_lba_valid(d, lba, hash, &pba))
    {
#ifndef NO_DEDU
        if (strcmp(d->ltp_table_head->table[lba].hash, hash) != 0)
        {
            do_some_top_buffer_stuff(d, lba, &pba);
            dedupe_update_ltp_table(d, lba, pba, hash);
        }
        else
        {
            *pass = true;
        }
#endif
        return pba;
    }

#endif

#ifndef NO_DEDU
    // ltp_entry 沒有被 trim 也不是 valid 的情況：lba 第一次被寫入
    // 檢查 hash 是否已存在於 disk，是的話就將 pba 設為該 block
    bool is_in_storage_flag = is_in_storage(d, hash, &pba);
    // 檢查 ltp_entry 是否 valid
    bool is_ltp_mapping_flag = DEDU_is_ltp_mapping_valid(d, lba, hash);

    // lba 第一次被寫入，且 hash 值已存在於 pba 中
    if (is_in_storage_flag && !is_ltp_mapping_flag)
    {
        dedupe_update_ltp_table(d, lba, pba, hash);
        // 檢查存在該 hash 的 pba 是否為 trimed，若不是，則 referenced_count++
        if (d->storage[pba].status != status_trimed)
        {
            d->storage[pba].referenced_count++;
        }
        return pba;
    }
#endif

    // lba 與 hash 皆第一次被寫入
    pba = dedupe_find_next_pba(d, lba, hash, dedupe);
    dedupe_update_ltp_table(d, lba, pba, hash);
#ifdef NEW_ALLOC
    if (d->zinfo.phases == zalloc_phase1)
    {
    }
#endif
    return pba;
}
#endif
