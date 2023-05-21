#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/random.h>

#include "block_swap.h"
#include "dump.h"
#include "lba.h"
#include "op_mode.h"
#include "pba.h"
#include "top_buffer.h"
#include "parse.h"
#include "output.h"
#include "active_swap.h"

#ifdef VIRTUAL_GROUPS
extern unsigned long granularity;
#endif
bool is_csv_flag = false;
bool is_DEDU = false;
op_mode_t recording_mode = normal_op_mode;
jmp_buf env;
unsigned long long bytes;

#ifdef DEDU_ORIGIN
bool is_delete = false;
double storage_usage(struct disk *d)
{
	double sum = 0;
	for (size_t i = 0; i < d->report.max_block_num; i++)
	{
		if (d->storage[i].status == status_in_use)
			sum++;
	}
#ifdef TOP_BUFFER
	sum -= d->report.current_top_buffer_count;
#endif
	return sum / (double)d->report.max_block_num * 100;
}
FILE *fp = NULL;

#endif

void parsing_csv(struct disk *d, FILE *stream)
{
	unsigned long lba, n, val;
	char *line = NULL;
	ssize_t nread;
	size_t len;
	unsigned long fid, remain, remainder, num_bytes, left, num_traces, percent, ten_percent, total_traces;
	int c;
	struct report *report = &d->report;
	num_traces = 4500000;
	percent = num_traces / 100;
	ten_percent = num_traces / 10;
	total_traces = 0;

	while ((num_traces--) && ((nread = getline(&line, &len, stream)) != -1))
	{
		char *p = line;
		while (*p == ' ')
			p++;
		if (*p == '#')
			continue;
		if ((val = sscanf(p, "%d,%lu,%llu,%lu\n", &c, &fid, &bytes, &num_bytes)) == 4)
		{
			report->ins_count++;
			lba = bytes / BLOCK_SIZE;
			remainder = bytes % BLOCK_SIZE;
			remain = (remainder == 0 ? 0 : BLOCK_SIZE - remainder);
			n = 0;
			if (num_bytes == 0)
			{
				n = 0;
			}
			else if (remain < num_bytes)
			{
				n = !!remain;
				left = num_bytes - remain;
				n += (left / BLOCK_SIZE) + !!(left % BLOCK_SIZE);
			}
			else
			{
				n = 1;
			}
			total_traces++;
			switch (c)
			{
			case 1:
				report->read_ins_count++;
				d->d_op->read(d, lba, n, fid);
				break;
			case 2:
				report->write_ins_count++;
				d->d_op->write(d, lba, n, fid);
				break;
			case 3:
				report->delete_ins_count++;
				d->d_op->remove(d, lba, n, fid);
				break;
			default:
				fprintf(stderr,
						"ERROR: parsing instructions failed. Unrecongnized "
						"mode. mode: %d\n",
						c);
				break;
			}
		}
		else
		{
			fprintf(stderr,
					"ERROR: parsing instructions failed. Unrecongnized format.\n");
			exit(EXIT_FAILURE);
		}
		if (!(num_traces % percent))
		{
			putchar('#');
			fflush(stdout);
		}
		if (!(num_traces % ten_percent))
		{
			putchar('\n');
			fflush(stdout);
		}
	}
	free(line);
}

void parsing_postmark(struct disk *d, FILE *stream)
{
	unsigned long lba, n, val;
	char *line;
	ssize_t nread;
	size_t len;
	unsigned long fid;
	int count, c;
	struct report *report = &d->report;
	line = NULL;

	while ((nread = getline(&line, &len, stream)) != -1)
	{
		char *p = line;
		while (*p == ' ')
			p++;
		if (*p == '#')
			continue;
		if ((val = sscanf(p, "%d,%lu,%lu,%lu\n", &c, &fid, &lba, &n)) == 4)
		{
			report->ins_count++;
			switch (c)
			{
			case 1:
				report->read_ins_count++;
				count = d->d_op->read(d, lba, n, fid);
				if (count != n)
				{
					fprintf(stderr, "Error: size of input != size of output while "
									"reading\n");
					exit(EXIT_FAILURE);
				}
				break;
			case 2:
				report->write_ins_count++;
				count = d->d_op->write(d, lba, n, fid);
				if (count != n)
				{
					fprintf(stderr, "Error: size of input != size of output while "
									"writing\n");
					exit(EXIT_FAILURE);
				}
				break;
			case 3:
				report->delete_ins_count++;
				d->d_op->remove(d, lba, n, fid);
				break;
			default:
				fprintf(stderr, "ERROR: parsing instructions failed. Unrecongnized "
								"mode.\n");
				break;
			}
		}
		else
		{
			fprintf(stderr,
					"ERROR: parsing instructions failed. Unrecongnized format.\n");
			exit(EXIT_FAILURE);
		}
	}
	free(line);
}

void start_parsing(struct disk *d, char *file_name)
{
	FILE *stream;
	stream = fopen(file_name, "r");
	if (!stream)
	{
		printf("filename = %s\n", file_name);
		fprintf(stderr, "ERROR: open file failed. %s\n", strerror(errno));
		exit(EXIT_FAILURE);
		return;
	}
	if (is_csv_flag)
		parsing_csv(d, stream);
#ifdef DEDU_ORIGIN
	else if (is_DEDU)
		DEDU_parsing_csv(d, stream);
#endif
	else
		parsing_postmark(d, stream);
	fclose(stream);
}

int main(int argc, char **argv)
{
	int physical_size, logical_size, opt;
	size_t len;
	char input_file[MAX_LENS + 1], output_file[MAX_LENS + 1];
	time_t start_time, end_time; /* elapsed timers */
	opt = 0;
	logical_size = physical_size = 1000;
	/* parse arguments */
	while ((opt = getopt(argc, argv, "cds:i:g:o:l:")) != -1)
	{
		switch (opt)
		{
		case 'c':
			is_csv_flag = true;
			break;
		case 's':
			physical_size = atoi(optarg);
			break;
#ifdef DEDU_ORIGIN
		case 'd':
			is_DEDU = true;
			break;
		case 'o':
			len = strlen(optarg);
			strcpy(output_file, optarg);
			if (!(fp = fopen(output_file, "w+")))
			{
				fprintf(stderr, "ERROR: open output file failed. %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			break;
		case 'l':
			logical_size = atoi(optarg);
			break;
#endif
		case 'i':
			len = strlen(optarg);
			if (len > MAX_LENS)
				len = MAX_LENS;
			strncpy(input_file, optarg, len);
			input_file[len] = '\0';
			break;
#ifdef VIRTUAL_GROUPS
		case 'g':
			granularity = atoi(optarg);
			break;
#endif
		default:
			fprintf(stderr, "Usage: %s (-c) [-s size(GB)] [-i input_file_name]\n",
					argv[0]);
			exit(EXIT_FAILURE);
			break;
		}
	}

	/* create virtual disk */
	struct disk d = {0};
	struct report *report = &d.report;
	printf("Init disk...\n");
#ifdef DEDU_ORIGIN
	fprintf(fp, "Init disk...\n");
#endif
	if (init_disk(&d, physical_size, logical_size))
	{
		fprintf(stderr, "ERROR: init_disk failed\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("Init disk [OK]\n");
#ifdef DEDU_ORIGIN
		fprintf(fp, "Init disk [OK]\n");
#endif
	}

	/* parse operations file */
	printf("Parse instructions...\n");
#ifdef DEDU_ORIGIN
	fprintf(fp, "Parse instructions...\n");
#endif
	time(&start_time);
	start_parsing(&d, input_file);
	time(&end_time);
	printf("Parse instructions [OK]\n\n");
#ifdef DEDU_ORIGIN
	fprintf(fp, "Parse instructions [OK]\n\n");
	double usage = storage_usage(&d);
#endif

	double elapsed = difftime(end_time, start_time);
	printf("-------------------------\n");
	printf("Time information:\n\n");
	printf("%f seconds total\n", elapsed);
	printf("-------------------------\n");
	printf("Disk information:\n\n");
	printf("Size of disk = %d GB\n", physical_size);
	printf("-------------------------\n");
	printf("Transaction information:\n\n");
	printf("Total number of instructions        = %16lu instructions\n", report->ins_count);
	printf("Total number of read instructions   = %16lu instructions\n", report->read_ins_count);
	printf("Total number of write instructions  = %16lu instructions\n", report->write_ins_count);
	printf("Total number of delete instructions  = %16lu instructions\n", report->delete_ins_count);
	printf("Total number of invalid read        = %16lu blocks\n", report->num_invalid_read);
	printf("Total number of invalid write       = %16lu blocks\n", report->num_invalid_write);
	printf("Total size of read instructions   = %16lu MB\n", report->read_ins_count * SECTOR_SIZE / MEGABYTE);
	printf("Total size of write instructions  = %16lu MB\n", report->write_ins_count * SECTOR_SIZE / MEGABYTE);
	printf("\n");
#ifdef DEDU_ORIGIN
	fprintf(fp, "-------------------------\n");
	fprintf(fp, "Time information:\n\n");
	fprintf(fp, "%f seconds total\n", elapsed);
	fprintf(fp, "-------------------------\n");
	fprintf(fp, "Disk information:\n\n");
	fprintf(fp, "Size of disk = %d GB\n", physical_size);
	fprintf(fp, "-------------------------\n");
	fprintf(fp, "Transaction information:\n\n");
	fprintf(fp, "Total number of instructions        = %16lu instructions\n", report->ins_count);
	fprintf(fp, "Total number of read instructions   = %16lu instructions\n", report->read_ins_count);
	fprintf(fp, "Total number of write instructions  = %16lu instructions\n", report->write_ins_count);
	fprintf(fp, "Total number of delete instructions  = %16lu instructions\n", report->delete_ins_count);
	fprintf(fp, "Total number of invalid read        = %16lu blocks\n", report->num_invalid_read);
	fprintf(fp, "Total number of invalid write       = %16lu blocks\n", report->num_invalid_write);
	fprintf(fp, "Total size of read instructions   = %16lu MB\n", report->read_ins_count * SECTOR_SIZE / MEGABYTE);
	fprintf(fp, "Total size of write instructions  = %16lu MB\n", report->write_ins_count * SECTOR_SIZE / MEGABYTE);
	fprintf(fp, "\n");
#endif
#ifdef TOP_BUFFER
	printf("-------------------------\n");
	printf("Total write top buffer size = %19lu MB\n", report->total_write_top_buffer_size / MEGABYTE);
	printf("Total read scp size         = %19lu MB\n", report->total_read_scp_size / MEGABYTE);
	printf("Total scp count             = %19d times\n", report->scp_count);
#ifdef DEDU_ORIGIN
	fprintf(fp, "-------------------------\n");
	fprintf(fp, "Total write top buffer size = %19lu MB\n", report->total_write_top_buffer_size / MEGABYTE);
	fprintf(fp, "Total read scp size         = %19lu MB\n", report->total_read_scp_size / MEGABYTE);
	fprintf(fp, "Total scp count             = %19d times\n", report->scp_count);
#endif
#endif
#ifdef BLOCK_SWAP
	printf("Total block swap count      = %19ld blocks\n", report->current_block_swap_count);
#ifdef DEDU_ORIGIN
	fprintf(fp, "Total block swap count      = %19ld blocks\n", report->current_block_swap_count);
#endif
#elif defined DEDU_WRITE
	printf("Total block swap count      = %19ld blocks\n", report->current_block_swap_count);
	printf("Total inversion swap count      = %19ld blocks\n", report->reversion_swap_count);
#ifdef DEDU_ORIGIN
	fprintf(fp, "Total block swap count      = %19ld blocks\n", report->current_block_swap_count);
	fprintf(fp, "Total inversion swap count      = %19ld blocks\n", report->reversion_swap_count);
#endif
#endif
#ifdef VIRTUAL_GROUPS
	printf("Total dual swap count      = %19ld blocks\n", report->dual_swap_count);
#endif
	printf("#########################\n");
	printf("######## Latency ########\n");
	printf("#########################\n");
	printf("Secure Deletion Latency     = %19lu ns\n", report->total_delete_time);
	printf("Data Write Latency          = %19lu ns\n", report->normal.total_write_time);
	printf("Data Read Latency           = %19lu ns\n", report->normal.total_read_time);
	printf("#########################\n");
	printf("######### Size ##########\n");
	printf("#########################\n");
	printf("Accumulated Write Size      = %19lu B\n", report->total_write_size);
	printf("Accumulated Rewrite Size    = %19lu B\n", report->total_rewrite_size);
	printf("Accumulated Reread Size     = %19lu B\n", report->total_reread_size);
	printf("#########################\n");
	printf("Secure Deletion:\n");
	printf("Accumulated Write Size      = %19lu B\n", report->total_delete_write_size);
	printf("Accumulated Rewrite Size    = %19lu B\n", report->total_delete_rewrite_size);
	printf("Accumulated Reread Size     = %19lu B\n", report->total_delete_reread_size);
	printf("#########################\n");
	printf("Total Write Block Size      = %19lu B\n", report->normal.total_write_block_size);
	printf("Total Read Block Size       = %19lu B\n", report->normal.total_read_block_size);
	printf("Total Delete Block Size     = %19lu B\n", report->total_delete_write_block_size);
	printf("#########################\n");
	printf("Storage usage               = %19.2f %%\n", usage);
	printf("End\n");
#ifdef DEDU_ORIGIN
	fprintf(fp, "#########################\n");
	fprintf(fp, "######## Latency ########\n");
	fprintf(fp, "#########################\n");
	fprintf(fp, "Secure Deletion Latency     = %19lu ns\n", report->total_delete_time);
	fprintf(fp, "Data Write Latency          = %19lu ns\n", report->normal.total_write_time);
	fprintf(fp, "Data Read Latency           = %19lu ns\n", report->normal.total_read_time);
	fprintf(fp, "#########################\n");
	fprintf(fp, "######### Size ##########\n");
	fprintf(fp, "#########################\n");
	fprintf(fp, "Accumulated Write Size      = %19lu B\n", report->total_write_size);
	fprintf(fp, "Accumulated Rewrite Size    = %19lu B\n", report->total_rewrite_size);
	fprintf(fp, "Accumulated Reread Size     = %19lu B\n", report->total_reread_size);
	fprintf(fp, "#########################\n");
	fprintf(fp, "Secure Deletion:\n");
	fprintf(fp, "Accumulated Write Size      = %19lu B\n", report->total_delete_write_size);
	fprintf(fp, "Accumulated Rewrite Size    = %19lu B\n", report->total_delete_rewrite_size);
	fprintf(fp, "Accumulated Reread Size     = %19lu B\n", report->total_delete_reread_size);
	fprintf(fp, "#########################\n");
	fprintf(fp, "Total Write Block Size      = %19lu B\n", report->normal.total_write_block_size);
	fprintf(fp, "Total Read Block Size       = %19lu B\n", report->normal.total_read_block_size);
	fprintf(fp, "Total Delete Block Size     = %19lu B\n", report->total_delete_write_block_size);
	fprintf(fp, "#########################\n");
	fprintf(fp, "Storage usage               = %19.2f  %%\n", usage);
	fprintf(fp, "End\n");
#endif
	// DEDU_createBlockSwap(&d, 0, 8);
	// d.d_op->DEDU_write(&d, 4, 1, "fb:ed:91:2b:8d:86");
	output_ltp_table(&d);
	output_ptt_table(&d);
	output_disk_info(&d);
	end_disk(&d);
	if (fp != NULL)
		fclose(fp);
	return 0;
}
