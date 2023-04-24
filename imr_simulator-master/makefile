CC=	gcc
CXX=g++
OBJS= src/lba.c src/pba.c src/batch.c src/chs.c src/record_op.c src/rw.c src/active_swap.c src/parse.c src/output.c
		# src/top_buffer.c src/block_swap.c src/scp.c 

TOP_BUFFER_OBJS= src/top_buffer.c src/scp.c
BLOCK_SWAP_OBJS= src/block_swap.c
VG_OBJS = src/fid_table.c src/virtual_groups.c src/dump.c
VG_HISTORY_OBJS = src/ring_buffer.c
IMRS=
# CPPFLAGS=-std=c++11 -Wfatal-errors -Wall -DZALLOC -DTOP_BUFFER
CPPFLAGS=-std=c++11 -Wfatal-errors -Wall
CFLAGS=-Wfatal-errors -Wall -g
GTESTGLAGS = -lgtest -pthread
# LDFLAGS= -lgtest -lpthread -Iinclude
LDFLAGS= -Iinclude
TETFLAGS = src/lba.c src/active_swap.c src/parse.c src/top_buffer.c src/scp.c src/batch.c src/pba.c
.PHONY: clean dirs valgrind dedupe_write cmr nodedupe dedupeorigin trim gtest
# zalloc topbuffer blockswap vg blockswap_virtual
# all:gtest
all: nodedupe dedupeorigin # dedupe_write trim ray_dedupe_write ray_trim

gtest:
	$(CXX) $(CPPFLAGS) -DDEDU_WRITE test/ut_main.cpp -o bin/ut_all $(GTESTGLAGS) $(LDFLAGS) $(TETFLAGS)

nodedupe: $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DNO_DEDU test/main.c -o bin/nodedupe $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS)

dedupeorigin: $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DBLOCK_SWAP test/main.c -o bin/dedupeorigin $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS)

dedupe_write: $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DDEDU_WRITE test/main.c -o bin/dedupewrite $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS)

trim: $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DDEDU_WRITE -DTRIM test/main.c -o bin/trim $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS)

ray_dedupe_write: $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DDEDU_WRITE test/main.c -o bin/ray_dedupewrite $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS)

ray_trim: $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DDEDU_WRITE -DTRIM test/main.c -o bin/ray_trim $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS)

adjusted_write: $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DDEDU_WRITE test/main.c -o bin/adjusted_dedupewrite $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS)

adjusted_trim: $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DDEDU_WRITE -DTRIM test/main.c -o bin/adjusted_trim $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS)

cmr: $(OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DCMR test/main.c -o bin/cmr $(OBJS)

native: $(OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DNATIVE test/main.c -o bin/native $(OBJS)

zalloc: $(OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DZALLOC test/main.c -o bin/zalloc $(OBJS)

topbuffer: $(OBJS) $(TOP_BUFFER_OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DTOP_BUFFER test/main.c -o bin/topbuffer $(OBJS) $(TOP_BUFFER_OBJS)

blockswap: $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DBLOCK_SWAP test/main.c -o bin/blockswap $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS)


blockswap_virtual: $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DBLOCK_SWAP -DZALLOC_JOURNAL_VIRTUAL test/main.c -o bin/blockswap $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS)


vg: $(OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DVIRTUAL_GROUPS test/main.c $(VG_OBJS) $(OBJS) -o bin/vg

vg_history: $(OBJS) $(VG_HISTORY_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -DVIRTUAL_GROUPS -DATTR_HISTORY test/main.c $(VG_OBJS) $(OBJS) $(VG_HISTORY_OBJS) -o bin/vg_history

imr_cpp: $(OBJS)
	$(CXX) $(CPPFLAGS) -DZALLOC -DTOP_BUFFER src/main.c src/top_buffer.c src/scp.c src/block_swap.c $(OBJS) -o bin/imr $(LDFLAGS)

test100g: block_swap
	./bin/imr -s 100 -i instructions/100m_1024

test1g: native
	./bin/imr -s 1 -i instructions/1m_1024

SIZE=1
TRACE=1m_1024_333
testall: native zalloc topbuffer block_swap vg
	./bin/native -s $(SIZE) -i instructions/$(TRACE)
	./bin/zalloc -s $(SIZE) -i instructions/$(TRACE)
	./bin/topbuffer -s $(SIZE) -i instructions/$(TRACE)
	./bin/blockswap -s $(SIZE) -i instructions/$(TRACE)
	./bin/vg -s $(SIZE) -i instructions/$(TRACE)

testtopbuffer: topbuffer
	./bin/imr -s 1 -i instructions/topbuffer/1m_514_1scp

test_cpp: imr_cpp
	./bin/imr -s 10 -i test/postmark_instructions

valgrind: block_swap
	valgrind --track-origins=yes --tool=memcheck --leak-check=full bin/imr -i instructions/1m_1024 -s 1

ut_native:	dirs
	$(CXX) $(CPPFLAGS) -DNATIVE $(OBJS) test/ut_main.cpp -o bin/ut_main $(LDFLAGS)

ut_zalloc:	dirs
	$(CXX) $(CPPFLAGS) -DZALLOC $(OBJS) test/ut_main.cpp -o bin/ut_main $(LDFLAGS)

ut_top_buffer:	dirs
	$(CXX) $(CPPFLAGS) -DTOP_BUFFER $(OBJS) $(TOP_BUFFER_OBJS) test/ut_main.cpp -o bin/ut_main $(LDFLAGS)

ut_block_swap:	dirs
	$(CXX) $(CPPFLAGS) -DBLOCK_SWAP test/ut_main.cpp $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS) -o bin/ut_main $(LDFLAGS)

ut_vg: dirs
	$(CXX) $(CPPFLAGS) -DVIRTUAL_GROUPS  $(OBJS) $(VG_OBJS) test/ut_main.cpp -o bin/ut_main $(LDFLAGS)

ut_batch: dirs
	$(CXX) $(CPPFLAGS) -DNATIVE test/ut_main.cpp $(OBJS) -o bin/ut_main $(LDFLAGS)

ut_main: dirs
	$(CXX) $(CPPFLAGS) test/ut_main.cpp $(OBJS) -o bin/ut_main $(LDFLAGS)

ut_test: ut_main
	./bin/ut_main

dirs:
	mkdir -p bin
	mkdir -p report
	mkdir -p random

clean:
	rm -f bin/*
.PHONY: normalrun val randomrun nodeduperun run test_delete
EXE = blockswap
CSV = csv/
INPUT = user002_lba_v6_t2
CSVEXTEN = .csv
RANDOM = random/
TIMES = 2
NORMALMODE = 1
RANDOMMODE = 2
REPORT = report/
TXTEXTEN = .txt
DISKSIZE = 1100

run: nodeduperun dedupeoriginrun # deleterun trimrun

nodeduperun: bin/nodedupe
	# ./bin/nodedupe -d -i ../trace/user000.csv -o report/nodedupe-test.txt -s 519 -l 700
	# ./bin/nodedupe -d -i ../trace/user000_2.csv -o report/nodedupe-test_2.txt -s 425 -l 700
	# ./bin/nodedupe -d -i ../trace/user000_origin.csv -o report/nodedupe-test_origin.txt -s 425 -l 700

dedupeoriginrun: bin/dedupeorigin
	./bin/dedupeorigin -d -i ../trace/user000.csv -o report/dedupeorigin-test.txt -s 425 -l 700
	./bin/dedupeorigin -d -i ../trace/user000_2.csv -o report/dedupeorigin-test_2.txt -s 425 -l 700
	./bin/dedupeorigin -d -i ../trace/user000_origin.csv -o report/dedupeorigin-test_origin.txt -s 425 -l 700

deleterun: bin/dedupewrite
	# ./bin/dedupewrite -d -i ../trace/user000.csv -o report/deleterun-user000_425.txt -s 425 -l 425
	# ./bin/dedupewrite -d -i ../trace/user000.csv -o report/deleterun-user000_488.txt -s 488 -l 425
	
trimrun: bin/trim
	# ./bin/trim -d -i ../trace/user000.csv -o report/trim-user000_425.txt -s 425 -l 425
	# ./bin/trim -d -i ../trace/user000.csv -o report/trim-user000_488.txt -s 488 -l 425


dedurun: bin/dedupewrite
	# ./bin/dedupewrite -d 1 -i dedupe/normal/user000_128kb_h_1.csv -o report/dedupewrite-user000_128kb_h_1_425.txt -s 425 -l 425
	# ./bin/dedupewrite -d 1 -i dedupe/normal/user000_128kb_h_1.csv -o report/dedupewrite-user000_128kb_h_1_488.txt -s 488 -l 425
	./bin/dedupewrite -d 1 -i dedupe/normal/user000_128kb_h_1.csv -o report/dedupewrite-user000_128kb_h_1_781.txt -s 781 -l 425
	# ./bin/dedupewrite -d 1 -i dedupe/normal/user000_128kb_h_1.csv -o report/dedupewrite-user000_128kb_h_1_848.txt -s 848 -l 425
	# ./bin/dedupewrite -d 1 -i dedupe/normal/user002_128kb_lba_t1_half_1.csv -o report/dedupewrite-user002_128kb_lba_t1_half_1_685.txt -s 685 -l 1096
	# ./bin/dedupewrite -d 1 -i dedupe/normal/user002_128kb_lba_t1_half_1.csv -o report/dedupewrite-user002_128kb_lba_t1_half_1_1096.txt -s 1096 -l 1096
	# ./bin/dedupewrite -d 1 -i dedupe/normal/user002_128kb_lba_t1_half_1.csv -o report/dedupewrite-user002_128kb_lba_t1_half_1_2191.txt -s 2191 -l 1096

randomrun: bin/$(EXE)
	./bin/$(EXE) -d 2 -r dedupe/normal/user000_128kb_lba_t$(TIMES)_1$(CSVEXTEN) -i dedupe/random/user000_128kb_lba$(CSVEXTEN) -t $(TIMES) -o $(REPORT)$(EXE)-user000_128kb_lba_random_unused_1$(TXTEXTEN) -s 425
	./bin/$(EXE) -d 2 -r dedupe/normal/user000_128kb_lba_t$(TIMES)_2$(CSVEXTEN) -i dedupe/random/user000_128kb_lba$(CSVEXTEN) -t $(TIMES) -o $(REPORT)$(EXE)-user000_128kb_lba_random_unused_2$(TXTEXTEN) -s 425
	./bin/$(EXE) -d 2 -r dedupe/normal/user000_128kb_lba_t$(TIMES)_3$(CSVEXTEN) -i dedupe/random/user000_128kb_lba$(CSVEXTEN) -t $(TIMES) -o $(REPORT)$(EXE)-user000_128kb_lba_random_unused_3$(TXTEXTEN) -s 425
	./bin/$(EXE) -d 2 -r dedupe/normal/user002_128kb_lba_t$(TIMES)_1$(CSVEXTEN) -i dedupe/random/user002_128kb_lba$(CSVEXTEN) -t $(TIMES) -o $(REPORT)$(EXE)-user002_128kb_lba_random_unused_1$(TXTEXTEN) -s 1096
	./bin/$(EXE) -d 2 -r dedupe/normal/user002_128kb_lba_t$(TIMES)_2$(CSVEXTEN) -i dedupe/random/user002_128kb_lba$(CSVEXTEN) -t $(TIMES) -o $(REPORT)$(EXE)-user002_128kb_lba_random_unused_2$(TXTEXTEN) -s 1096
	./bin/$(EXE) -d 2 -r dedupe/normal/user002_128kb_lba_t$(TIMES)_3$(CSVEXTEN) -i dedupe/random/user002_128kb_lba$(CSVEXTEN) -t $(TIMES) -o $(REPORT)$(EXE)-user002_128kb_lba_random_unused_3$(TXTEXTEN) -s 1096

normalrun: bin/$(EXE)
	# ./bin/blockswap -d 1 -i dedupe/normal/user000_128kb_lba_t2_1.csv -o report/blockswap-user000_128kb_lba_t2.txt -s 425 -l 425
	# ./bin/blockswap -d 1 -i dedupe/normal/user002_128kb_lba_t2_1.csv -o report/blockswap-user002_128kb_lba_t2.txt -s 1096 -l 1096
	./bin/blockswap -d 1 -i dedupe/normal/user000_128kb_lba_half_1.csv -o report/blockswap-user000_128kb_lba_t1_half_99.txt -s 425 -l 425
	# ./bin/blockswap -d 1 -i dedupe/normal/user002_128kb_lba_t1_half_1.csv -o report/blockswap-user002_128kb_lba_t1_half_99.txt -s 554 -l 1096
	# ./bin/blockswap -d 1 -i dedupe/normal/user000_128kb_lba_t1_half_2.csv -o report/blockswap-user000_128kb_lba_t1_half_50.txt -s 781 -l 425
	# ./bin/blockswap -d 1 -i dedupe/normal/user002_128kb_lba_t1_half_1.csv -o report/blockswap-user002_128kb_lba_t1_half_50.txt -s 1096 -l 1096


test: bin/blockswap
	./bin/blockswap -d 1 -i dedupe/normal/test.csv -o report/blockswap-test.txt -s 425 -l 425


val: bin/$(EXE)
	valgrind ./bin/$(EXE) -d $(MODE) -i $(INPUT)$(CSVEXTEN) -t $(TIMES) -o $(RANDOM)$(INPUT)_t$(TIMES)$(CSVEXTEN)
