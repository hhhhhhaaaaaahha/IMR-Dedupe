#pragma once
#include <gtest/gtest.h>

#include "batch.h"
#include "lba.h"
#include "pba.h"
#include "scp.h"
#include "top_buffer.h"
#include "list.h"
#include "virtual_groups.h"

class VirtualGroupsTest : public ::testing::Test {
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

TEST_F(VirtualGroupsTest, VirtualGroups) {
    virtual_groups_head_t *h = &d.vgh;
    ASSERT_EQ(6, h->granularity);
    ASSERT_EQ(103, h->num_of_groups);

    int i = 0;
    virtual_group_t *t = nullptr;
    for (i = 0; i < h->num_of_groups - 1; i++) {
        t = &h->vg[i];
        ASSERT_EQ(i * h->granularity, t->lower_border);
        ASSERT_EQ(t->lower_border + h->granularity - 1, t->higher_border);
        ASSERT_EQ(i, t->vg_id);
        ASSERT_EQ(h->granularity, t->capacity);
    }

    t = &h->vg[i];
    ASSERT_EQ(i * h->granularity, t->lower_border);
    ASSERT_EQ(d.report.max_track_num, t->higher_border);
    ASSERT_EQ(i, t->vg_id);
    ASSERT_EQ(t->higher_border - t->lower_border + 1, t->capacity);
}

TEST_F(VirtualGroupsTest, FreeList) {
    struct list_head *pos;
    virtual_group_t *t = d.vgh.vg;
    int i = 0;
    list_for_each(pos, &vg_free_head.list) {
        virtual_group_t *pos_ent = list_entry(pos, virtual_group_t, list);
        ASSERT_EQ(pos_ent, t);
        t++;
        i++;
    }
    virtual_group_t *first = list_entry(vg_free_head.list.next, virtual_group_t, list);
    ASSERT_EQ(first, vg_free_head.current);
    ASSERT_EQ(i, d.vgh.num_of_groups);
}
