#pragma once
#include <gtest/gtest.h>

#include "batch.h"
#include "lba.h"
#include "pba.h"
#include "top_buffer.h"

class TopBufferTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        init_disk(&d, 1);
        current_top_buffer_track = 1;
    }

    void TearDown() override
    {
        end_disk(&d);
        memset(&d, 0, sizeof(d));
    }

    struct disk d = {0};
    unsigned long pba;
};

TEST_F(TopBufferTest, find_empty_top_buffer)
{
    find_empty_top_buffer(&d, &pba);
    ASSERT_EQ(512, pba);
}

TEST_F(TopBufferTest, is_top_buffer_full)
{
    d.report.current_top_buffer_count = d.report.max_top_buffer_num;
    ASSERT_EQ(true, is_top_buffer_full(&d));
}

TEST_F(TopBufferTest, create_top_buffer)
{
    create_top_buffer(&d, 0, 512);
    ASSERT_EQ(status_booked, d.storage[512].status);
    ASSERT_EQ(buffered_type, d.ptt_table_head->table[512].type);
    ASSERT_EQ(0, d.ptt_table_head->table[512].scp_pba);
}

TEST_F(TopBufferTest, one_track_buffered)
{
    size_t count;
    struct report *r = &d.report;
    enable_top_buffer = true;

    count = 256 * SECTOR_PER_TRACK;
    d.d_op->write(&d, 0, count, 0);
    ASSERT_EQ(count, r->total_write_actual_size / SECTOR_SIZE);
    ASSERT_EQ(0, r->total_read_virtual_size);
    ASSERT_EQ(0, r->total_write_virtual_size);
    // into phase 2
    d.d_op->write(&d, count, 1, 0);
    // top_buffer a track
    d.d_op->write(&d, 0, SECTOR_PER_TRACK, 0);
    ASSERT_EQ(SECTOR_PER_TRACK, r->current_top_buffer_count);
}

TEST_F(TopBufferTest, trigger_top_buffer)
{
    size_t count, count2, tb_count;
    struct report *r = &d.report;
    enable_top_buffer = true;
    tb_count = r->max_top_buffer_num;

    count = 256 * SECTOR_PER_TRACK;
    d.d_op->write(&d, 0, count, 0);
    count2 = count - d.report.max_top_buffer_num;
    // reach 98% blocks
    d.d_op->write(&d, count, count2, 0);
    ASSERT_EQ(0, r->current_top_buffer_count);
    // create max top buffer
    d.d_op->write(&d, 0, tb_count, 0);
    ASSERT_EQ(tb_count, r->current_top_buffer_count);
    ASSERT_EQ(0, r->scp_count);
}

TEST_F(TopBufferTest, trigger_scp)
{
    size_t count, count2, tb_count;
    struct report *r = &d.report;
    enable_top_buffer = true;
    tb_count = r->max_top_buffer_num;

    count = 256 * SECTOR_PER_TRACK;
    d.d_op->write(&d, 0, count, 0);
    count2 = count - d.report.max_top_buffer_num - 390;
    // reach 98% blocks
    d.d_op->write(&d, count, count2, 0);
    // create max top buffer
    d.d_op->write(&d, 0, tb_count, 0);
    ASSERT_EQ(0, r->scp_count);
    // trigger scp
    d.d_op->write(&d, tb_count, SECTOR_PER_TRACK, 0);
    ASSERT_EQ(tb_count, r->current_top_buffer_count);
    ASSERT_EQ(1, r->scp_count);
}

TEST_F(TopBufferTest, close_top_buffer)
{
    size_t count, count2, tb_count;
    struct report *r = &d.report;
    enable_top_buffer = true;
    tb_count = r->max_top_buffer_num;

    count = 256 * SECTOR_PER_TRACK;
    printf("1\n");
    d.d_op->write(&d, 0, count, 0);
    // number of 390 is to align a track
    count2 = count - d.report.max_top_buffer_num - 390;
    // reach 98% - 390 blocks
    printf("2\n");

    d.d_op->write(&d, count, count2, 0);
    // create max top buffer
    printf("3\n");
    d.d_op->write(&d, 0, tb_count, 0);
    ASSERT_EQ(true, enable_top_buffer);
    d.d_op->write(&d, count + count2, SECTOR_PER_TRACK, 0);
    ASSERT_EQ(tb_count, r->current_top_buffer_count);
    d.d_op->write(&d, tb_count, 1, 0);
    ASSERT_EQ(0, d.report.scp_count);
}