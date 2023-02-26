#pragma once
#include <gtest/gtest.h>

#include "batch.h"
#include "lba.h"
#include "pba.h"

class ZallocTest : public ::testing::Test
{
   protected:
    void SetUp() override { init_disk(&d, 1); }

    void TearDown() override { end_disk(&d); }

    struct disk d = {0};
};

TEST_F(ZallocTest, find_next_pba)
{
    unsigned long pba = find_next_pba(&d, 0, 0, 0, 1);
    ASSERT_EQ(0, pba);
}

TEST_F(ZallocTest, pba_search)
{
    unsigned long pba = pba_search(&d, 0, 0, 1);
    ASSERT_EQ(0, pba);
    for (int i = 1; i < 512; i++) {
        pba = pba_search(&d, i, 0, 1);
        ASSERT_EQ(i, pba);
    }
    pba = pba_search(&d, 512, 0, 1);
    ASSERT_EQ(1024, pba);
    pba = pba_search(&d, 513, 0, 1);
    ASSERT_EQ(1025, pba);
    ASSERT_EQ(0, d.ltp_table_head->table[0].pba);
    ASSERT_EQ(1024, d.ltp_table_head->table[512].pba);
    ASSERT_EQ(1025, d.ltp_table_head->table[513].pba);
}

TEST_F(ZallocTest, batch_sync)
{
    unsigned long pba = 0;
    ASSERT_EQ(0, gbtable.merge_head.size);
    for (int i = 0; i < 512; i++) {
        pba = pba_search(&d, i, 0, 1);
        ASSERT_EQ(i, pba);
        batch_add(&d, pba, &gbtable);
    }
    for (int i = 512; i < 1024; i++) {
        pba = pba_search(&d, i, 0, 1);
        ASSERT_EQ(512 + i, pba);
        batch_add(&d, pba, &gbtable);
    }
    pba = pba_search(&d, 1024, 0, 1);
    ASSERT_EQ(2048, pba);
    batch_add(&d, pba, &gbtable);
    batch_sync(&d, &gbtable);
    ASSERT_EQ(3, gbtable.merge_head.size);
    ASSERT_EQ(0, gbtable.merge_head.table[0].pba);
    ASSERT_EQ(512, gbtable.merge_head.table[0].size);
    ASSERT_EQ(1024, gbtable.merge_head.table[1].pba);
    ASSERT_EQ(512, gbtable.merge_head.table[1].size);
    ASSERT_EQ(2, gbtable.merge_head.table[1].track);
    ASSERT_EQ(2048, gbtable.merge_head.table[2].pba);
    ASSERT_EQ(1, gbtable.merge_head.table[2].size);
    ASSERT_EQ(4, gbtable.merge_head.table[2].track);
    batch_clear(&gbtable);
}

TEST_F(ZallocTest, lba_write)
{
    size_t total = lba_write(&d, 0, 1025, 0);
    ASSERT_EQ(1025, total);
}

TEST_F(ZallocTest, lba_read)
{
    lba_write(&d, 0, 1025, 0);
    size_t total = lba_read(&d, 0, 1025, 0);
    ASSERT_EQ(1025, total);
}

TEST_F(ZallocTest, lba_write_move_out)
{
    size_t total = lba_write(&d, 0, 2048, 0);
    ASSERT_EQ(2048, total);
    ASSERT_EQ(0, d.report.total_read_virtual_size);
    total = lba_write(&d, 0, 1536, 0);
    ASSERT_EQ(0, d.report.total_read_virtual_size);
    ASSERT_EQ((2048 + 1536) * SECTOR_SIZE, d.report.total_write_actual_size);
}

TEST_F(ZallocTest, phase_boundary)
{
    ASSERT_EQ(510, d.zinfo.phase1_end);
    ASSERT_EQ(511, d.zinfo.phase2_start);
    ASSERT_EQ(3, d.zinfo.phase2_end);
    ASSERT_EQ(1, d.zinfo.phase3_start);
    ASSERT_EQ(509, d.zinfo.phase3_end);
}

TEST_F(ZallocTest, intoPhase3)
{
    unsigned long lba, pba;
    for (size_t i = 0; i < 256; i++) {
        for (size_t j = 0; j < SECTOR_PER_TRACK; j++) {
            lba = chs_to_lba(i, j);
            pba = chs_to_lba(2 * i, j);
            ASSERT_EQ(status_free, d.storage[pba].status);
            d.d_op->write(&d, lba, 1, 1);
            ASSERT_EQ(status_in_use, d.storage[pba].status);
        }
    }
    unsigned long track = d.zinfo.phase2_start;
    for (size_t i = 256; i < 384; i++) {
        for (size_t j = 0; j < SECTOR_PER_TRACK; j++) {
            pba = chs_to_lba(track, j);
            ASSERT_EQ(status_free, d.storage[pba].status);
            d.d_op->write(&d, chs_to_lba(i, j), 1, 1);
            ASSERT_EQ(status_in_use, d.storage[pba].status);
        }
        track -= 4;
    }
    track = d.zinfo.phase3_start;
    for (size_t i = 384; i < 512; i++) {
        for (size_t j = 0; j < SECTOR_PER_TRACK; j++) {
            pba = chs_to_lba(track, j);
            ASSERT_EQ(status_free, d.storage[pba].status);
            d.d_op->write(&d, chs_to_lba(i, j), 1, 1);
            pba = lba_to_pba(&d, lba);
            ASSERT_EQ(status_in_use, d.storage[pba].status);
        }
        track += 4;
    }
}
