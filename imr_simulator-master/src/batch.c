#include <assert.h>

#include "batch.h"
#include "lba.h"
#include "pba.h"
#include "record_op.h"

/* normal use */
b_table_head_t gbtable;
/* for scp */
b_table_head_t mbtable;

/*
 * return int
 * 1 for error
 * 0 for success
 */
int init_batch_table(b_table_head_t *table)
{
    size_t capacity = 4096;
    if (!(table->block_head.table = (bb_entry_t *)calloc(
              capacity, sizeof(*table->block_head.table))))
    {
        fprintf(stderr, "Error: malloc batch table failed\n");
        goto done_block_table;
    }
    table->block_head.capacity = capacity;
    table->block_head.size = 0;

    if (!(table->extend_head.table = (bb_entry_t *)calloc(
              capacity * 3, sizeof(*table->extend_head.table))))
    {
        fprintf(stderr, "Error: malloc batch table failed\n");
        goto done_extend_table;
    }
    table->extend_head.capacity = 3 * capacity;
    table->extend_head.size = 0;

    return 0;
done_extend_table:
    free(table->block_head.table);
done_block_table:
    return -1;
}

void end_batch_table(b_table_head_t *table)
{
    free(table->block_head.table);
    table->block_head.capacity = 0;
    table->block_head.size = 0;
    free(table->extend_head.table);
    table->extend_head.capacity = 0;
    table->extend_head.size = 0;
}
// 增加batch_block_head的capacity
static void increase_bb_head_capacity(bb_head_t *h, int i)
{
    if (!(h->table = (bb_entry_t *)realloc(h->table, h->capacity * i * sizeof(*h->table)))) // 如果配置空間失敗了
    {
        fprintf(stderr, "ERROR: Fail to increase capacity of batch block table. " // printf錯誤訊息到stderr
                        "capacity: %lu, i: %d\n",
                h->capacity, i);
        exit(EXIT_FAILURE); // 離開
    }
    h->capacity *= i;
}
// 比較兩個batch entry，a比b大的話return 1，a == b的話return 0，a<b的話return -1
int batch_cmp(const void *a, const void *b)
{
    unsigned long first = ((bb_entry_t *)a)->pba;     // 強制轉型後丟給first
    unsigned long second = ((bb_entry_t *)b)->pba;    // 強制轉型後丟給second
    return first > second ? 1 : (first < second) ? -1 // 比較後回傳
                                                 : 0;
}
// 檢查batch_block_table是不是滿的
static bool is_bb_table_full(bb_head_t *h)
{
    return h->capacity <= h->size; // 如果h->size > h->capacity的話 return 1
}
// 增加一個entry到batch_block_table
void bb_table_add(bb_head_t *h, unsigned long pba, bool isVirtual)
{
    if (is_bb_table_full(h))             // 用is_bb_table_full檢查batch_block_table是不是滿的
        increase_bb_head_capacity(h, 2); // 如果是滿的話，利用increase_bb_head_capacity這個funciton來增加它的capacity
    bb_entry_t *e = &h->table[h->size];  // size代表目前table有的最後一個entry的下一個index，用pointer e來接這個entry
    // 下面是這function的參數pba丟進去e裡面
    e->pba = pba;
    e->isVirtual = isVirtual; // 把valid設為true
    h->size++;                // 最後再把size+1
}
// 這個function是要拿到最後一個的pba
bool bb_table_get_last_pba(bb_head_t *h, unsigned long *result)
{
    if (h->size == 0)                    // 如果使用的大小為0(也就是size==0)
        return false;                    // return false，代表取不到
    *result = h->table[h->size - 1].pba; // 透過result把最後一個的pba回傳回去
    return true;                         // 有取的到的話就return true
}
// 對齊格式用的batch_add
void batch_add(struct disk *d, unsigned long pba, b_table_head_t *h)
{
    bb_table_add(&h->block_head, pba, false);
}
// 把batch table的size全部清空為0
void batch_clear(b_table_head_t *table)
{
    table->block_head.size = 0;
    table->extend_head.size = 0;
}
// 增加一個isVirtual == true的entry到batch_block_table裡面
void bb_table_add_entry_virtual(bb_head_t *h, bb_entry_t *e)
{
    bb_table_add(h, e->pba, true);
}
// 增加一個entry到batch_block_table(直接是一個entry版)
void bb_table_add_entry(bb_head_t *h, bb_entry_t *e)
{
    bb_table_add(h, e->pba, e->isVirtual);
}
// 取得batch_block_table的size
size_t bb_table_get_size(bb_head_t *h)
{
    return h->size;
}
// 把batch_block_table的最後一個的isvirtual改成false
void bb_table_modify_last_to_physical(bb_head_t *h)
{
    if (h->size == 0)
        return;
    long pos = h->size - 1;
    h->table[pos].isVirtual = false;
}
// 更改某一個batch_track_table中的一個entry
void bb_table_modify(bb_head_t *h, unsigned long index, unsigned long pba, bool isVirtual)
{
    bb_entry_t *p = h->table;
    p[index].pba = pba;
    p[index].isVirtual = isVirtual;
}
// 這個function的用意是在於去檢查如果寫入bottom track會影響到前後兩個top track，需要另外做處理
/* must be invoked before batch_write */
int batch_extend(struct disk *d, b_table_head_t *h)
{
    bb_head_t *merge_head, *extend_head; /* track 0 is bottom track */
    bb_entry_t *merge_table;
    unsigned long track, prev_track, next_track, pba;
    size_t i, count;

    merge_head = &h->block_head;     // merge_head指向h->block_head
    extend_head = &h->extend_head;   // extend_head指向h->extendhead
    merge_table = merge_head->table; // merge_table指向merge_head這個table的第一個entry

    for (i = 0; i < merge_head->size; i++) // 用迴圈跑每一個merge table裡面的每一個entry
    {
        track = merge_table[i].pba; // track = 當下這一個entry的pba
        /* add track to m table */
        if (is_toptrack(track))                                             // 利用is_toptrack這個macro來判斷這個track是不是top track
        {                                                                   // 如果是top track的話
            if (bb_table_get_last_pba(extend_head, &pba) && (pba == track)) // 利用bb_table_get_last_pba這個function來找最後一個extend table的最後一個pba，如果是最後一個的話，這個if成立
                bb_table_modify_last_to_physical(extend_head);              // 把extend table最後一個entry中的isvirtual改成false
            else
                bb_table_add_entry(extend_head, &merge_table[i]); // 如果不是最後一個的話，把這個entry加到extend table裡面
            continue;                                             // 這個track判斷完之後直接跑下一個迴圈
        }
        /* add lower bound */
        prev_track = track - 1;                                                     // 如果不是top track的話，用prev_track來當下這個track的前一個
        if (track && storage_is_in_use(d, prev_track))                              // 用storage_is_in_use這個function來判斷前一個track是不是有在使用的跟當下這個track是不是無效的來判斷
        {                                                                           // 如果true的話代表前一個track是需要被處理的
            if (!(bb_table_get_last_pba(extend_head, &pba) && (pba == prev_track))) // 判斷pre_trak是不是最後一個pba
            {
                bb_table_add(extend_head, track - 1, true); // 如果不是的話就把他加到extend table裡面，並把該entry的isvirtual設成true
                count++;                                    // count++，用來記錄是virtual的個數
            }
        }

        /* add middle */
        bb_table_add_entry(extend_head, &merge_head->table[i]); // 再把當下的這個track在merge table的entry 加進去extend tabgle裡面

        /* add upper bound */
        next_track = track + 1;
        if ((track != (d->report.max_track_num - 1)) && (storage_is_in_use(d, next_track))) // 再來去判斷當下這個迴圈的track的後一個，判斷方式跟前一個一樣
        {
            bb_table_add(extend_head, track + 1, true);
        }
    }

    return bb_table_get_size(extend_head); // 回傳extend table裡面有幾個entry
}
// 用來把batch table做排序的，依照pba做排序，如果table裡面是空的回傳0，成功回傳1
int batch_sync(struct disk *d, b_table_head_t *h)
{
    if (h->block_head.size == 0)
    {
        return 0;
    }
    bb_head_t *bbh = &h->block_head;
    bb_entry_t *p = bbh->table;
    qsort((void *)p, bbh->size, sizeof(*p), batch_cmp);
    return 1;
}

extern bool is_csv_flag;
int _batch_read(struct disk *d, b_table_head_t *t)
{
    if (0 == batch_sync(d, t)) // 如果batch_sync return 0，代表裡面有0個block
    {
        if (is_csv_flag) // 如果is_cav_flag==1
        {
            batch_clear(t); // 把batch table的size全部清空為0
            return 0;       // return 0結束
        }
        else
        {
            fprintf(stderr, "Error: sync batch with 0 block in %s()\n", // 如果不是is_csv_flag，把錯誤訊息印到stderr，然後結束
                    __FUNCTION__);
            exit(EXIT_FAILURE);
        }
    }
    int count = pba_read(d, t); // 如果batch table是有東西的，執行pba_read，回傳做了幾個entry到count
    record_read(d, t);          // 紀錄read的一些info到d->report裡面
    return count;               // 把count回傳
}

int batch_read(struct disk *d, b_table_head_t *t)
{
    int count = _batch_read(d, t); // 呼叫_batch_read這個funciton來做read operation
    batch_clear(t);                // 做完之後把batch table清空
    return count;                  // 回傳做了幾個operation
}

int batch_write(struct disk *d, b_table_head_t *t)
{
    int count; //, merge_count;
    batch_sync(d, t);
    // assert(0 != (merge_count = batch_sync(d, t))); // 檢查batch table是不是為0個，是的話就跳出，並把個數存在merge_count裡面
#ifndef CMR // if not define CMR
    batch_extend(d, t);
    // int extend_count = batch_extend(d, t); // 呼叫batch_extend這個function來檢查top跟bottom track之間影響的問題，最後再把做好處理的extend table的總entry的個數回傳
    // assert(extend_count >= merge_count);   // 檢查extend table的個數有沒有大於原本table的個數，如果沒有的話就跳出
#endif
    count = pba_write(d, t); // 執行pba_write，回傳做了幾個entry到count
    record_write(d, t);      // 紀錄write的一些info到d->report裡面
    batch_clear(t);          // 做完之後把batch table清空
    return count;            // 回傳做了幾個operation
}

int batch_delete(struct disk *d, b_table_head_t *t)
{
    assert(0 != batch_sync(d, t)); // 檢查batch table是不是為0個，是的話就跳出
    batch_extend(d, t);            // 呼叫batch_extend這個function來檢查top跟bottom track之間影響的問題
    record_delete(d, t);           // 紀錄delete的一些info到d->report裡面
    int count = pba_delete(d, t);  // 執行pba_delete，回傳做了幾個entry到count
    batch_clear(t);                // 做完之後把batch table清空
    return count;                  // 回傳做了幾個operation
}

int batch_invalid(struct disk *d, b_table_head_t *t)
{
    assert(0 != batch_sync(d, t)); // 檢查batch table是不是為0個，是的話就跳出
#ifndef VIRTUAL_GROUPS
    batch_extend(d, t);  // 呼叫batch_extend這個function來檢查top跟bottom track之間影響的問題
    record_delete(d, t); // 紀錄delete的一些info到d->report裡面
#endif
    int count = pba_delete(d, t);
#ifdef VIRTUAL_GROUPS
    update_vg_list(d, &t->block_head);
#endif
    batch_clear(t);
    return count;
}