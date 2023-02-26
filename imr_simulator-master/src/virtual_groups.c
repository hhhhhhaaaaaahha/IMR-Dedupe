#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "batch.h"
#include "dump.h"
#include "fid_table.h"
#include "lba.h"
#include "list.h"
#include "pba.h"
#include "rw.h"
#include "virtual_groups.h"

journal_check_bottom_is_free check_bottom_ptr =
    vg_journal_check_bottom_free_2_sides;

unsigned int percentage = 100;
bool identical_attr_files = false;

virtual_group_t *global_vg_list;
vg_list_head_t vg_free_head;
vg_list_head_t vg_random_head;
vg_list_head_t vg_constrained_head;
/* only exist 1 hot vg and 1 cold vg in system */
virtual_group_t *current_hot_vg;
virtual_group_t *current_cold_vg;
virtual_group_t **this_vg;
// for jfs
virtual_group_t *current_journal_vg;
vg_list_head_t vg_jfs_head;

bool *vg_update_arr;
virtual_group_t *vg_arr;
virtual_groups_head_t *vgh;
unsigned long granularity;

int num_of_groups;

void vg_init_attr(virtual_group_t *vg, file_attr_t attr)
{
    vg->f_attr = attr;
    vg->current_track = (attr == hot_file_attr ? vg->current_top_track
                                               : vg->current_bottom_track);
}

void init_vg_element(virtual_group_t *vg)
{
    unsigned long start = vg->lower_border;
    unsigned long end = vg->higher_border;

    vg->vg_status = vg_free;
    vg->capacity = vg->higher_border - vg->lower_border + 1;
    vg->f_attr = free_file_attr;
    vglist_add_tail(&vg_free_head, &vg->list);
    if (is_toptrack(start))
    {
        vg->top_start = start;
        vg->bottom_start = start + 1;
    }
    else
    {
        vg->bottom_start = start;
        vg->top_start = start + 1;
    }
    if (is_toptrack(end))
    {
        vg->top_end = end;
        vg->bottom_end = end - 1;
    }
    else
    {
        vg->bottom_end = end;
        vg->top_end = end - 1;
    }

    vg->current_top_track = vg->top_start;
    vg->current_bottom_track = vg->bottom_start;
    vg->current_track = vg->bottom_start;
    vg->in_use = false;
}

int init_virtual_groups(struct disk *d)
{
    vgh = &d->vgh;
    struct report *r = &d->report;
    vgh->granularity = granularity;
    vgh->num_of_groups = (r->max_track_num / vgh->granularity) +
                         !!(r->max_track_num % vgh->granularity);
    num_of_groups = vgh->num_of_groups;
    if (!(vgh->vg = (virtual_group_t *)malloc(sizeof(*vgh->vg) *
                                              vgh->num_of_groups)))
    {
        fprintf(stderr, "ERROR: Couldn't malloc vg array.\n");
        exit(EXIT_FAILURE);
    }
    vg_arr = vgh->vg;
    if (!(vg_update_arr = (bool *)malloc(vgh->num_of_groups * sizeof(bool))))
    {
        fprintf(stderr, "ERROR: Couldn't malloc vg_update_arr array.\n");
        exit(EXIT_FAILURE);
    }
    memset(vg_update_arr, false, sizeof(bool) * vgh->num_of_groups);
    /* initialize global vg list */
    INIT_LIST_HEAD(&vg_free_head.list);
    INIT_LIST_HEAD(&vg_random_head.list);
    INIT_LIST_HEAD(&vg_constrained_head.list);
    INIT_LIST_HEAD(&vg_jfs_head.list);
    int i = 0;
    virtual_group_t **next = &global_vg_list;
    for (i = 0; i < vgh->num_of_groups - 1; i++)
    {
        virtual_group_t *t = &vgh->vg[i];
        t->lower_border = i * vgh->granularity;
#ifdef RESERVED_END_TRACK
        t->higher_border = t->lower_border + vgh->granularity - 1 - 1;
#else
        t->higher_border = t->lower_border + vgh->granularity - 1;
#endif
        t->vg_id = i;
        *next = &vgh->vg[i];
        next = &vgh->vg[i].next;
        init_vg_element(t);
    }

    // last one
    virtual_group_t *t = &vgh->vg[i];
    t->lower_border = i * vgh->granularity;
    t->higher_border = r->max_track_num - 1;
    t->vg_id = i;
    *next = &vgh->vg[i];
    vgh->vg[i].next = NULL;
    init_vg_element(t);

    return 0;
}

void end_vg_list_head(vg_list_head_t *head)
{
    INIT_LIST_HEAD(&head->list);
    head->count = 0;
}

void end_virtual_groups(virtual_groups_head_t *head)
{
    end_vg_list_head(&vg_free_head);
    end_vg_list_head(&vg_random_head);
    end_vg_list_head(&vg_constrained_head);
    current_hot_vg = NULL;
    current_cold_vg = NULL;
    global_vg_list = NULL;
    free(head->vg);
    free(vg_update_arr);
}

bool journal_find_available_track(struct disk *d, virtual_group_t *vg)
{
    unsigned long top = vg->top_start;
    unsigned long bottom = vg->bottom_start;
    /* 1st: iterate bottom track */
    for (size_t track = bottom; track <= vg->higher_border; track += 2)
    {
        if (storage_is_in_use(d, track))
            continue;
        if (!check_bottom_ptr(d, track))
            continue;
        vg->current_track = track;
        return true;
    }
    /* iterate top track */
    for (size_t track = top; track <= vg->higher_border; track += 2)
    {
        if (storage_is_in_use(d, track))
            continue;
        vg->current_track = track;
        return true;
    }
    return false;
}

bool journal_get_before_current_vg(struct disk *d)
{
    if (!current_journal_vg)
        return false;
    virtual_group_t *vg;
    for (vg = global_vg_list; vg != current_journal_vg; vg = vg->next)
    {
        if (vg->in_use)
            continue;
        if (journal_find_available_track(d, vg))
        {
            get_vg(vg, &current_journal_vg);
            return true;
        }
    }
    current_journal_vg = NULL;
    return false;
}

void get_vg(virtual_group_t *vg, virtual_group_t **to)
{
    assert(to != NULL);
    list_del(&vg->list);
    *to = vg;
    vg->in_use = true;
}

bool journal_get_after_current_vg(struct disk *d)
{
    virtual_group_t *vg =
        (!current_journal_vg ? global_vg_list : current_journal_vg->next);

    for (; vg != NULL; vg = vg->next)
    {
        if (vg->in_use)
            continue;
        if (journal_find_available_track(d, vg))
        {
            get_vg(vg, &current_journal_vg);
            return true;
        }
    }
    return false;
}

bool journal_get_from_current_vg(struct disk *d)
{
    if (current_journal_vg == NULL)
        return false;
    virtual_group_t *vg = current_journal_vg;
    /* iterate from current track */
    for (size_t track = vg->current_track; track <= vg->higher_border;
         track += 2)
    {
        if (storage_is_in_use(d, track))
            continue;
        if (!is_toptrack(track) && !check_bottom_ptr(d, track))
        {
            continue;
        }
        vg->current_track = track;
        return true;
    }
    /* have iterated all track */
    if (is_toptrack(vg->current_track))
    {
        vglist_add_tail(&vg_jfs_head, &vg->list);
        vg->in_use = false;
        return false;
    }

    /* iterate top track */
    for (size_t track = vg->top_start; track <= vg->higher_border; track += 2)
    {
        if (storage_is_in_use(d, track))
            continue;
        vg->current_track = track;
        return true;
    }
    vglist_add_tail(&vg_jfs_head, &vg->list);
    vg->in_use = false;
    return false;
}

bool vg_get_from_another_current(struct disk *d, file_attr_t attr)
{
    this_vg = (attr == hot_file_attr ? &current_cold_vg : &current_hot_vg);
    virtual_group_t *vg = *this_vg;

    for (size_t track = vg->current_track; track <= vg->higher_border;
         track += 2)
    {
        if (storage_is_in_use(d, track))
            continue;
        vg->current_track = track;
        return true;
    }
    set_vg(vg);
    return false;
}

bool vg_get_from_current_vg(struct disk *d, file_attr_t attr)
{
    this_vg = (attr == hot_file_attr ? &current_hot_vg : &current_cold_vg);
    virtual_group_t *vg = *this_vg;
    if (!vg)
        return false;
    for (size_t track = vg->current_track; track <= vg->higher_border;
         track += 2)
    {
        if (storage_is_in_use(d, track))
            continue;
        vg->current_track = track;
        return true;
    }
    set_vg(vg);
    *this_vg = NULL;
    return false;
}

bool vg_get_from_free_list(struct disk *d, file_attr_t attr)
{
    struct list_head *pos;
    list_for_each(pos, &vg_free_head.list)
    {
        virtual_group_t *vg = list_entry(pos, virtual_group_t, list);
        assert(!vg->in_use);
        vg_init_attr(vg, attr);
        get_vg(vg, this_vg);
        vg->vg_status = vg_random;
        vg_free_head.count--;
        return true;
    }
    return false;
}

bool vg_get_from_all_list(struct disk *d, file_attr_t attr)
{
    struct list_head *pos;
    identical_attr_files = true;
    list_for_each(pos, &vg_random_head.list)
    {
        virtual_group_t *vg = list_entry(pos, virtual_group_t, list);
        assert(!vg->in_use);
        for (size_t i = vg->current_track; i <= vg->higher_border; i += 2)
        {
            if (storage_is_in_use(d, i))
                continue;
            get_vg(vg, this_vg);
            vg->current_track = i;
            return true;
        }
        for (size_t i = vg->lower_border; i <= vg->higher_border; i += 1)
        {
            if (storage_is_in_use(d, i))
                continue;
            get_vg(vg, this_vg);
            vg->current_track = i;
            return true;
        }
    }
    list_for_each(pos, &vg_constrained_head.list)
    {
        virtual_group_t *vg = list_entry(pos, virtual_group_t, list);
        assert(!vg->in_use);
        for (size_t i = vg->current_track; i <= vg->higher_border; i += 2)
        {
            if (storage_is_in_use(d, i))
                continue;
            get_vg(vg, this_vg);
            vg->current_track = i;
            return true;
        }
        for (size_t i = vg->lower_border; i <= vg->higher_border; i += 1)
        {
            if (storage_is_in_use(d, i))
                continue;
            get_vg(vg, this_vg);
            vg->current_track = i;
            return true;
        }
    }
    return false;
}

bool vg_get_from_random_list(struct disk *d, file_attr_t attr)
{
    struct list_head *pos;
    list_for_each(pos, &vg_random_head.list)
    {
        virtual_group_t *vg = list_entry(pos, virtual_group_t, list);
        assert(!vg->in_use);
        if (attr != vg->f_attr)
            continue;
        for (size_t i = vg->current_track; i <= vg->higher_border; i += 2)
        {
            if (storage_is_in_use(d, i))
                continue;
            get_vg(vg, this_vg);
            vg_random_head.count--;
            vg->current_track = i;
            return true;
        }
    }
    return false;
}

bool vg_get_from_constrained_list(struct disk *d, file_attr_t attr)
{
    struct list_head *pos;
    list_for_each(pos, &vg_constrained_head.list)
    {
        virtual_group_t *vg = list_entry(pos, virtual_group_t, list);
        if (attr == vg->f_attr)
            continue;
        for (size_t i = vg->current_track; i <= vg->higher_border; i += 2)
        {
            if (storage_is_in_use(d, i))
                continue;
            get_vg(vg, this_vg);
            vg->current_track = i;
            return true;
        }
    }
    return false;
}

int clear_track(struct disk *d, unsigned long track)
{
    int count = 0;
    unsigned long base = track;
    if (d->storage[base].status == status_invalid)
    {
        d->storage[base].status = status_free;
        count++;
    }
    return count;
}

int clear_near_track(struct disk *d, unsigned long track)
{
    int count = 0;
    if (track != 0)
        count += clear_track(d, track - 1);
    if (track < (d->report.max_track_num) - 1)
        count += clear_track(d, track + 1);
    return count;
}

bool _journal_get_block(struct disk *d, file_attr_t attr, unsigned long *result)
{
    virtual_group_t *vg = current_journal_vg;
    unsigned long pba = vg->current_track;
    d->storage[pba].status = status_booked;
    vg->current_track += 2;
    clear_near_track(d, pba);
    *result = pba;
    return true;
}

bool _vg_get_block(struct disk *d, file_attr_t attr, unsigned long *result)
{
    virtual_group_t *vg = *this_vg;
    unsigned long pba = vg->current_track;
    d->storage[pba].status = status_booked;
    vg->current_track += 2;
    *result = pba;
    return true;
}

unsigned long vg_journal_get_block(struct disk *d)
{
    unsigned long pba = -1;
    if (journal_get_from_current_vg(d) &&
        _journal_get_block(d, hot_file_attr, &pba))
        goto done_get_block;
    else if (journal_get_after_current_vg(d) &&
             _journal_get_block(d, hot_file_attr, &pba))
        goto done_get_block;
    else if (journal_get_before_current_vg(d) &&
             _journal_get_block(d, hot_file_attr, &pba))
        goto done_get_block;
done_get_block:
    return pba;
}

unsigned long vg_get_block(struct disk *d, file_attr_t attr)
{
    unsigned long pba;
    if (vg_get_from_current_vg(d, attr) && _vg_get_block(d, attr, &pba))
        return pba;
    else if (!identical_attr_files && vg_get_from_free_list(d, attr) &&
             _vg_get_block(d, attr, &pba))
        return pba;
    else if (!identical_attr_files && vg_get_from_random_list(d, attr) &&
             _vg_get_block(d, attr, &pba))
        return pba;
    else if (!identical_attr_files && vg_get_from_constrained_list(d, attr) &&
             _vg_get_block(d, attr, &pba))
        return pba;
    else if (vg_get_from_all_list(d, attr) && _vg_get_block(d, attr, &pba))
        return pba;
    else if (vg_get_from_another_current(d, attr) &&
             _vg_get_block(d, attr, &pba))
        return pba;
    else
    {
        fprintf(stderr, "Error: Failed to get block.\n");
        fprintf(stderr, "file attr: %s.\n", file_attr_str[attr]);
        assert(false);
        exit(EXIT_FAILURE);
    }
}

/* need to consider ltp table of top only. */
void swap_block(struct disk *d, unsigned long top, unsigned long bottom)
{
    unsigned long top_lba = d->storage[top].lba;
    block_status_t status = d->storage[top].status;
    switch (status)
    {
    case status_free:
        d->storage[bottom].status = status_free;
        break;
    case status_invalid:
        d->storage[bottom].status = status_invalid;
        break;
    case status_in_use:
        rw_block(d, top, bottom);
        d->storage[bottom].status = status_in_use;
        d->storage[bottom].lba = top_lba;
        d->ltp_table_head->table[top_lba].pba = bottom;
        break;
    default:
        fprintf(stderr, "Error: swap_block out of options.\n");
        fprintf(stderr, "status= %d\n", status);
        exit(EXIT_FAILURE);
    }
    d->storage[top].status = status_booked;
}
bool do_dual_swap(struct disk *d, unsigned long bba, unsigned long *result)
{
    assert(result != NULL);
    if (is_toptrack(bba))
        return false;
    virtual_group_t *vg = pba_to_vg(bba);
    if (vg->vg_status != vg_constrain)
        return false;
    unsigned long track, higher_neighbor, lower_neighbor;
    track = lba_to_track(bba);
    higher_neighbor = track > d->report.max_track_num ? -1 : track + 1;
    lower_neighbor = track == 0 ? -1 : track - 1;
    if (((higher_neighbor == -1) ||
         (higher_neighbor != -1 && storage_is_free(d, higher_neighbor))) &&
        ((lower_neighbor == -1) ||
         ((lower_neighbor != -1) && storage_is_free(d, lower_neighbor))))
        return false;
    return dual_swap(d, vg, bba, result);
}

bool dual_swap(struct disk *d,
               virtual_group_t *vg,
               unsigned long bba,
               unsigned long *tba)
{
    /* find an empty top block */
    unsigned long top_block = -1;
    unsigned int count = -1;
    bool has_empty = false;

    for (unsigned long track = vg->top_start;
         !has_empty && (track <= vg->top_end); track += 2)
    {
        unsigned long pba = track;
        block_status_t status = d->storage[pba].status;
        if (status == status_booked)
            continue;
        else if ((status == status_free) || (status == status_invalid))
        {
            top_block = pba;
            has_empty = true;
            break;
        }
        else if (d->storage[pba].count < count)
        {
            count = d->storage[pba].count;
            top_block = pba;
        }
        /*
        } else {
            printf("d->storage[pba].count = %u\n", d->storage[pba].count);
            printf("count = %u\n", count);
            if (d->storage[pba].count < count)
                printf("<<<<<<<<<<\n");
            else if (d->storage[pba].count > count)
                printf(">>>>>>>>>>\n");
            else
                printf("==========\n");
        }
        */
    }
    if (top_block == -1)
    {
        // all top block are booked, just leave.
        return false;
    }
    swap_block(d, top_block, bba);
    d->report.dual_swap_count++;
    *tba = top_block;
    return true;
}

virtual_group_t *pba_to_vg(unsigned long pba)
{
    for (virtual_group_t *p = global_vg_list; p != NULL; p = p->next)
    {
        unsigned long begin = p->lower_border;
        unsigned long end = p->higher_border;
        if ((pba >= begin) && (pba <= end))
            return p;
    }
    return NULL;
}

void set_vg(virtual_group_t *vg)
{
    file_attr_t attr = vg->f_attr;
    vg_status_t status = vg->vg_status;
    vg->start_track = vg->lower_border;
    switch (status)
    {
    case vg_free:
        vg->vg_status = vg_random;
        vglist_add_tail(&vg_random_head, &vg->list);
        break;
    case vg_random:
        if (attr == hot_file_attr)
            vg->current_track = vg->bottom_start;
        else
            vg->current_track = vg->top_start;
    case vg_constrain:
        vg->vg_status = vg_constrain;
        vglist_add_tail(&vg_constrained_head, &vg->list);
        break;
    case vg_jfs:
        break;
    }
    vg->in_use = false;
}

int track_to_vg_index(unsigned long track)
{
    return (track / vgh->granularity);
}

void clear_vg_update_arr()
{
    memset(vg_update_arr, false, sizeof(*vg_update_arr) * vgh->num_of_groups);
}

void _update_vg_list(struct disk *d, int i)
{
    bool top_has_data, bottom_has_data;
    top_has_data = false;
    bottom_has_data = false;
    virtual_group_t *vg = &vg_arr[i];
    if (vg->in_use)
        return;
    for (size_t i = vg->lower_border; i < vg->higher_border; i++)
        if (storage_is_in_use(d, i))
        {
            if (is_toptrack(i))
                top_has_data = true;
            else
                bottom_has_data = true;
        }
    list_del(&vg->list);
    if (top_has_data && bottom_has_data)
    {
        vg->vg_status = vg_constrain;
        list_add_tail(&vg->list, &vg_constrained_head.list);
    }
    else if (top_has_data || bottom_has_data)
    {
        vg->vg_status = vg_random;
        vg->current_track = (top_has_data ? vg->top_start : vg->bottom_start);
        list_add_tail(&vg->list, &vg_random_head.list);
    }
    else
    {
        vg->vg_status = vg_free;
        list_add_tail(&vg->list, &vg_free_head.list);
    }
}

void update_vg_list(struct disk *d, bb_head_t *t)
{
    for (size_t i = 0; i < t->size; i++)
    {
        int index = track_to_vg_index(t->table[i].pba);
        vg_update_arr[index] = true;
    }
    for (int i = 0; i < num_of_groups; i++)
        if (vg_update_arr[i])
            _update_vg_list(d, i);
    clear_vg_update_arr();
}

bool vg_journal_check_bottom_free_no_check(struct disk *d, unsigned long track)
{
    return true;
}

bool vg_journal_check_bottom_free_1_sides(struct disk *d, unsigned long track)
{
    unsigned int is_1_side = false;
    if ((track != 0) && storage_is_in_use(d, track - 1))
        is_1_side = ~is_1_side;
    else if ((track < d->report.max_track_num - 1) &&
             storage_is_in_use(d, track + 1))
        is_1_side = ~is_1_side;
    return (bool)is_1_side;
}

bool vg_journal_check_bottom_free_2_sides(struct disk *d, unsigned long track)
{
    bool lower, higher;
    lower = higher = true;
    if ((track != 0) && storage_is_in_use(d, track - 1))
        lower = false;
    else if ((track < d->report.max_track_num - 1) &&
             storage_is_in_use(d, track + 1))
        higher = false;
    return lower && higher;
}

bool journal_switch_check_bottom()
{
    static int count = 0;
    switch (count)
    {
    case 0:
        check_bottom_ptr = vg_journal_check_bottom_free_1_sides;
        count++;
        return true;
    case 1:
        check_bottom_ptr = vg_journal_check_bottom_free_no_check;
        count++;
        return true;
    default:
        return false;
    }
}