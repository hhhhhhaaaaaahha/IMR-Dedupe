#pragma once
#include <gtest/gtest.h>
#include "batch.h"
#include "lba.h"
#include "pba.h"
#include "record_op.h"

class BatchTest : public ::testing::Test
{
   protected:
    void SetUp() override { init_disk(&d, 1); }

    void TearDown() override { end_disk(&d); }

    struct disk d = {0};
};

TEST_F(BatchTest, OneTopBlock)
{
    d.storage[1].status = status_in_use;
    batch_add(&d, 1, &gbtable);
    batch_extend(&d, &gbtable);
    ASSERT_EQ(1, gbtable.extend_head.size);
    pba_write(&d, &gbtable);
    record_write(&d, &gbtable);
    ASSERT_EQ(d.report.normal.total_write_block_size,
              d.report.total_write_size);
    batch_clear(&gbtable);
}

TEST_F(BatchTest, OneBottomBlock)
{
    d.storage[0].status = status_in_use;
    batch_add(&d, 0, &gbtable);
    batch_extend(&d, &gbtable);
    ASSERT_EQ(1, gbtable.extend_head.size);
    pba_write(&d, &gbtable);
    record_write(&d, &gbtable);
    ASSERT_EQ(d.report.normal.total_write_block_size,
              d.report.total_write_size);
    batch_clear(&gbtable);
}

TEST_F(BatchTest, OneBottomOneRewriteBlock)
{
    d.storage[0].status = status_in_use;
    d.storage[1].status = status_in_use;
    batch_add(&d, 0, &gbtable);
    batch_extend(&d, &gbtable);
    ASSERT_EQ(2, gbtable.extend_head.size);
    pba_write(&d, &gbtable);
    record_write(&d, &gbtable);
    ASSERT_EQ(d.report.normal.total_write_block_size,
              d.report.total_write_size);
    batch_clear(&gbtable);
}

TEST_F(BatchTest, OneBottomTwoRewriteBlock)
{
    d.storage[1].status = status_in_use;
    d.storage[2].status = status_in_use;
    d.storage[3].status = status_in_use;
    batch_add(&d, 2, &gbtable);
    batch_extend(&d, &gbtable);
    ASSERT_EQ(3, gbtable.extend_head.size);
    pba_write(&d, &gbtable);
    record_write(&d, &gbtable);
    ASSERT_EQ(d.report.normal.total_write_block_size,
              d.report.total_write_size);
    batch_clear(&gbtable);
}

TEST_F(BatchTest, TwoBottomBlock)
{
    d.storage[0].status = status_in_use;
    d.storage[2].status = status_in_use;
    batch_add(&d, 0, &gbtable);
    batch_add(&d, 2, &gbtable);
    batch_extend(&d, &gbtable);
    ASSERT_EQ(2, gbtable.extend_head.size);
    pba_write(&d, &gbtable);
    record_write(&d, &gbtable);
    ASSERT_EQ(d.report.normal.total_write_block_size,
              d.report.total_write_size);
    batch_clear(&gbtable);
}

TEST_F(BatchTest, TwoBottomOneRewriteBlock1)
{
    d.storage[1].status = status_in_use;
    d.storage[2].status = status_in_use;
    d.storage[4].status = status_in_use;
    batch_add(&d, 2, &gbtable);
    batch_add(&d, 4, &gbtable);
    batch_extend(&d, &gbtable);
    ASSERT_EQ(3, gbtable.extend_head.size);
    pba_write(&d, &gbtable);
    record_write(&d, &gbtable);
    ASSERT_EQ(d.report.normal.total_write_block_size,
              d.report.total_write_size);
    batch_clear(&gbtable);
}

TEST_F(BatchTest, TwoBottomOneRewriteBlock2)
{
    d.storage[1].status = status_in_use;
    d.storage[2].status = status_in_use;
    d.storage[3].status = status_in_use;
    d.storage[4].status = status_in_use;
    batch_add(&d, 2, &gbtable);
    batch_add(&d, 4, &gbtable);
    batch_extend(&d, &gbtable);
    ASSERT_EQ(4, gbtable.extend_head.size);
    pba_write(&d, &gbtable);
    record_write(&d, &gbtable);
    ASSERT_EQ(d.report.normal.total_write_block_size,
              d.report.total_write_size);
    batch_clear(&gbtable);
}

TEST_F(BatchTest, TwoBottomOneRewriteBlock3)
{
    d.storage[1].status = status_in_use;
    d.storage[2].status = status_in_use;
    d.storage[3].status = status_in_use;
    d.storage[4].status = status_in_use;
    d.storage[5].status = status_in_use;
    batch_add(&d, 2, &gbtable);
    batch_add(&d, 4, &gbtable);
    batch_extend(&d, &gbtable);
    ASSERT_EQ(5, gbtable.extend_head.size);
    pba_write(&d, &gbtable);
    record_write(&d, &gbtable);
    ASSERT_EQ(d.report.normal.total_write_block_size,
              d.report.total_write_size);
    batch_clear(&gbtable);
}
