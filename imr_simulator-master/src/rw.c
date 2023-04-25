#include "rw.h"
#include "record_op.h"
#include "pba.h"

void rw_block(struct disk *d, unsigned long from, unsigned long to) // from: tba, to: bba
{
    // reread
    batch_add(d, from, &mbtable);
    batch_sync(d, &mbtable);
    pba_read(d, &mbtable);
    record_read(d, &mbtable);
    batch_clear(&mbtable);
    // rewrite
    batch_add(d, to, &mbtable);
    batch_sync(d, &mbtable);
    pba_write(d, &mbtable);
    batch_extend(d, &mbtable);
    record_write(d, &mbtable);
    batch_clear(&mbtable);
}