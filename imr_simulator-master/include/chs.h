#ifndef CHS_H
#define CHS_H

#include <stdbool.h>
#include <stddef.h>

struct disk;
int chs_read(struct disk *d, unsigned long pba);
void chs_write(struct disk *d, unsigned long pba);
void chs_delete(struct disk *d, unsigned long pba);

#endif