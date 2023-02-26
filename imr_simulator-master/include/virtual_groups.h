#pragma once

#include <stdbool.h>
#include "batch.h"
#include "file_attr.h"
#include "list.h"

#define is_toptrack(t) ((t) % 2)

typedef bool (*journal_check_bottom_is_free)(struct disk *, unsigned long);
extern journal_check_bottom_is_free check_bottom_ptr;

typedef enum
{
    vg_free,
    vg_random,
    vg_constrain,
    vg_jfs,
} vg_status_t;

typedef struct _virtual_groups_head_t virtual_groups_head_t;

typedef struct _virtual_group_t virtual_group_t;

typedef struct _vg_list_head_t vg_list_head_t;

struct disk;

struct _virtual_group_t
{
    struct list_head list;
    // for journaling
    virtual_group_t *next;
    // the border of vg
    unsigned long lower_border, higher_border;
    unsigned long top_start, top_end;
    unsigned long bottom_start, bottom_end;
    unsigned long start_track, end_track;
    unsigned long current_top_track, current_bottom_track;
    unsigned long current_track;
    int vg_id;
    int capacity;
    int size;
    vg_status_t vg_status;
    file_attr_t f_attr;
    bool in_use;
};

struct _virtual_groups_head_t
{
    int num_of_groups;
    unsigned long granularity;
    virtual_group_t *vg;
};

struct _vg_list_head_t
{
    struct list_head list;
    int count;
};

extern vg_list_head_t vg_free_head;
extern vg_list_head_t vg_random_head;
extern vg_list_head_t vg_constrained_head;

int init_virtual_groups(struct disk *d);
void end_virtual_groups(virtual_groups_head_t *head);
unsigned long vg_get_block(struct disk *d, file_attr_t attr);
bool dual_swap(struct disk *d,
               virtual_group_t *vg,
               unsigned long pba,
               unsigned long *result);
bool do_dual_swap(struct disk *d, unsigned long bba, unsigned long *result);

void set_vg(virtual_group_t *vg);
void get_vg(virtual_group_t *vg, virtual_group_t **to);
virtual_group_t *pba_to_vg(unsigned long pba);

bool vg_journal_check_bottom_free_no_check(struct disk *d, unsigned long pba);
bool vg_journal_check_bottom_free_1_sides(struct disk *d, unsigned long pba);
bool vg_journal_check_bottom_free_2_sides(struct disk *d, unsigned long pba);

unsigned long vg_journal_get_block(struct disk *d);
void update_vg_list(struct disk *d, bb_head_t *t);
bool journal_switch_check_bottom();

static inline void vglist_add_tail(vg_list_head_t *vg_head,
                                   struct list_head *head)
{
    vg_head->count++;
    list_add_tail(head, &vg_head->list);
}

extern virtual_group_t *global_vg_list;