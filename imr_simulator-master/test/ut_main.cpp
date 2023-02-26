#include <gtest/gtest.h>
// #include "op_mode.h"
// #ifdef ZALLOC
// #include "ut_zalloc.h"
// #ifdef TOP_BUFFER
// #include "ut_scp.h"
// #include "ut_top_buffer.h"
// #ifdef BLOCK_SWAP
// #include "ut_block_swap.h"
// #endif
// #endif
// #elif defined(VIRTUAL_GROUPS)
// #include "ut_fid_table.h"
// #include "ut_virtual_groups.h"
// #endif

// #include "ut_batch.h"

// op_mode_t recording_mode = normal_op_mode;
// bool is_csv_flag = false;
#include "ut_swap.h"

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
