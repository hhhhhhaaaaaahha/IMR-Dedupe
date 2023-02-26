#pragma once
typedef enum
{
    normal_op_mode,
    journaling_op_mode,
} op_mode_t;

extern op_mode_t recording_mode;