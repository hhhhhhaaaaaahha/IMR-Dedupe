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
// #include <iostream>
// using namespace std;

bool phase1_is_full = false;
bool initialized = false;
#ifdef DEDU_ORIGIN
extern FILE *fp;
#endif
extern jmp_buf env;

#ifdef ZALLOC
unsigned long zalloc_find_next_pba(struct disk *d, unsigned long t, unsigned long fid)
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
            /* sequential search */
            if (phase1_is_full)
            {
                for (size_t i = zinfo->phase1_start; i <= zinfo->phase1_end; i += 2)
                {
                    pba = i;
                    if (storage_is_free(d, i))
                    {
                        d->storage[pba].status = status_booked;
                        zinfo->current_track = i;
                        phase1_is_full = false;
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
                phase1_is_full = true;
            }
            else
            {
                zinfo->current_track += 2;
            }
            break;
        case zalloc_phase2:
            if (track == zinfo->phase2_end)
            {
                printf("\nPhase3!\n");
                fprintf(fp, "\nPhase3!\n");
                zinfo->current_track = zinfo->phase3_start;
                zinfo->phases = zalloc_phase3;
            }
            // ?????????????????????????????????top track
            else
            {
                zinfo->current_track -= 4;
            }
            break;

        case zalloc_phase3:
            // ??????track>?????????track???????????????disk????????????
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
                    continue;
                }
#endif
                perror("Error: disk is full. Can't find next empty block.");
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
            // ?????????????????????top_buffer_count>0?????????????????????scp
            if (d->report.current_top_buffer_count)
            {
                scp(d);
                // d->zinfo.current_track = current_scp_track;
                continue;
            }
#endif
            perror("Error: disk is full. Can't find next empty block.");
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
        if (storage_is_free(d, pba))
        {
            d->storage[pba].status = status_booked;
            return pba;
        }
    }
    return -1;
}
#endif
// ??????????????????block???status??????status_booked????????????block???index??????
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
// ???native_find_next_pba????????????
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
    return zalloc_find_next_pba(d, t, fid);
#elif defined(ZALLOC)
    return zalloc_journal_get_block(d, t, fid);
#else
    return native_journal_get_block(d, t, fid); //
#endif
}

unsigned long find_next_pba(struct disk *d, unsigned long t, unsigned long fid)
{
#ifdef ZALLOC
    return zalloc_find_next_pba(d, t, fid);
#elif defined(VIRTUAL_GROUPS)
    return isd_find_next_pba(d, t, fid);
#else
    return native_find_next_pba(d, t, fid);
#endif
}

int pba_read(struct disk *d, b_table_head_t *h)
{
    size_t sum = 0;                                                                                         // ??????sum = 0
    bb_head_t *bth = &h->block_head;                                                                        // bth = h->block_head
    sum = bth->size;                                                                                        // sum = h->block_head?????????(size)
    struct time_size *t_s = (recording_mode == normal_op_mode) ? &d->report.normal : &d->report.journaling; // ????????????t_s?????????recording_mode == normal_op_mode????????? = d->report.normal???????????????????????????d->report.journaling
    t_s->total_read_block_size += sum * BLOCK_SIZE;                                                         // ???t_s??????total_read_actual_size??????sum * SECTOR_SIZE????????????????????????????????????????????????
    return sum;                                                                                             // ?????????sum return
}

int pba_write(struct disk *d, b_table_head_t *h)
{
    /* clear block data */
    bb_head_t *bbh = &h->block_head;                                                                        // bbh = h->block_head
    size_t sum = bbh->size;                                                                                 // ??????sum????????????h->block_head???size
    struct time_size *t_s = (recording_mode == normal_op_mode) ? &d->report.normal : &d->report.journaling; // ????????????t_s?????????recording_mode == normal_op_mode????????? = d->report.normal???????????????????????????d->report.journaling
    for (size_t i = 0; i < bbh->size; i++)                                                                  // ????????????????????????block table?????????entry
    {
        unsigned long pba = bbh->table[i].pba; // pba = ?????????????????????h->block_head.table???pba
        chs_write(d, pba);                     // ??????chs_write??????function
    }
    t_s->total_write_block_size += sum * BLOCK_SIZE; // ???t_s??????total_write_actual_size??????sum * SECTOR_SIZE????????????????????????????????????????????????
    return sum;                                      // ?????????sum return
}

int pba_delete(struct disk *d, b_table_head_t *h)
{
    bb_head_t *bbh = &h->block_head;       // bbh = h->block_head
    size_t sum = bbh->size;                // ??????sum????????????h->block_head???size
    for (size_t i = 0; i < bbh->size; i++) // ????????????????????????block table?????????entry
    {
        unsigned long pba = bbh->table[i].pba; // pba = ?????????????????????h->block.head.table???pba
        chs_delete(d, pba);                    // ??????chs_write??????function
    }
    d->report.total_delete_write_block_size += sum * BLOCK_SIZE; // d->report.total_delete_write_actual_size?????????sum * SECTOR_SIZE???????????????????????????????????????????????????
    return sum;                                                  // ?????????sum return
}
// ??????????????????pba????????????storage?????????valid
bool is_storage_data_valid(struct disk *d, unsigned long pba)
{
    return d->storage[pba].status == status_in_use; // ??????d->storage[pba].status == status_in_us???return 1
}
// ??????????????????lba???pba???fid?????????logic_to_physical_table
void update_ltp_table(struct disk *d, unsigned long lba, unsigned pba, unsigned long fid)
{
    d->ltp_table_head->table[lba].pba = pba;
    d->ltp_table_head->table[lba].valid = true;
    d->ltp_table_head->table[lba].fid = fid;
}
// ???pba?????????top buffer address
unsigned long pba_to_tba(struct disk *d, unsigned long pba)
{
    return d->ptt_table_head->table[pba].tba;
}
// ??????lba?????????valid????????????valid????????????*p???????????????pba
bool is_lba_valid(struct disk *d, unsigned long lba, unsigned long fid, unsigned long *p)
{
    unsigned long pba = lba_to_pba(d, lba);                                 // ??????lba_to_pba??????function??????????????????pba
    *p = pba;                                                               // ???pba?????????p
    if (is_ltp_mapping_valid(d, lba, fid) && is_storage_data_valid(d, pba)) // ??????is_ltp_mapping_valid???is_storage_data_valid?????????function???true???????????????function???return true
    {
#ifdef TOP_BUFFER
        block_type_t type = d->ptt_table_head->table[pba].type;
        unsigned long track = lba_to_track(pba);
        switch (type)
        {
        // ??????type???top buffer?????????block swap????????????pba???????????????tba?????????p
        case top_buffer_type:
        case block_swap_type:
            *p = pba_to_tba(d, pba);
            return true;
        // ?????????buffered_type?????????????????????pba
        case buffered_type:
            return true;
        case normal_type:
            // ?????????track???top track??????zalloc???phase????????????????????????????????????pba
            if (is_toptrack(track))
                return true;
            else if (d->zinfo.phases == zalloc_phase1)
                return true;
            // ??????enable_top_buffer==true??????
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
    return false; // ???????????????return false
}

unsigned long pba_search_jfs(struct disk *d, unsigned long lba, unsigned long fid)
{
    unsigned long pba;
    if (is_lba_valid(d, lba, fid, &pba)) // ??????is_lba_valid???true????????????return pba
        return pba;
    pba = find_next_pba_jfs(d, lba, fid); // ??????is_lba_valid???false???????????????lba_to_sector??????macro?????????lba?????????sector
    update_ltp_table(d, lba, pba, fid);   // ?????????update_ltp_table?????????lba,pba,fid???????????????
    return pba;                           // ?????????pba return
}

unsigned long pba_search(struct disk *d, unsigned long lba, unsigned long fid)
{
    unsigned long pba;
    if (is_lba_valid(d, lba, fid, &pba)) // ??????is_lba_valid???true????????????return pba
    {
#if defined(VIRTUAL_GROUPS)
        if (do_dual_swap(d, pba, &pba))
            update_ltp_table(d, lba, pba, fid);
#endif
        return pba;
    }
    pba = find_next_pba(d, lba, fid);   // ??????find_next_pba_jfs??????function??????????????????pba
    update_ltp_table(d, lba, pba, fid); // ?????????update_ltp_table?????????lba,pba,fid???????????????
    return pba;                         // ?????????pba return
}

unsigned long lba_to_tba(struct disk *d, unsigned long lba)
{
    unsigned long pba = d->ltp_table_head->table[lba].pba; // ??????lba??????????????????pba
    unsigned long tba = d->ptt_table_head->table[pba].tba; // ??????pba??????????????????tba
    if (d->ptt_table_head->table[pba].type == normal_type) // ???pba?????????ptt_table_head??????pba(index)???type?????????normal_type
        return pba;                                        // ??????????????? return pba
    else
        return tba; // ?????????????????????return tba
}
// lba???pba
unsigned long lba_to_pba(struct disk *d, unsigned long lba)
{
    return d->ltp_table_head->table[lba].pba;
}
#ifdef DEDU_ORIGIN
bool DEDU_is_ltp_mapping_valid(struct disk *d, unsigned long lba, char *hash)
{
    struct ltp_entry *e = &d->ltp_table_head->table[lba];
    return ((e->valid) && strcmp(hash, e->hash) == 0);
}
void DEDU_delete_ltp_table(struct disk *d, unsigned long lba)
{
    d->ltp_table_head->table[lba].pba = 0;
}
void DEDU_update_ltp_table(struct disk *d, unsigned long lba, unsigned pba, char *hash)
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
unsigned long DEDU_find_next_pba(struct disk *d, unsigned long t, char *hash)
{

#ifdef ZALLOC
    return zalloc_find_next_pba(d, t, 0);
#else
    return DEDU_native_find_next_pba(d, t, hash);
#endif
}
bool DEDU_is_lba_trimed(struct disk *d, unsigned long lba, char *hash, unsigned long *p)
{
    unsigned long pba = lba_to_pba(d, lba);
    *p = pba;
    if (DEDU_is_ltp_mapping_valid(d, lba, hash) && d->ltp_table_head->table[lba].trim)
        return true;
    return false;
}
// ??????lba?????????valid????????????valid????????????*p???????????????pba
bool DEDU_is_lba_valid(struct disk *d, unsigned long lba, char *hash, unsigned long *p)
{
    unsigned long pba = lba_to_pba(d, lba); // ??????lba_to_pba??????function??????????????????pba
    *p = pba;                               // ???pba?????????p
    /*
    if????????????pba != 0 ????????????????????????lba??????lba_to_pba???function?????????
    pba????????????0(?????????)???????????????????????????????????????lba?????????pba = 0
    */
    if (is_storage_data_valid(d, pba) && DEDU_is_ltp_mapping_valid(d, lba, hash)) // ??????is_ltp_mapping_valid???is_storage_data_valid?????????function???true???????????????function???return true
    {
#ifdef TOP_BUFFER
        block_type_t type = d->ptt_table_head->table[pba].type;
        unsigned long track = lba_to_track(pba);
        switch (type)
        {
        // ??????type???top buffer?????????block swap????????????pba???????????????tba?????????p
        case top_buffer_type:
        case block_swap_type:
            *p = pba_to_tba(d, pba);
            return true;
        // ?????????buffered_type?????????????????????pba
        case buffered_type:
            return true;
        case normal_type:
            // ?????????track???top track??????zalloc???phase????????????????????????????????????pba
            if (is_toptrack(track))
                return true;
            else if (d->zinfo.phases == zalloc_phase1)
                return true;
            // ??????enable_top_buffer==true??????
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
    return false; // ???????????????return false
}
#ifndef NO_DEDU
bool is_in_storage(struct disk *d, char *hash, unsigned long *pba)
{
#ifdef ZALLOC
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
                *pba = i;
                return true;
            }
        }
    }
    else
    {
        for (uint64_t i = 0; i < d->report.max_block_num; i++)
        {
            if (d->storage[i].status != status_in_use)
                continue;
            if (strcmp(d->storage[i].hash, hash) == 0)
            {
                *pba = i;
                return true;
            }
        }
    }
    return false;
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
#endif
unsigned long DEDU_update(struct disk *d, unsigned long lba, unsigned long pba, char *hash)
{
    if (!is_toptrack(pba))
    {
        unsigned long prev_track = pba - 1;
        unsigned long next_track = pba + 1;
        unsigned long tba;
        if (d->storage[prev_track].status == status_in_use || d->storage[next_track].status == status_in_use)
        {
            if (d->storage[pba].referenced_count == 0)
            {
                tba = DEDU_run_block_swap(d, pba);
                if ((signed)tba != -1)
                {
                    DEDU_update_ltp_table(d, lba, tba, hash);
                    return tba;
                }
            }
        }
    }
    return pba;
}

unsigned long DEDU_pba_search(struct disk *d, unsigned long lba, char *hash)
{
    unsigned long pba;
    if (DEDU_is_lba_trimed(d, lba, hash, &pba) || DEDU_is_lba_valid(d, lba, hash, &pba))
    {
#ifdef DEDU_WRITE
        pba = DEDU_update(d, lba, pba, hash);
        return pba;
#endif
    }
#ifndef NO_DEDU
    bool is_in_storage_flag = is_in_storage(d, hash, &pba);
    bool is_ltp_mapping_flag = DEDU_is_ltp_mapping_valid(d, lba, hash);
    if (is_in_storage_flag && !is_ltp_mapping_flag)
    {
        DEDU_update_ltp_table(d, lba, pba, hash);
        d->storage[pba].referenced_count++;
        return pba;
    }
#endif
    pba = DEDU_find_next_pba(d, lba, hash);   // ??????find_next_pba_jfs??????function??????????????????pba
    DEDU_update_ltp_table(d, lba, pba, hash); // ?????????update_ltp_table?????????lba,pba,fid???????????????
    return pba;                               // ?????????pba return
}
#endif
