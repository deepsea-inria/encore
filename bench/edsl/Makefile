all: basic

PBENCH_PATH=../../../pbench

include $(PBENCH_PATH)/Makefile_common

TCMALLOC_HOME=-L $(HOME)/Installs/gperftools/lib/

CHUNKEDSEQ_HOME=$(HOME)/chunkedseq/include
ENCORE_HOME=$(HOME)/encore/include
CMDLINE_HOME=$(HOME)/cmdline/include
PBBS_INCLUDE_HOME=$(HOME)/pbbs-include

INCLUDES= -I $(CHUNKEDSEQ_HOME) -I $(ENCORE_HOME) -I $(CMDLINE_HOME) -I $(PBBS_INCLUDE_HOME)

TCMALLOC= $(TCMALLOC_HOME) -ltcmalloc

CILK_PLUS= -DUSE_CILK_PLUS -DUSE_CILK_PLUS_RUNTIME -fcilkplus -lcilkrts

COMMON_PREFIX= -fpermissive -pthread -O2 -DNDEBUG -std=gnu++11

sequence.encore:
	g++ -DENCORE_ENABLE_STATS $(COMMON_PREFIX) $(TCMALLOC) $(INCLUDES) -o sequence.encore sequence.cpp

sequence.cilk:
	g++ $(CILK_PLUS) $(COMMON_PREFIX) $(INCLUDES) -o sequence.cilk sequence.cpp

clean: pbench_clean
	rm -f sequence.encore sequence.cilk
