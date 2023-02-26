#pragma once
struct disk;

unsigned long find_empty_block_swap(struct disk *d);
void createBlockSwap(struct disk *d, unsigned long bba, unsigned long tba);
unsigned long run_block_swap(struct disk *d, unsigned long pba);
int init_block_swap(struct disk *d);
void block_info_swap(struct disk *d, unsigned long bba, unsigned long tba);
