#include "record_op.h"
#include "batch.h"
#include "lba.h"
#include "op_mode.h"
#include "pba.h"

void record_read(struct disk *d, b_table_head_t *h)
{
    unsigned long curr_track, next_track;
    int continuous_tracks = 1;
    size_t i, len;
    bb_entry_t *m = h->block_head.table;
    len = h->block_head.size;

    curr_track = m[0].pba;
    for (i = 0; (i < len) && ((i + 1) < len); i++)
    {
        if (m[i].isVirtual)
        {
            fprintf(stderr, "Error: read virtual tracks.\n");
            exit(EXIT_FAILURE);
        }
        next_track = m[i + 1].pba;
        if ((curr_track + 1) == next_track)
        {
            continuous_tracks++;
        }
        else
        {
            record_read_physical_tracks(d, continuous_tracks);
            continuous_tracks = 1;
        }
        curr_track = next_track;
    }
    /* last one */
    record_read_physical_tracks(d, continuous_tracks);
}
#ifdef CMR
void record_write(struct disk *d, b_table_head_t *h)
{
    bb_entry_t *m = h->block_head.table;
    unsigned long curr_track, next_track;
    size_t nums;
    long continuous_tracks;

    nums = h->block_head.size;
    continuous_tracks = 1;
    curr_track = m[0].pba;

    for (size_t i = 0; (i < nums) && (i + 1 < nums); i++)
    {
        next_track = m[i + 1].pba;
        if ((curr_track + 1) == next_track)
        {
            continuous_tracks++;
        }
        else
        {
            record_write_physical_tracks(d, continuous_tracks);
            continuous_tracks = 1;
        }
        curr_track = next_track;
    }
    record_write_physical_tracks(d, continuous_tracks);
}
#else
/*
 * unit of calculated size is _track_.
 */
void record_write(struct disk *d, b_table_head_t *h)
{
    unsigned long curr_track, next_track, extend_size;
    long continuous_tracks = 1;
    long num_phy_tracks = 0;
    size_t i;
    bool curr_is_virtual, is_virtual;
    bb_entry_t *m = h->extend_head.table;
    extend_size = h->extend_head.size;

    /* in case only has 1 entry */
    curr_track = m[0].pba;
    curr_is_virtual = m[0].isVirtual;
    is_virtual = curr_is_virtual;
    for (i = 0; (i < extend_size) && ((i + 1) < extend_size); i++)
    {
        if (curr_is_virtual)
            is_virtual = true;
        else
            num_phy_tracks++;
        next_track = m[i + 1].pba;
        if ((curr_track + 1) == next_track)
        {
            continuous_tracks++;
        }
        else
        {
            if (is_virtual)
                record_write_virtual_tracks(d, continuous_tracks, num_phy_tracks);
            else
                record_write_physical_tracks(d, continuous_tracks);
            continuous_tracks = 1;
            num_phy_tracks = 0;
            is_virtual = false;
        }
        curr_track = next_track;
        curr_is_virtual = m[i + 1].isVirtual;
    }
    /* last one */
    if (curr_is_virtual)
        is_virtual = true;
    else
        num_phy_tracks++;

    if (is_virtual)
    {
        record_write_virtual_tracks(d, continuous_tracks, num_phy_tracks);
    }
    else
    {
        record_write_physical_tracks(d, continuous_tracks);
    }
}
#endif

#ifdef CMR
/*
 * unit of calculated size is _track_.
 */
void record_delete(struct disk *d, b_table_head_t *h)
{
    bb_entry_t *m = h->block_head.table;
    unsigned long curr_track, next_track;
    size_t nums;
    long continuous_tracks;

    nums = h->block_head.size;
    continuous_tracks = 1;
    curr_track = m[0].pba;

    for (size_t i = 0; (i < nums) && (i + 1 < nums); i++)
    {
        next_track = m[i + 1].pba;
        if ((curr_track + 1) == next_track)
        {
            continuous_tracks++;
        }
        else
        {
            record_delete_physical_tracks(d, continuous_tracks);
            continuous_tracks = 1;
        }
        curr_track = next_track;
    }
    record_delete_physical_tracks(d, continuous_tracks);
}
#else
/*
 * unit of calculated size is _track_.
 */
void record_delete(struct disk *d, b_table_head_t *h)
{
    unsigned long curr_track, next_track, extend_size;
    long continuous_tracks = 1;
    long num_phy_tracks = 0;
    size_t i;
    bool curr_is_virtual, is_virtual;
    bb_entry_t *m = h->extend_head.table;
    extend_size = h->extend_head.size;

    /* in case only has 1 entry */
    curr_track = m[0].pba;
    curr_is_virtual = m[0].isVirtual;
    is_virtual = curr_is_virtual;
    for (i = 0; (i < extend_size) && ((i + 1) < extend_size); i++)
    {
        if (curr_is_virtual)
            is_virtual = true;
        else
            num_phy_tracks++;

        next_track = m[i + 1].pba;
        if ((curr_track + 1) == next_track)
        {
            continuous_tracks++;
        }
        else
        {
            if (is_virtual)
            {
                record_delete_virtual_tracks(d, continuous_tracks,
                                             num_phy_tracks);
            }
            else
            {
                record_delete_physical_tracks(d, continuous_tracks);
            }
            continuous_tracks = 1;
            num_phy_tracks = 0;
            is_virtual = false;
        }
        curr_track = next_track;
        curr_is_virtual = m[i + 1].isVirtual;
    }
    /* last one */
    if (curr_is_virtual)
        is_virtual = true;
    else
        num_phy_tracks++;

    if (is_virtual)
    {
        record_delete_virtual_tracks(d, continuous_tracks, num_phy_tracks);
    }
    else
    {
        record_delete_physical_tracks(d, continuous_tracks);
    }
}
#endif

void record_write_physical_tracks(struct disk *d, long tracks)
{
    struct report *p = &d->report;
    struct time_size *t_s = (recording_mode == normal_op_mode) ? &p->normal : &p->journaling;
    p->total_access_time += L_SEEK + tracks * (L_ROTATIONAL + L_TRANSFER);
    t_s->total_write_time += L_SEEK + tracks * (L_ROTATIONAL + L_TRANSFER);
    t_s->total_write_size += tracks * TRACK_SIZE;
    p->total_write_size += tracks * TRACK_SIZE;
    // total_write_block_size records in pba_write()
}

void record_read_physical_tracks(struct disk *d, long tracks)
{
    struct report *p = &d->report;
    struct time_size *t_s = (recording_mode == normal_op_mode) ? &p->normal : &p->journaling;
    p->total_access_time += L_SEEK + tracks * (L_ROTATIONAL + L_TRANSFER);
    t_s->total_read_time += L_SEEK + tracks * (L_ROTATIONAL + L_TRANSFER);
    p->total_read_size += tracks * TRACK_SIZE;
    // total_read_block_size records in pba_read()
}

void record_delete_physical_tracks(struct disk *d, long tracks)
{
    struct report *p = &d->report;
    // printf("delete physical tracks: %d tracks\n", tracks);
    p->total_access_time += L_SEEK + tracks * (L_ROTATIONAL + L_TRANSFER);
    p->total_delete_time += L_SEEK + tracks * (L_ROTATIONAL + L_TRANSFER);
    p->total_delete_write_size += tracks * TRACK_SIZE;
    // total_delete_actual_size records in pba_delete()
}

void record_write_virtual_tracks(struct disk *d, long total_tracks, long num_phy_tracks)
{
    struct report *p = &d->report;
    struct time_size *t_s = (recording_mode == normal_op_mode) ? &p->normal : &p->journaling;
    uint64_t reread_time, write_time, total_time;
    reread_time = L_SEEK + total_tracks * (L_ROTATIONAL + L_TRANSFER);
    // one for temp area, one for writing back
    write_time = 2 * (L_SEEK + total_tracks * (L_ROTATIONAL + L_TRANSFER));
    total_time = reread_time + write_time;
    // record
    p->total_access_time += total_time;
    t_s->total_write_time += write_time + reread_time;
    p->total_rewrite_size += (total_tracks - num_phy_tracks) * TRACK_SIZE;
    p->total_reread_size += total_tracks * TRACK_SIZE;
    t_s->total_write_size += num_phy_tracks * TRACK_SIZE;
    p->total_write_size += num_phy_tracks * TRACK_SIZE;
}

void record_delete_virtual_tracks(struct disk *d, long total_tracks, long num_phy_tracks)
{
    // printf("delete virtual tracks: %d tracks\n", tracks);
    struct report *p = &d->report;
    uint64_t read_time, write_time, total_time;
    read_time = L_SEEK + total_tracks * (L_ROTATIONAL + L_TRANSFER);
    // 2 times, one for temp area, one for writing back
    write_time = 2 * (L_SEEK + total_tracks * (L_ROTATIONAL + L_TRANSFER));
    total_time = read_time + write_time;
    p->total_access_time += total_time;
    p->total_delete_time += write_time + read_time;
    p->total_delete_write_size += num_phy_tracks * TRACK_SIZE;
    p->total_delete_rewrite_size +=
        (total_tracks - num_phy_tracks) * TRACK_SIZE;
    p->total_delete_reread_size += total_tracks * TRACK_SIZE;
}