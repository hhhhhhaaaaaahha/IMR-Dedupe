#pragma once
#include <gtest/gtest.h>
#include "batch.h"
#include "lba.h"
#include "pba.h"
#include "top_buffer.h"

class NativeTest : public ::testing::Test
{
   protected:
    void SetUp() override { init_disk(&d, 1); }

    void TearDown() override { end_disk(&d); }

    struct disk d = {0};
};

TEST_F(NativeTest, pba_search)
{
    unsigned long pba = pba_search(&d, 0, 0, 1);
    ASSERT_EQ(0, pba);
    pba = pba_search(&d, 512, 0, 1);
    ASSERT_EQ(512, pba);
    pba = pba_search(&d, 1024, 0, 1);
    ASSERT_EQ(1024, pba);
    ASSERT_EQ(0, d.ltp_table_head->table[0].pba);
    ASSERT_EQ(512, d.ltp_table_head->table[512].pba);
    ASSERT_EQ(1024, d.ltp_table_head->table[1024].pba);
}

TEST_F(NativeTest, create_disk)
{
    ASSERT_EQ(0, d.report.total_read_virtual_size);
}

TEST_F(NativeTest, batch_add)
{
    for (int i = 0; i <= 1024; i++) {
        unsigned long pba = pba_search(&d, i, 0, 1);
        batch_add(&d, pba, &gbtable);
    }
    for (int i = 0; i <= 1024; i++) {
        ASSERT_EQ(i, gbtable.block_head.table[i].pba);
    }
    batch_clear(&gbtable);
}

TEST_F(NativeTest, batch_sync)
{
    ASSERT_EQ(0, gbtable.merge_head.size);
    for (int i = 0; i <= 1024; i++) {
        unsigned long pba = pba_search(&d, i, 0, 1);
        batch_add(&d, pba, &gbtable);
    }
    batch_sync(&d, &gbtable);
    EXPECT_EQ(3, gbtable.merge_head.size);
    EXPECT_EQ(0, gbtable.merge_head.table[0].pba);
    EXPECT_EQ(512, gbtable.merge_head.table[0].size);
    EXPECT_EQ(512, gbtable.merge_head.table[1].pba);
    EXPECT_EQ(512, gbtable.merge_head.table[1].size);
    EXPECT_EQ(1, gbtable.merge_head.table[1].track);
    EXPECT_EQ(1024, gbtable.merge_head.table[2].pba);
    EXPECT_EQ(1, gbtable.merge_head.table[2].size);
    batch_clear(&gbtable);
}

TEST_F(NativeTest, lba_read)
{
    lba_write(&d, 0, 1025, 0);
    size_t total = lba_read(&d, 0, 1025, 0);
    ASSERT_EQ(1025, total);
}

TEST_F(NativeTest, lba_write)
{
    size_t total = lba_write(&d, 0, 1025, 0);
    ASSERT_EQ(1025, total);
}

TEST_F(NativeTest, lba_write_move_out)
{
    ASSERT_EQ(0, d.report.total_read_virtual_size);
    size_t total = lba_write(&d, 0, 2048, 0);
    ASSERT_EQ(2048, total);
    ASSERT_EQ(0, d.report.total_read_virtual_size);
    total = lba_write(&d, 0, 1536, 0);
    ASSERT_EQ(4 * TRACK_SIZE, d.report.total_read_virtual_size);
}

TEST_F(NativeTest, lba_write_move_out2)
{
    size_t total = lba_write(&d, 0, 4 * SECTOR_PER_TRACK, 0);
    ASSERT_EQ(4 * SECTOR_PER_TRACK, total);
    ASSERT_EQ(0, d.report.total_read_virtual_size);
    total = lba_write(&d, 1024, 1, 0);
    ASSERT_EQ(3 * TRACK_SIZE, d.report.total_read_virtual_size);
}

TEST_F(NativeTest, write_whole_disk)
{
    for (size_t i = 0; i < d.report.max_block_num; i++)
        d.d_op->write(&d, i, 1, 0);
}
