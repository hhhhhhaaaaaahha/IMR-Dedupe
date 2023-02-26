#pragma once
#include <gtest/gtest.h>

#include "active_swap.h"
#include "parse.h"

class ActiveSwapTest : public ::testing::Test
{
protected:
    struct disk d = {0};
    void SetUp() override
    {
        init_disk(&d, 1, 1);
        FILE *fp = fopen("trace/test.csv", "r");
        if (!fp)
        {
            fprintf(stderr, "Error: No such file or directory!\n");
            exit(EXIT_FAILURE);
        }
        DEDU_parsing_csv(&d, fp);
    }
    void TearDown() override
    {
        end_disk(&d);
        memset(&d, 0, sizeof(d));
    }
};

TEST_F(ActiveSwapTest, normalswap)
{
    ASSERT_EQ(d.report.current_use_block_num, 256);
}