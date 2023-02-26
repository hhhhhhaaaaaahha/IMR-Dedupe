#include "lba.h"
unsigned long DEDU_run_block_swap(struct disk *d, unsigned long bba);
void DEDU_createBlockSwap(struct disk *d, unsigned long from, unsigned long to);