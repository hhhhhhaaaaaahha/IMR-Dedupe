
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "chs.h"
#include "lba.h"
#include "pba.h"
#include "scp.h"
#ifdef VIRTUAL_GROUPS
#include "virtual_groups.h"
#endif

int chs_read(struct disk *d, unsigned long base)
{
    return 1;
}

void chs_write(struct disk *d, unsigned long pba)
{
    block_status_t status = d->storage[pba].status;                 // 宣告status = d->storage[pba].status
    assert((status == status_booked) || (status == status_in_use) || (status == status_trimed)); // 檢查如果status!=status_booked或者!=status_in_use的時候跳出
    if (status != status_in_use)                                    // 如果status!=status_in_use的話
    {
        d->storage[pba].status = status_in_use; // 把d->storage[pba].status改成status_in_use
        d->report.current_use_block_num++;      // d->report.current_use_block_num+1
#ifdef VIRTUAL_GROUPS
        virtual_group_t *vg = pba_to_vg(pba);
        vg->size++;
#endif
    }
    d->storage[pba].count++; // d->storage[pba].count+1，count是為了記錄使用次數
}

void chs_delete(struct disk *d, unsigned long pba)
{
    unsigned long lba = d->storage[pba].lba[0];        // 宣告lba = d->storage[pba].lba
    block_status_t status = d->storage[pba].status; // 宣告status = d->storage[pba].status
    if (status != status_in_use)                    // 如果status!=status_in_use的話
    {
        fprintf(stderr, "Error: Delete block failed. status != in_use.\n"); // 印出錯誤訊息到stderr
        fprintf(stderr, "status = %d\n", status);                           // 印出錯誤訊息到stderr
        fprintf(stderr, "%s\n", d->storage[pba].hash);
        exit(EXIT_FAILURE); // 離開
    }
    d->storage[pba].status = status_invalid; // 刪除後把status改成status_invalid
#ifndef TOP_BUFFER
    bool valid = d->ltp_table_head->table[lba].valid; // 檢查原本ltp對應關西的那個entry是不是true
    assert(valid == true);                            // 如果不是的話就跳出
#else
    d->ptt_table_head->table[pba].type = normal_type;
    d->ptt_table_head->table[pba].tba = pba;
#endif
#ifdef TOP_BUFFER
    if (!scp_mode)
        d->ltp_table_head->table[lba].valid = false;
#else
    d->ltp_table_head->table[lba].valid = false; // 如果沒有跳出表示原本應該要是true的，現在因為刪除了所以把valid改成false
#endif
#ifdef VIRTUAL_GROUPS
    virtual_group_t *vg = pba_to_vg(pba);
    vg->size--;
#endif
}
