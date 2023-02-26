#pragma once

#include <gtest/gtest.h>

#include "fid_table.h"

class FidTableTest : public ::testing::Test
{
   protected:
    void SetUp() override { init_fid_table(&fthead); }

    void TearDown() override { exit_fid_table(&fthead); }

    ring_buffer_head_t fthead = {};
    virtual_group_t vg1;
};

TEST_F(FidTableTest, CreateFidTable)
{
    ring_buffer_head_t head = {};
    int result = init_fid_table(&head);
    ASSERT_EQ(10, head.capacity);
    ASSERT_EQ(0, head.count);
    ASSERT_NE(nullptr, head.table);
    ASSERT_EQ(0, result);
    free(head.table);
}

TEST_F(FidTableTest, ClearFidTalbe)
{
    ring_buffer_head_t head = {};
    init_fid_table(&head);
    exit_fid_table(&head);
    ASSERT_EQ(0, head.capacity);
    ASSERT_EQ(0, head.count);
    ASSERT_EQ(nullptr, head.table);
}

TEST_F(FidTableTest, Add1Fid)
{
    add_fid(&fthead, 1, hot_file_attr, &vg1);
    ASSERT_EQ(10, fthead.capacity);
    ASSERT_EQ(1, fthead.count);
    fte_t *e = &fthead.table[0];
    ASSERT_EQ(1, e->fid);
    ASSERT_EQ(hot_file_attr, e->attr);
    ASSERT_EQ(&vg1, e->vg);
}

TEST_F(FidTableTest, AddFids)
{
    add_fid(&fthead, 0, hot_file_attr, &vg1);
    add_fid(&fthead, 1, hot_file_attr, &vg1);
    add_fid(&fthead, 2, hot_file_attr, &vg1);
    ASSERT_EQ(10, fthead.capacity);
    ASSERT_EQ(3, fthead.count);
}

TEST_F(FidTableTest, AddDuplicateFid)
{
    add_fid(&fthead, 0, hot_file_attr, &vg1);
    add_fid(&fthead, 0, hot_file_attr, &vg1);
    ASSERT_EQ(10, fthead.capacity);
    ASSERT_EQ(1, fthead.count);
}

TEST_F(FidTableTest, IncreaseFidTable)
{
    for (int i = 0; i < 11; i++)
        add_fid(&fthead, i, hot_file_attr, &vg1);
    ASSERT_EQ(20, fthead.capacity);
    ASSERT_EQ(11, fthead.count);
}

TEST_F(FidTableTest, FindFidSuccess)
{
    for (int i = 0; i < 9; i++)
        add_fid(&fthead, i, hot_file_attr, &vg1);
    ASSERT_NE(nullptr, get_attr(&fthead, 1));
}

TEST_F(FidTableTest, FindFidFail)
{
    for (int i = 0; i < 9; i++)
        add_fid(&fthead, i, hot_file_attr, &vg1);
    ASSERT_EQ(nullptr, get_attr(&fthead, 9));
}
