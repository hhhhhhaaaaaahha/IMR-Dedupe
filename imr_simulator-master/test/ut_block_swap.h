#pragma once

#include <gtest/gtest.h>

#include "batch.h"
#include "block_swap.h"
#include "lba.h"
#include "pba.h"
#include "top_buffer.h"

class BlockSwapTest : public ::testing::Test {
protected:
    void SetUp() override {
        init_disk(&d, 1);
    }

    void TearDown() override {
        end_disk(&d);
    }

    struct disk d = {0};
};

TEST_F(BlockSwapTest, trigger_block_swap) {
    size_t count, count2;
    // struct report *r = &d.report;
    enable_top_buffer = true;

    count = 256 * SECTOR_PER_TRACK; 
    d.d_op->write(&d, 0, count, 0);
    count2 = count - d.report.max_top_buffer_num;
    // trigger block swap
    d.d_op->write(&d, count, count2, 0);
    ASSERT_EQ(false, enable_block_swap);
    d.d_op->write(&d, count + count2, 1, 0);
    ASSERT_EQ(true, enable_block_swap);
}
