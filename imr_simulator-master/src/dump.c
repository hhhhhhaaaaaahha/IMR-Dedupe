#include "dump.h"
#include "fid_table.h"
#include "lba.h"
#include "virtual_groups.h"

char *vg_status_str[] = {
    "free",
    "random",
    "constrain",
    "jfs",
};

char *file_attr_str[] = {
    "free_file",
    "hot_file",
    "cold_file",
    "error_file",
};

void dump_fid_table()
{
    fprintf(stderr, "[START] dump fid table.\n");
    for (int i = 0; i < g_fid_table.count; i++)
    {
        fprintf(stderr,
                "fid: %lu, attr: %s\n",
                g_fid_table.table[i].fid,
                file_attr_str[g_fid_table.table[i].attr]);
    }
    fprintf(stderr, "[ END ] dump fid table.\n");
}

void dump_vg_list(struct disk *d)
{
    fprintf(stderr, "[START] dump vg list informations...\n");
    fprintf(stderr, "Total number of vg: %d\n", d->vgh.num_of_groups);
    for (virtual_group_t *vg = global_vg_list; vg != NULL; vg = vg->next)
    {
        fprintf(stderr, "vg%d:\n", vg->vg_id);
        fprintf(stderr, "status:    %s\n", vg_status_str[vg->vg_status]);
        fprintf(stderr, "file_attr: %s\n", file_attr_str[vg->f_attr]);
        fprintf(stderr, "capacity:  %d\n", vg->capacity);
        fprintf(stderr, "count:     %d\n", vg->size);
        fprintf(stderr, "----------------------\n");
    }

    fprintf(stderr, "[ END ] dump vg list informations...\n");
}
