all: basic

PBENCH_PATH=../../pbench

include $(PBENCH_PATH)/Makefile_common

TCMALLOC_HOME=-L $(HOME)/Installs/gperftools/lib/

CHUNKEDSEQ_HOME=$(HOME)/chunkedseq/include
ENCORE_INCLUDE_HOME=$(HOME)/concurrent-counters/include
CMDLINE_HOME=$(HOME)/cmdline/include
PBBS_INCLUDE_HOME=$(HOME)/pbbs-include
PCTL_INCLUDE_HOME=$(HOME)/pctl/include
PBBS_PCTL_INCLUDE_HOME=$(HOME)/pbbs-pctl/include
PBBS_PCTL_TEST_INCLUDE_HOME=$(HOME)/pbbs-pctl/test/include
QUICKCHECK_INCLUDE_HOME=$(HOME)/quickcheck/quickcheck
#HWLOC_HOME=$(HOME)/hwloc/

HWLOC_FLAGS=$(pkg-config --cflags hwloc)
HWLOC_LIBS=$(pkg-config --libs hwloc)
HWLOC_DIRECTIVES=-DHAVE_HWLOC $(HWLOC_FLAGS) -lhwloc # $(HWLOC_LIBS)

INCLUDE_FILES=$(wildcard $(CHUNKEDSEQ_HOME)/*.hpp) $(wildcard $(ENCORE_INCLUDE_HOME)/*.hpp) $(wildcard $(CMDLINE_HOME)/*.hpp) $(wildcard $(PBBS_INCLUDE_HOME)/*.hpp) $(wildcard $(PCTL_INCLUDE_HOME)/*.hpp) $(wildcard $(PBBS_PCTL_INLCUDE_HOME)/*.hpp) $(wildcard $(PBBS_PCTL_TEST_INCLUDE_HOME)/*.hpp) $(wildcard $(QUICKCHECK_INCLUDE_HOME)/*.hpp)

INCLUDE_DIRECTIVES= -I $(CHUNKEDSEQ_HOME) -I $(ENCORE_INCLUDE_HOME) -I $(CMDLINE_HOME) -I $(PBBS_INCLUDE_HOME) -I $(PCTL_INCLUDE_HOME) -I $(PBBS_PCTL_INCLUDE_HOME) -I $(PBBS_PCTL_TEST_INCLUDE_HOME) -I $(QUICKCHECK_INCLUDE_HOME)

TCMALLOC= $(TCMALLOC_HOME) -ltcmalloc

CILK_PLUS= -DUSE_CILK_PLUS -DUSE_CILK_PLUS_RUNTIME -fcilkplus -lcilkrts

COMMON_PREFIX= -fpermissive -pthread -O2 -DNDEBUG -std=gnu++11 $(HWLOC_DIRECTIVES)
DEBUG_PREFIX= -fpermissive -pthread -O0 -std=gnu++11 -g

%.dyn: %.cpp $(INCLUDE_FILES)
	g++ -DENCORE_ENABLE_STATS $(COMMON_PREFIX) $(TCMALLOC) $(INCLUDE_DIRECTIVES) -o $@ $<

%.sta: %.cpp $(INCLUDE_FILES)
	g++ -DENCORE_ENABLE_STATS $(COMMON_PREFIX) $(TCMALLOC) $(INCLUDE_DIRECTIVES) -o $@ $< # -DFIXED_SIZE_INCOUNTER

%.sim: %.cpp $(INCLUDE_FILES)
	g++ -DENCORE_ENABLE_STATS $(COMMON_PREFIX) $(TCMALLOC) $(INCLUDE_DIRECTIVES) -DENCORE_INCOUNTER_SIMPLE -o $@ $<

%.dbg: %.cpp $(INCLUDE_FILES)
	g++ -DENCORE_ENABLE_STATS $(DEBUG_PREFIX) $(INCLUDE_DIRECTIVES) -o $@ $<

clean:
	rm -f *.dbg *.sta *.sim *.dyn
