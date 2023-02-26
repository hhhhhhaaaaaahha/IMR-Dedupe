#pragma once

#include <gtest/gtest.h>

#include "batch.h"
#include "lba.h"
#include "pba.h"
#include "scp.h"
#include "top_buffer.h"

extern b_table_head_t scp_top_mtable;
extern b_table_head_t scp_bottom_mtable;

class SCPTest : public ::testing::Test {
   protected:
    void SetUp() override {
        memset(&d, 0, sizeof(d));
        init_disk(&d, 1);
    }

    void TearDown() override {
        end_disk(&d);
    }

    struct disk d;
};

TEST_F(SCPTest, init_scp) {
    bb_head_t *bh = &scp_top_mtable.block_head;
    bt_head_t *mh = &scp_top_mtable.merge_head;
    bt_head_t *eh = &scp_top_mtable.extend_head;
    ASSERT_EQ(0, bh->size);
    ASSERT_EQ(0, mh->size);
    ASSERT_EQ(0, eh->size);
    ASSERT_EQ(4096, bh->capacity);
    ASSERT_EQ(4096, mh->capacity);
    ASSERT_EQ(3 * 4096, eh->capacity);
    bh = &scp_bottom_mtable.block_head;
    mh = &scp_bottom_mtable.merge_head;
    eh = &scp_bottom_mtable.extend_head;
    ASSERT_EQ(0, bh->size);
    ASSERT_EQ(0, mh->size);
    ASSERT_EQ(0, eh->size);
    ASSERT_EQ(4096, bh->capacity);
    ASSERT_EQ(4096, mh->capacity);
    ASSERT_EQ(3 * 4096, eh->capacity);
}

TEST_F(SCPTest, trigger_scp) {
    size_t count;
    struct report *r = &d.report;

    count = 256 * SECTOR_PER_TRACK; 
    d.d_op->write(&d, 0, count, 0);
    // into phase 2
    d.d_op->write(&d, count, 1, 0);
    // fill top buffer
    d.d_op->write(&d, 0, 5242, 0);
    ASSERT_EQ(0, r->total_read_scp_size);
    // trigger scp
    d.d_op->write(&d, 5242, 1, 0);
    ASSERT_EQ(TRACK_SIZE, r->total_read_scp_size);
}

#if 0
TEST_F(SCPTest, run_scp) {
    unsigned long pba;
    find_empty_top_buffer(&d, &pba);
    d.d_op->write(&d, 0, 1, 0);
    ASSERT_EQ(1, d.report.current_use_block_num);
    enable_top_buffer = true;
    d.zinfo.phases = zalloc_phase2;
    /* write to top buffered track */
    d.d_op->write(&d, 0, 1, 0);
    ASSERT_EQ(top_buffer_type, d.ptt_table_head->table[0].type);
    ASSERT_EQ(0, d.ptt_table_head->table[pba].scp_pba);
    ASSERT_EQ(2, d.report.current_use_block_num);
    ASSERT_EQ(status_in_use, d.storage[pba].status);
    /* SCP */
    run_scp(&d, lba_to_track(pba));
    ASSERT_EQ(normal_type, d.ptt_table_head->table[0].type);
    ASSERT_EQ(1, d.report.current_use_block_num);
    ASSERT_EQ(status_free, d.storage[pba].status);
}

TEST_F(SCPTest, scp) {
    unsigned long pba;
    find_empty_top_buffer(&d, &pba);
    d.d_op->write(&d, 0, 2, 0);
    ASSERT_EQ(2, d.report.current_use_block_num);
    enable_top_buffer = true;
    d.zinfo.phases = zalloc_phase2;
    /* write to top buffered track */
    d.d_op->write(&d, 0, 2, 0);
    ASSERT_EQ(4, d.report.current_use_block_num);
    ASSERT_EQ(top_buffer_type, d.ptt_table_head->table[0].type);
    ASSERT_EQ(top_buffer_type, d.ptt_table_head->table[1].type);
    ASSERT_EQ(buffered_type, d.ptt_table_head->table[pba].type);
    ASSERT_EQ(buffered_type, d.ptt_table_head->table[pba + 1].type);
    ASSERT_EQ(0, d.ptt_table_head->table[pba].scp_pba);
    ASSERT_EQ(1, d.ptt_table_head->table[pba + 1].scp_pba);
    scp(&d);
    ASSERT_EQ(normal_type, d.ptt_table_head->table[0].type);
    ASSERT_EQ(2, d.report.current_use_block_num);
    ASSERT_EQ(status_free, d.storage[pba].status);
}

TEST_F(SCPTest, scp2) {
    unsigned long pba;
    find_empty_top_buffer(&d, &pba);
    d.d_op->write(&d, 0, 80000, 0);
    ASSERT_EQ(normal_type, d.ptt_table_head->table[pba].type);
    ASSERT_EQ(zalloc_phase1, d.zinfo.phases);
    d.d_op->write(&d, 5243, 1, 0);
    ASSERT_EQ(normal_type, d.ptt_table_head->table[pba].type);
}
#endif