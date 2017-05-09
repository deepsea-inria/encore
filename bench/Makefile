all: basic

PBENCH_PATH=../../pbench

include $(PBENCH_PATH)/Makefile_common

TCMALLOC_HOME=-L $(HOME)/Installs/gperftools/lib/

PACKAGE_PATH=../../

CACTUS_HOME=$(PACKAGE_PATH)/cactus-stack/include
CHUNKEDSEQ_HOME=$(PACKAGE_PATH)/chunkedseq/include
ENCORE_INCLUDE_HOME=$(PACKAGE_PATH)/encore/include
CMDLINE_HOME=$(PACKAGE_PATH)/cmdline/include
PBBS_INCLUDE_HOME=$(PACKAGE_PATH)/pbbs-include
PCTL_INCLUDE_HOME=$(PACKAGE_PATH)/pctl/include
PBBS_PCTL_INCLUDE_HOME=$(PACKAGE_PATH)/pbbs-pctl/include
PBBS_PCTL_TEST_INCLUDE_HOME=$(PACKAGE_PATH)/pbbs-pctl/test/include
QUICKCHECK_INCLUDE_HOME=$(PACKAGE_PATH)/quickcheck/quickcheck

HWLOC_FLAGS=`pkg-config --cflags hwloc`
HWLOC_LIBS=`pkg-config --libs hwloc`
HWLOC_DIRECTIVES=-DHAVE_HWLOC $(HWLOC_FLAGS) $(HWLOC_LIBS)

INCLUDE_FILES=$(wildcard $(CHUNKEDSEQ_HOME)/*.hpp) $(wildcard $(ENCORE_INCLUDE_HOME)/*.hpp) $(wildcard $(CMDLINE_HOME)/*.hpp) $(wildcard $(PBBS_INCLUDE_HOME)/*.hpp) $(wildcard $(PCTL_INCLUDE_HOME)/*.hpp) $(wildcard $(PBBS_PCTL_INLCUDE_HOME)/*.hpp) $(wildcard $(PBBS_PCTL_TEST_INCLUDE_HOME)/*.hpp) $(wildcard $(QUICKCHECK_INCLUDE_HOME)/*.hpp) $(wildcard $(CACTUS_HOME)/*.hpp) $(wildcard *.hpp)

INCLUDE_DIRECTIVES= -I $(CHUNKEDSEQ_HOME) -I $(ENCORE_INCLUDE_HOME) -I $(CMDLINE_HOME) -I $(PBBS_INCLUDE_HOME) -I $(PCTL_INCLUDE_HOME) -I $(PBBS_PCTL_INCLUDE_HOME) -I $(PBBS_PCTL_TEST_INCLUDE_HOME) -I $(QUICKCHECK_INCLUDE_HOME) -I $(CACTUS_HOME)

TCMALLOC= $(TCMALLOC_HOME) -ltcmalloc

CILK_PLUS= -DUSE_CILK_PLUS -DUSE_CILK_PLUS_RUNTIME -fcilkplus -lcilkrts

COMMON_PREFIX= -fpermissive -pthread -O2 -DNDEBUG -std=gnu++11 $(HWLOC_DIRECTIVES)
DEBUG_PREFIX= -fpermissive -pthread -O0 -std=gnu++11 -g -DENCORE_USE_SIMPLE_SYNCHRONIZATION

sequence.encore: $(INCLUDE_FILES)
	g++ -DENCORE_ENABLE_STATS $(COMMON_PREFIX) $(TCMALLOC) $(INCLUDE_DIRECTIVES) -o sequence.encore sequence.cpp

sequence.cilk: $(INCLUDE_FILES)
	g++ $(CILK_PLUS) $(COMMON_PREFIX) $(INCLUDE_DIRECTIVES) -o sequence.cilk sequence.cpp

%.dbg: %.cpp $(INCLUDE_FILES)
	g++ -DENCORE_ENABLE_STATS $(DEBUG_PREFIX) $(INCLUDE_DIRECTIVES) -o $@ $<

clean: pbench_clean
	rm -f *.encore *.cilk *.dbg