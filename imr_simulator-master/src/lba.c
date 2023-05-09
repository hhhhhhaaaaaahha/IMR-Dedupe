
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "output.h"
#include "active_swap.h"
#include "batch.h"
#include "lba.h"
#include "pba.h"
#include "record_op.h"
#ifdef BLOCK_SWAP
#include "block_swap.h"
#endif
#ifdef TOP_BUFFER
#include "scp.h"
#include "top_buffer.h"
#endif
#ifdef VIRTUAL_GROUPS
#include "fid_table.h"
#include "virtual_groups.h"
#endif
#ifdef DEDU_WRITE
#include "scp.h"
#include "top_buffer.h"
#include "block_swap.h"
#endif
// #include <iostream>
// using namespace std;
extern bool is_bottom_has_trimed_track;
bool enable_top_buffer = false;
bool enable_block_swap = false;
extern jmp_buf env;
struct disk_operations imr_ops =
    {
        .read = lba_read,
        .write = lba_write,
        .remove = lba_delete,
        .journaling_write = journaling_write,
        .invalid = lba_invalid,
        .DEDU_write = DEDU_lba_write,
        .DEDU_remove = DEDU_Trim};

int init_disk(struct disk *disk, int physical_size, int logical_size)
{
    void *p;
    struct report *report = &disk->report;
    unsigned long block_num = 0;
#ifdef ZALLOC
    unsigned long max_track_index, phase1_start, phase1_end, phase2_start, phase2_end, phase3_start, phase3_end;
#ifdef DEDU_WRITE
    unsigned long phase4_start, phase4_end;
#endif
#endif

    assert(disk);
    assert(physical_size > 0);

    /* Initialize struct report */
    memset(&disk->report, 0, sizeof(disk->report));
    /* Initialize track info */
    unsigned long total_tracks;
    uint64_t total_byte = GB_TO_BYTE(physical_size);
    total_tracks = total_byte / TRACK_SIZE;
    report->max_track_num = total_tracks;
    block_num = total_tracks;
#ifdef DEDU_ORIGIN
    uint64_t logical_byte = GB_TO_BYTE(logical_size);
    uint64_t logical_block_num = logical_byte / BLOCK_SIZE;
    report->max_logical_block_num = logical_block_num;
#endif
    /* Initialize disk storage */
    p = malloc(block_num * sizeof(*disk->storage));
    if (!p)
    {
        goto done_disk_storage;
    }
    memset(p, 0, block_num * sizeof(*disk->storage));
    disk->storage = (struct block *)p;
    for (uint64_t i = 0; i < block_num; i++)
    {
        disk->storage[i].lba_capacity = 2;
        disk->storage[i].lba = (unsigned long *)malloc(sizeof(unsigned long) * disk->storage[i].lba_capacity);
    }

    /* Initialize lba to pba table head*/
    p = malloc(sizeof(*disk->ltp_table_head));
    if (!p)
    {
        goto done_ltp_table_head;
    }
    memset(p, 0, sizeof(*disk->ltp_table_head));
    disk->ltp_table_head = (struct ltp_table_head *)p;

    /* Initialize lba to pba table */
#ifdef DEDU_ORIGIN
    p = malloc(logical_block_num * sizeof(struct ltp_entry));
    if (!p)
    {
        goto done_ltp_table;
    }
    memset(p, 0, logical_block_num * sizeof(struct ltp_entry));
#endif
    disk->ltp_table_head->table = (struct ltp_entry *)p;

    report->max_block_num = block_num;
#ifdef TOP_BUFFER
    report->max_top_buffer_num = TOP_BUFFER_CAPACITY(report->max_block_num);
#endif
#ifdef BLOCK_SWAP
    report->block_swap_boundary = report->max_block_num - report->max_top_buffer_num;
#elif defined DEDU_WRITE
    report->block_swap_boundary = report->max_block_num - report->max_top_buffer_num;
#endif
    disk->dinfo.current_track = 0;
#ifdef ZALLOC
    /* Initialize zalloc info */
    phase1_end = phase2_start = phase2_end = phase3_start = phase3_end = 0;
#if defined(JFS) && !defined(ZALLOC_JOURNAL_VIRTUAL)
    phase1_start = (report->max_track_num / 100) * 2;
#else
    phase1_start = 0;
#endif
    /*
    phase1是寫所有的bottom_track
    phase1_start = 0(第一個track，也就是bottom track)
    如果最後一個track是top track的話，phase1_end就會是最後一個track的前一個，phase2_start就會是最後一個track(最後一個top track)
    如果最後一個track是bottom track的話，phase1_end就會是最後一個track，phase2_start就會是最後一個track的前一個(最後一個top track)
    */
    max_track_index = report->max_track_num - 1;
    // 這邊是依據最後一個是不是top track來決定phase1_end跟phase2_start
    if (is_toptrack(max_track_index))
    {
        phase1_end = max_track_index - 1;
        phase2_start = max_track_index;
#ifdef DEDU_WRITE
        phase4_end = max_track_index;
#endif
    }
    else
    {
#ifdef DEDU_WRITE
        phase4_end = max_track_index - 1;
#endif
        phase1_end = max_track_index;
        phase2_start = max_track_index - 1;
    }

    phase2_end = phase1_start + (phase2_start % 4);
    // 如果phase2_end == 第一個top track的話，phase3_start = 第二個top track開始
    if (phase2_end == (phase1_start + 1))
    {
        phase3_start = phase1_start + 3;
    }
    // 如果phase2_end != 第一個top track的話，phase3_start = 第一個top track開始
    else
    {
        phase3_start = phase1_start + 1;
    }
    // (max_track_index - phase3_start) 跟 3的補數做AND再跟phase3_start 相加
    phase3_end = phase3_start + ((max_track_index - phase3_start) & ~0x3UL);

    disk->zinfo.phase1_start = phase1_start;
    disk->zinfo.phase1_end = phase1_end;
    disk->zinfo.phase2_start = phase2_start;
    disk->zinfo.phase2_end = phase2_end;
    disk->zinfo.phase3_start = phase3_start;
    disk->zinfo.phase3_end = phase3_end;
    disk->zinfo.phases = zalloc_phase1;
    /* set current_track */
    disk->zinfo.current_track = phase1_start;
#ifdef DEDU_WRITE
    phase4_start = 1;
    disk->zinfo.phase4_start = phase4_start;
    disk->zinfo.phase4_end = phase4_end;
    // printf("start = %ld\n", disk->zinfo.phase4_start);
    // printf("end = %ld\n", disk->zinfo.phase4_end);
#endif
#endif

#ifdef TOP_BUFFER
    init_top_buffer(disk);

    /* Initialize lba to tba table head*/
    p = malloc(sizeof(struct ptt_table_head));
    if (!p)
    {
        goto done_ptt_table_head;
    }
    memset(p, 0, sizeof(struct ptt_table_head));
    disk->ptt_table_head = (struct ptt_table_head *)p;

    /* Initialize lba to pba table */
    p = malloc(block_num * sizeof(struct ptt_entry));
    if (!p)
    {
        goto done_ltl_table;
    }
    memset(p, 0, block_num * sizeof(struct ptt_entry));
    disk->ptt_table_head->table = (struct ptt_entry *)p;
    if (init_scp(disk))
        goto done_scp;
#elif defined DEDU_WRITE
    init_top_buffer(disk);

    /* Initialize lba to tba table head*/
    p = malloc(sizeof(struct ptt_table_head));
    if (!p)
    {
        goto done_ptt_table_head;
    }
    memset(p, 0, sizeof(struct ptt_table_head));
    disk->ptt_table_head = (struct ptt_table_head *)p;

    /* Initialize lba to pba table */
    p = malloc(block_num * sizeof(struct ptt_entry));
    if (!p)
    {
        goto done_ltl_table;
    }
    memset(p, 0, block_num * sizeof(struct ptt_entry));
    disk->ptt_table_head->table = (struct ptt_entry *)p;
    if (init_scp(disk))
        goto done_scp;
#endif
    /* Initialize btable info */
    if (init_batch_table(&gbtable))
    {
        goto done_gbtable;
    }
    if (init_batch_table(&mbtable))
    {
        goto done_mbtable;
    }

#ifdef VIRTUAL_GROUPS
    if (init_virtual_groups(disk))
        goto done_virtual_groups;
    if (init_fid_table(&g_fid_table))
        goto done_fid_table;
#endif

    disk->d_op = &imr_ops;
    return 0;
#ifdef VIRTUAL_GROUPS
done_fid_table:
    end_virtual_groups(&disk->vgh);
done_virtual_groups:
    end_batch_table(&mbtable);
#endif
done_mbtable:
    end_batch_table(&gbtable);
done_gbtable:
#ifdef TOP_BUFFER
    end_scp();
done_scp:
    free(disk->ptt_table_head->table);
done_ltl_table:
    free(disk->ptt_table_head);
done_ptt_table_head:
#elif defined DEDU_WRITE
    end_scp();
done_scp:
    free(disk->ptt_table_head->table);
done_ltl_table:
    free(disk->ptt_table_head);
done_ptt_table_head:
#endif
    free(disk->ltp_table_head->table);
done_ltp_table:
    free(disk->ltp_table_head);
done_ltp_table_head:
    free(disk->storage);
done_disk_storage:
    return -1;
}

void end_disk(struct disk *disk)
{
#ifdef TOP_BUFFER
    free(disk->ptt_table_head->table);
    free(disk->ptt_table_head);
#endif
    free(disk->ltp_table_head->table);
    free(disk->ltp_table_head);
    free(disk->storage);
    end_batch_table(&gbtable);
#ifdef VIRTUAL_GROUPS
    end_batch_table(&mbtable);
    end_virtual_groups(&disk->vgh);
#endif
    clear_info(disk);
#ifdef ZALLOC
    memset(&disk->zinfo, 0, sizeof(disk->zinfo));
#endif
    memset(&disk, 0, sizeof(disk));
    enable_block_swap = false;
    enable_top_buffer = false;
}

bool is_ltp_mapping_valid(struct disk *d, unsigned long lba, unsigned long fid)
{
    struct ltp_entry *e = &d->ltp_table_head->table[lba];
    return ((e->valid) && e->fid == fid);
}
// 檢查lba對應的physical block中的data是不是valid
bool is_block_data_valid(struct disk *d, unsigned long lba, unsigned long fid)
{
    if (!is_ltp_mapping_valid(d, lba, fid))
        return false;
#ifdef TOP_BUFFER
    // 利用lba_to_pba這個function來找到對應的tba
    unsigned long tba = lba_to_tba(d, lba);
    // 檢查該tba block的status是不是status_in_use，如果是的話is_block_data_valid==true，else = false
    return d->storage[tba].status == status_in_use;
#else
    return d->storage[lba_to_pba(d, lba)].status == status_in_use;
#endif
}
void add_pba(struct disk *d, unsigned long lba, b_table_head_t *table)
{
    unsigned long pba = lba_to_pba(d, lba);
    batch_add(d, pba, table);
}
// 用傳入的lba找到對應的tba並加入batch table裡面
void add_tba(struct disk *d, unsigned long lba, b_table_head_t *table)
{
    unsigned long tba = lba_to_tba(d, lba);
    batch_add(d, tba, table);
}

int lba_invalid(struct disk *d, unsigned long lba, size_t n, unsigned long fid)
{
    if (n == 0)
        return 0;
    for (size_t i = 0; i < n; i++)
    {
        // 如果lba對應的block是valid的話
        if (is_block_data_valid(d, lba + i, fid))
        {
#ifdef TOP_BUFFER
            add_tba(d, lba + i, &gbtable);
            unsigned long pba = lba_to_pba(d, lba + i);
            // 如果該lba對應到的pba的type是top_buffer_type，代表該lba對應到的pba的block是top buffer
            // 就要加進batch table裡面
            if (d->ptt_table_head->table[pba].type == top_buffer_type)
                add_pba(d, lba + i, &gbtable);
#else
            add_pba(d, lba + i, &gbtable);
#endif
        }
        else
        {
            fprintf(stderr, "Error: Invalid a block whick is not valid. lba: %lu\n", lba + i);
            exit(EXIT_FAILURE);
        }
    }
    return batch_invalid(d, &gbtable);
}

int lba_read(struct disk *d, unsigned long lba, size_t n, unsigned long fid)
{
    size_t num_invalid = 0;
    struct report *report = &d->report;
    if (n == 0)
        return 0;
    if (!(lba < report->max_block_num) ||
        !((lba + (n - 1)) < report->max_block_num))
    {
        return 0;
    }
    for (size_t i = 0; i < n; i++)
    {
        if (is_block_data_valid(d, lba + i, fid))
        {
#ifdef TOP_BUFFER
            // 如果是top buffer的話就要找lba對應的tba
            batch_add(d, lba_to_tba(d, lba + i), &gbtable);
#else
            batch_add(d, lba_to_pba(d, lba + i), &gbtable);
#endif
        }
        else
        {
            num_invalid++;
            d->report.num_invalid_read++;
        }
    }
    if (num_invalid == n)
        return 0;
    return batch_read(d, &gbtable);
}

bool is_invalid_write(struct disk *d, unsigned long lba, unsigned long fid)
{
    struct ltp_entry *e = &d->ltp_table_head->table[lba];
    return e->valid && (fid != e->fid);
}

int lba_write(struct disk *d, unsigned long lba, size_t n, unsigned long fid)
{
    size_t num_invalid = 0;
    struct report *report = &d->report;
    if (n == 0)
        return 0;
    if (!(lba < report->max_block_num) || !((lba + (n - 1)) < report->max_block_num))
    {
        return 0;
    }
    for (size_t i = 0; i < n; i++)
    {
        if (is_invalid_write(d, lba + i, fid))
        {
            num_invalid++;
            report->num_invalid_write++;
            continue;
        }
        unsigned long pba = pba_search(d, lba + i, fid);
        assert(pba < d->report.max_block_num);
        /* link lba and pba */
        d->storage[pba].lba[0] = lba + i;
        batch_add(d, pba, &gbtable);
    }
    if (num_invalid == n)
        return 0;
    return batch_write(d, &gbtable);
}

int native_lba_delete(struct disk *d, unsigned long lba, size_t n, unsigned long fid)
{
    struct report *report = &d->report;
    size_t len = report->max_block_num;
    struct ltp_entry *entry = d->ltp_table_head->table;
    int count = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (entry[i].valid && (fid == entry[i].fid))
        {
#ifdef TOP_BUFFER
            unsigned long tba = lba_to_tba(d, i);
            batch_add(d, tba, &gbtable);
#else
            unsigned long pba = lba_to_pba(d, i);
            batch_add(d, pba, &gbtable);
#endif
            count++;
        }
    }
    if (count == 0)
        return 0;
    return batch_delete(d, &gbtable);
}
int lba_delete(struct disk *d, unsigned long lba, size_t n, unsigned long fid)
{
#ifdef VIRTUAL_GROUPS
    return vg_lba_delete(d, lba, n, fid);
#else
    return native_lba_delete(d, lba, n, fid);
#endif
}

void clear_info(struct disk *d)
{
    struct report *p = &d->report;
    p->total_access_time = 0;
    memset(&d->report.normal, 0, sizeof(struct time_size));
    memset(&d->report.journaling, 0, sizeof(struct time_size));
}

#ifdef VIRTUAL_GROUPS
int vg_lba_delete(struct disk *d, unsigned long lba, size_t n, unsigned long fid)
{
    struct report *report = &d->report;
    size_t len = report->max_block_num;
    struct ltp_entry *entry = d->ltp_table_head->table;
    int count = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (entry[i].valid && (fid == entry[i].fid))
        {
            unsigned long pba = entry[i].pba;
            unsigned long lba = d->storage[pba].lba;
            if (storage_is_free(d, pba))
            {
                fprintf(stderr, "Error: Delete pba = %lu failed. lba = %lu. status of "
                                "block is "
                                "not valid.",
                        pba, lba);
                exit(EXIT_FAILURE);
            }
            else if (lba == i)
            {
                batch_add(d, entry[i].pba, &gbtable);
                count++;
            }
            else
            {
                fprintf(stderr, "Error: Delete lba = %lu failed. lba of entry(%lu) "
                                "!= lba "
                                "of storage(%lu).\n",
                        lba, lba, i);
                exit(EXIT_FAILURE);
            }
        }
    }
    if (count == 0)
        return 0;
    return batch_invalid(d, &gbtable);
}

#endif
int journaling_write(struct disk *d, unsigned long lba, size_t n, unsigned long fid)
{
    struct report *report = &d->report;
    if (n == 0)
        return 0;
    assert((lba < report->max_block_num) || ((lba + (n - 1)) < report->max_block_num));

    for (size_t i = 0; i < n; i++)
    {
        unsigned long pba = pba_search_jfs(d, lba + i, fid);
        /* link lba and pba */
        d->storage[pba].lba[0] = lba + i;
        batch_add(d, pba, &gbtable);
    }
    return batch_write(d, &gbtable);
}
void increase_storage_lba_capacity(struct disk *d, unsigned long pba)
{
    if (!(d->storage[pba].lba = (unsigned long *)realloc(d->storage[pba].lba, d->storage[pba].lba_capacity * 2 * sizeof(unsigned long))))
    {
        fprintf(stderr, "ERROR: Fail to increase capacity of storage lba. "
                        "capacity: %u, i: %d\n",
                d->storage->lba_capacity, 2);
        exit(EXIT_FAILURE);
    }
    d->storage[pba].lba_capacity *= 2;
}
#ifdef DEDU_ORIGIN

bool DEDU_is_invalid_write(struct disk *d, unsigned long lba, char *hash)
{
    struct ltp_entry *e = &d->ltp_table_head->table[lba];
    return e->valid && (strcmp(hash, e->hash) != 0);
}
bool is_in_storage_fully_search(struct disk *d, char *hash, unsigned long *pba)
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
    return false;
}
bool is_lba_in_storage(struct disk *d, unsigned long pba, unsigned long lba)
{
    for (unsigned long i = 0; i < d->storage->referenced_count + 1; i++)
        if (d->storage[pba].lba[i] == lba)
            return true;
    return false;
}

int DEDU_lba_write(struct disk *d, unsigned long lba, size_t n, char *hash, int line_cnt)
{
    size_t num_invalid = 0;
    struct report *report = &d->report;
    if (n == 0)
    {
        return 0;
    }
    if (lba > report->max_logical_block_num || (lba + (n - 1)) > report->max_logical_block_num)
    {
        printf("lba = %ld\n", lba);
        printf("max_block_num = %ld\n", report->max_logical_block_num);
        perror("Error: lba out of max_block_num.");
        longjmp(env, 1);
        return 0;
    }
    for (size_t i = 0; i < n; i++)
    {
        unsigned long pba;
        pba = DEDU_pba_search(d, lba + i, hash);
        assert(pba < d->report.max_block_num);
#ifdef NO_DEDU
        d->storage[pba].lba[0] = lba + i;
        strcpy(d->storage[pba].hash, hash);
        batch_add(d, pba, &gbtable);
#else
        if (d->storage[pba].referenced_count > 0)
        {
            if (strcmp(hash, d->storage[pba].hash) != 0)
            {
                fprintf(stderr, "%s will be write\n", hash);
                fprintf(stderr, "%s in storage\n", d->storage[pba].hash);
                fprintf(stderr, "lba = %lu\n", lba);
                fprintf(stderr, "pba = %lu\n", pba);
                output_disk_info(d);
                output_ltp_table(d);
                exit(EXIT_FAILURE);
            }
            unsigned referenced_count = d->storage[pba].referenced_count;
            if (referenced_count + 1 > d->storage[pba].lba_capacity)
                increase_storage_lba_capacity(d, pba);
            if (!is_lba_in_storage(d, pba, lba))
                d->storage[pba].lba[referenced_count] = lba;
            return 0;
        }
        else
        {
            d->storage[pba].lba[0] = lba + i;
            strcpy(d->storage[pba].hash, hash);
            batch_add(d, pba, &gbtable);
        }
#endif
    }
    if (num_invalid == n)
        return 0;
    return batch_write(d, &gbtable);
}
// int find_next_lba(struct disk *d, unsigned long pba)
// {
//     struct ltp_entry *entry = d->ltp_table_head->table;
//     for (size_t i = 0; i < d->report.max_logical_block_num; i++)
//     {
//         if (entry[i].valid && entry[i].pba == pba && !entry[i].trim)
//             return i;
//     }
//     return -1;
// }

void delete_all_bottom_track(struct disk *d)
{
    for (size_t i = 0; i < d->report.max_block_num; i += 2)
    {
        d->storage[i].status = status_trimed;
    }
}

bool delete_lba_in_storage(struct disk *d, unsigned long pba, unsigned lba)
{
    unsigned long num_of_lba = d->storage[pba].referenced_count + 1;
    int index = -1;
    for (unsigned long i = 0; i < num_of_lba; i++)
    {
        if (d->storage[pba].lba[i] == lba)
        {
            index = i;
            break;
        }
    }
    if (index != -1)
    {
        for (unsigned long i = index; i < num_of_lba - 1; i++)
        {
            d->storage[pba].lba[i] = d->storage[pba].lba[i + 1];
        }
        return true;
    }
    return false;
}

void DEDU_Trim(struct disk *d, unsigned long lba, size_t n, char *hash)
{
    struct ltp_entry *entry = d->ltp_table_head->table;
    if (entry[lba].valid && strcmp(hash, entry[lba].hash) == 0 && !entry[lba].trim)
    {
        entry[lba].trim = true;
        entry[lba].valid = false;

        unsigned long pba = lba_to_pba(d, lba);
        assert(d->storage[pba].status == status_in_use);

        bool is_sucess_delete_lba_in_storage = delete_lba_in_storage(d, pba, lba);
        if (!is_sucess_delete_lba_in_storage)
        {
            fprintf(stderr, "Error: Failed to delete lba in disk!\npba = %ld\nlba = %ld\n", pba, lba);
            fprintf(stderr, "referenced count = %d\n", d->storage[pba].referenced_count);
            for (int i = 0; i < d->storage[pba].referenced_count + 1; i++)
            {
                fprintf(stderr, "In storage lba = %ld\n", d->storage[pba].lba[i]);
            }
            exit(EXIT_FAILURE);
        }

        if (d->storage[pba].referenced_count > 0)
        {
            d->storage[pba].referenced_count--;
        }
        else
        {
            d->storage[pba].status = status_trimed;
            is_bottom_has_trimed_track = true;
        }
    }
    else
    {
        return;
    }
}
#endif