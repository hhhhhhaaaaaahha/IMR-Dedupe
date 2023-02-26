#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static inline bool check_data_corruption(bool v)
{
    return v;
}

#define CHECK_DATA_CORRUPTION(condition, fmt, ...) \
    check_data_corruption(({                       \
        bool corruption = (condition);             \
        if (corruption)                            \
        {                                          \
            fprintf(stderr, fmt, ##__VA_ARGS__);   \
        }                                          \
        corruption;                                \
    }))
