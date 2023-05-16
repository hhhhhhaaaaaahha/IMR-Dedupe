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
all: nodedupe dedupeorigin dedupe_write trim  trim_v2

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

trim_v2: $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS) dirs
	$(CC) $(CFLAGS) $(LDFLAGS) -DDEDU_WRITE -DTRIM -DTRIM_V2 test/main.c -o bin/trim_v2 $(OBJS) $(TOP_BUFFER_OBJS) $(BLOCK_SWAP_OBJS)



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

run: deleterun #dedupeoriginrun trimrun trimv2run # nodeduperun

nodeduperun: bin/nodedupe
	# ./bin/nodedupe -d -i ../trace/normal/normal_70.csv -o report/ray/normal_70/nodedupe-normal_70_640.txt -s 640 -l 2000

dedupeoriginrun: bin/dedupeorigin
	# ./bin/dedupeorigin -d -i ../trace/normal/normal_70.csv -o report/ray/normal_70/450/dedupeorigin-normal_70_450.txt -s 450 -l 2000
	
	# ./bin/dedupeorigin -d -i ../trace/normal/normal_80.csv -o report/ray/normal_80/550/dedupeorigin-normal_80_550.txt -s 550 -l 2000
	# ./bin/dedupeorigin -d -i ../trace/normal/normal_80.csv -o report/ray/normal_80/470/dedupeorigin-normal_80_470.txt -s 470 -l 2000

deleterun: bin/dedupewrite
	./bin/dedupewrite -d -i ../trace/normal/normal_10.csv -o report/ray/normal_10/43/deleterun-normal_10_43.txt -s 43 -l 2000
	./bin/dedupewrite -d -i ../trace/normal/normal_10.csv -o report/ray/normal_10/37/deleterun-normal_10_37.txt -s 37 -l 2000
	
	# ./bin/dedupewrite -d -i ../trace/normal/normal_15.csv -o report/ray/normal_15/63/deleterun-normal_15_63.txt -s 63 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_15.csv -o report/ray/normal_15/55/deleterun-normal_15_55.txt -s 55 -l 2000
	
	# ./bin/dedupewrite -d -i ../trace/normal/normal_20.csv -o report/ray/normal_20/78/deleterun-normal_20_78.txt -s 78 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_20.csv -o report/ray/normal_20/88/deleterun-normal_20_88.txt -s 88 -l 2000
	
	# ./bin/dedupewrite -d -i ../trace/normal/normal_25.csv -o report/ray/normal_25/100/deleterun-normal_25_100.txt -s 100 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_25.csv -o report/ray/normal_25/110/deleterun-normal_25_110.txt -s 110 -l 2000
	
	# ./bin/dedupewrite -d -i ../trace/normal/normal_30.csv -o report/ray/normal_30/280/deleterun-normal_30_280.txt -s 280 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_30.csv -o report/ray/normal_30/250/deleterun-normal_30_250.txt -s 250 -l 2000
	
	# ./bin/dedupewrite -d -i ../trace/normal/normal_35.csv -o report/ray/normal_35/150/deleterun-normal_35_150.txt -s 150 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_35.csv -o report/ray/normal_35/170/deleterun-normal_35_170.txt -s 170 -l 2000
	
	# ./bin/dedupewrite -d -i ../trace/normal/normal_40.csv -o report/ray/normal_40/275/deleterun-normal_40_275.txt -s 275 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_40.csv -o report/ray/normal_40/235/deleterun-normal_40_235.txt -s 235 -l 2000
	
	# ./bin/dedupewrite -d -i ../trace/normal/normal_45.csv -o report/ray/normal_45/242/deleterun-normal_45_242.txt -s 242 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_45.csv -o report/ray/normal_45/211/deleterun-normal_45_211.txt -s 211 -l 2000
	
	# ./bin/dedupewrite -d -i ../trace/normal/normal_50.csv -o report/ray/normal_50/245/deleterun-normal_50_245.txt -s 245 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_50.csv -o report/ray/normal_50/285/deleterun-normal_50_285.txt -s 285 -l 2000
	
	# ./bin/dedupewrite -d -i ../trace/normal/normal_55.csv -o report/ray/normal_55/340/deleterun-normal_55_340.txt -s 340 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_55.csv -o report/ray/normal_55/298/deleterun-normal_55_298.txt -s 298 -l 2000
	
	# ./bin/dedupewrite -d -i ../trace/normal/normal_60.csv -o report/ray/normal_60/340/deleterun-normal_60_340.txt -s 340 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_60.csv -o report/ray/normal_60/385/deleterun-normal_60_385.txt -s 385 -l 2000
	
	# ./bin/dedupewrite -d -i ../trace/normal/normal_65.csv -o report/ray/normal_65/445/deleterun-normal_65_445.txt -s 445 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_65.csv -o report/ray/normal_65/385/deleterun-normal_65_385.txt -s 385 -l 2000
	
	# ./bin/dedupewrite -d -i ../trace/normal/normal_70.csv -o report/ray/normal_70/450/deleterun-normal_70_450.txt -s 450 -l 2000
	
	# ./bin/dedupewrite -d -i ../trace/normal/normal_75.csv -o report/ray/normal_75/570/deleterun-normal_75_570.txt -s 570 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_75.csv -o report/ray/normal_75/500/deleterun-normal_75_500.txt -s 500 -l 2000
	
	# ./bin/dedupewrite -d -i ../trace/normal/normal_80.csv -o report/ray/normal_80/550/deleterun-normal_80_550.txt -s 550 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_80.csv -o report/ray/normal_80/470/deleterun-normal_80_470.txt -s 470 -l 2000
	
	# ./bin/dedupewrite -d -i ../trace/normal/normal_85.csv -o report/ray/normal_85/550/deleterun-normal_85_550.txt -s 550 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_85.csv -o report/ray/normal_85/635/deleterun-normal_85_635.txt -s 635 -l 2000
	
	# ./bin/dedupewrite -d -i ../trace/normal/normal_90.csv -o report/ray/normal_90/760/deleterun-normal_90_760.txt -s 760 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_90.csv -o report/ray/normal_90/670/deleterun-normal_90_670.txt -s 670 -l 2000

	# ./bin/dedupewrite -d -i ../trace/normal/normal_95.csv -o report/ray/normal_95/580/deleterun-normal_95_580.txt -s 580 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_95.csv -o report/ray/normal_95/630/deleterun-normal_95_630.txt -s 630 -l 2000

	# ./bin/dedupewrite -d -i ../trace/normal/normal_100.csv -o report/ray/normal_100/850/deleterun-normal_100_850.txt -s 850 -l 2000
	# ./bin/dedupewrite -d -i ../trace/normal/normal_100.csv -o report/ray/normal_100/785/deleterun-normal_100_785.txt -s 785 -l 2000

trimrun: bin/trim	
	# ./bin/trim -d -i ../trace/normal/normal_60.csv -o report/ray/normal_60/340/trim-normal_60_340.txt -s 340 -l 2000
	# ./bin/trim -d -i ../trace/normal/normal_60.csv -o report/ray/normal_60/385/trim-normal_60_385.txt -s 385 -l 2000
	
	# ./bin/trim -d -i ../trace/normal/normal_65.csv -o report/ray/normal_65/445/trim-normal_65_445.txt -s 445 -l 2000
	# ./bin/trim -d -i ../trace/normal/normal_65.csv -o report/ray/normal_65/385/trim-normal_65_385.txt -s 385 -l 2000
	
	# ./bin/trim -d -i ../trace/normal/normal_70.csv -o report/ray/normal_70/450/trim-normal_70_450.txt -s 450 -l 2000
	
	# ./bin/trim -d -i ../trace/normal/normal_75.csv -o report/ray/normal_75/570/trim-normal_75_570.txt -s 570 -l 2000
	# ./bin/trim -d -i ../trace/normal/normal_75.csv -o report/ray/normal_75/500/trim-normal_75_500.txt -s 500 -l 2000
	
	# ./bin/trim -d -i ../trace/normal/normal_80.csv -o report/ray/normal_80/550/trim-normal_80_550.txt -s 550 -l 2000
	# ./bin/trim -d -i ../trace/normal/normal_80.csv -o report/ray/normal_80/470/trim-normal_80_470.txt -s 470 -l 2000
	
	# ./bin/trim -d -i ../trace/normal/normal_85.csv -o report/ray/normal_85/550/trim-normal_85_550.txt -s 550 -l 2000
	# ./bin/trim -d -i ../trace/normal/normal_85.csv -o report/ray/normal_85/635/trim-normal_85_635.txt -s 635 -l 2000
	
	# ./bin/trim -d -i ../trace/normal/normal_90.csv -o report/ray/normal_90/760/trim-normal_90_760.txt -s 760 -l 2000
	# ./bin/trim -d -i ../trace/normal/normal_90.csv -o report/ray/normal_90/670/trim-normal_90_670.txt -s 670 -l 2000

	# ./bin/trim -d -i ../trace/normal/normal_95.csv -o report/ray/normal_95/580/trim-normal_95_580.txt -s 580 -l 2000
	# ./bin/trim -d -i ../trace/normal/normal_95.csv -o report/ray/normal_95/630/trim-normal_95_630.txt -s 630 -l 2000

	# ./bin/trim -d -i ../trace/normal/normal_100.csv -o report/ray/normal_100/850/trim-normal_100_850.txt -s 850 -l 2000
	# ./bin/trim -d -i ../trace/normal/normal_100.csv -o report/ray/normal_100/785/trim-normal_100_785.txt -s 785 -l 2000

trimv2run: bin/trim_v2
	# ./bin/trim_v2 -d -i ../trace/normal/normal_60.csv -o report/ray/normal_60/340/trim_v2-normal_60_340.txt -s 340 -l 2000
	# ./bin/trim_v2 -d -i ../trace/normal/normal_60.csv -o report/ray/normal_60/385/trim_v2-normal_60_385.txt -s 385 -l 2000
	
	# ./bin/trim_v2 -d -i ../trace/normal/normal_65.csv -o report/ray/normal_65/445/trim_v2-normal_65_445.txt -s 445 -l 2000
	# ./bin/trim_v2 -d -i ../trace/normal/normal_65.csv -o report/ray/normal_65/385/trim_v2-normal_65_385.txt -s 385 -l 2000
	
	# ./bin/trim_v2 -d -i ../trace/normal/normal_70.csv -o report/ray/normal_70/450/trim_v2-normal_70_450.txt -s 450 -l 2000
	
	# ./bin/trim_v2 -d -i ../trace/normal/normal_75.csv -o report/ray/normal_75/570/trim_v2-normal_75_570.txt -s 570 -l 2000
	# ./bin/trim_v2 -d -i ../trace/normal/normal_75.csv -o report/ray/normal_75/500/trim_v2-normal_75_500.txt -s 500 -l 2000
	
	# ./bin/trim_v2 -d -i ../trace/normal/normal_80.csv -o report/ray/normal_80/550/trim_v2-normal_80_550.txt -s 550 -l 2000
	# ./bin/trim_v2 -d -i ../trace/normal/normal_80.csv -o report/ray/normal_80/470/trim_v2-normal_80_470.txt -s 470 -l 2000
	
	# ./bin/trim_v2 -d -i ../trace/normal/normal_85.csv -o report/ray/normal_85/550/trim_v2-normal_85_550.txt -s 550 -l 2000
	# ./bin/trim_v2 -d -i ../trace/normal/normal_85.csv -o report/ray/normal_85/635/trim_v2-normal_85_635.txt -s 635 -l 2000
	
	# ./bin/trim_v2 -d -i ../trace/normal/normal_90.csv -o report/ray/normal_90/760/trim_v2-normal_90_760.txt -s 760 -l 2000
	# ./bin/trim_v2 -d -i ../trace/normal/normal_90.csv -o report/ray/normal_90/670/trim_v2-normal_90_670.txt -s 670 -l 2000

	# ./bin/trim_v2 -d -i ../trace/normal/normal_95.csv -o report/ray/normal_95/580/trim_v2-normal_95_580.txt -s 580 -l 2000
	# ./bin/trim_v2 -d -i ../trace/normal/normal_95.csv -o report/ray/normal_95/630/trim_v2-normal_95_630.txt -s 630 -l 2000

	# ./bin/trim_v2 -d -i ../trace/normal/normal_100.csv -o report/ray/normal_100/850/trim_v2-normal_100_850.txt -s 850 -l 2000
	# ./bin/trim_v2 -d -i ../trace/normal/normal_100.csv -o report/ray/normal_100/785/trim_v2-normal_100_785.txt -s 785 -l 2000

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
