#include <stdio.h>
#include <stdlib.h>
#include "lba.h"
void DEDU_parsing_csv(struct disk *d, FILE *stream)
{
    struct report *report = &d->report;
    unsigned long num_of_use_block;
    char line[MAX_LENS + 1];
    int line_cnt = 1;
    while ((fgets(line, sizeof(line), stream)) != NULL)
    {
        // printf("line_cnt: %d\n", line_cnt);
        int operation;
        char hash[20];
        uint64_t size, lba;
        sscanf(line, "%d,%s ,%ld,%ld", &operation, hash, &size, &lba);
        report->ins_count++;
        num_of_use_block = size / BLOCK_SIZE;
        if (size % BLOCK_SIZE > 0)
            num_of_use_block++;
        // if (!is_delete && operation == 3)
        // {
        // 	delete_all_bottom_track(d);
        // 	is_delete = true;
        // }
        switch (operation)
        {
        case 2:
            report->write_ins_count++;
            d->d_op->DEDU_write(d, lba, num_of_use_block, hash, line_cnt);
            break;
        case 3:
            report->delete_ins_count++;
            d->d_op->DEDU_remove(d, lba, num_of_use_block, hash);
            break;
        default:
            fprintf(stderr, "ERROR: parsing instructions failed. Unrecongnized "
                            "mode. mode: %d\n",
                    operation);
            exit(EXIT_FAILURE);
            break;
        }
        line_cnt += 1;
    }
    printf("block size = %ld\n", BLOCK_SIZE);
}