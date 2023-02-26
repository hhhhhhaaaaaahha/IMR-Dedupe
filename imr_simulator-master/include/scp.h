#pragma once
#include "lba.h"

size_t run_scp(struct disk *d, unsigned long track);
unsigned long find_victim_track(struct disk *d);
size_t scp(struct disk *d);
int init_scp(struct disk *d);
void end_scp();

extern unsigned long current_scp_track;
extern bool scp_mode;