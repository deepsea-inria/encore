all: basic

PBENCH_PATH=../../../pbench

include $(PBENCH_PATH)/Makefile_common

TCMALLOC_HOME=-L $(HOME)/Installs/gperftools/lib/

CHUNKEDSEQ_HOME=$(HOME)/chunkedseq/include
ENCORE_INCLUDE_HOME=$(HOME)/encore/include
CMDLINE_HOME=$(HOME)/cmdline/include
PBBS_INCLUDE_HOME=$(HOME)/pbbs-include
PCTL_INCLUDE_HOME=$(HOME)/pctl/include
PBBS_PCTL_INCLUDE_HOME=$(HOME)/pbbs-pctl/include
PBBS_PCTL_TEST_INCLUDE_HOME=$(HOME)/pbbs-pctl/test/include
QUICKCHECK_INCLUDE_HOME=$(HOME)/quickcheck/quickcheck
HWLOC_HOME=$(HOME)/hwloc/

HWLOC_FLAGS=$(shell pkg-config --define-variable=HWLOC_INSTALL=$(HWLOC_HOME) --cflags $(HWLOC_HOME)/lib/pkgconfig/hwloc.pc)
HWLOC_LIBS=$(shell pkg-config --define-variable=HWLOC_INSTALL=$(HWLOC_HOME) --libs $(HWLOC_HOME)/lib/pkgconfig/hwloc.pc)
HWLOC_DIRECTIVES=-DHAVE_HWLOC $(HWLOC_FLAGS) $(HWLOC_LIBS)

INCLUDE_FILES=$(wildcard $(CHUNKEDSEQ_HOME)/*.hpp) $(wildcard $(ENCORE_INCLUDE_HOME)/*.hpp) $(wildcard $(CMDLINE_HOME)/*.hpp) $(wildcard $(PBBS_INCLUDE_HOME)/*.hpp) $(wildcard $(PCTL_INCLUDE_HOME)/*.hpp) $(wildcard $(PBBS_PCTL_INLCUDE_HOME)/*.hpp) $(wildcard $(PBBS_PCTL_TEST_INCLUDE_HOME)/*.hpp) $(wildcard $(QUICKCHECK_INCLUDE_HOME)/*.hpp)

INCLUDE_DIRECTIVES= -I $(CHUNKEDSEQ_HOME) -I $(ENCORE_INCLUDE_HOME) -I $(CMDLINE_HOME) -I $(PBBS_INCLUDE_HOME) -I $(PCTL_INCLUDE_HOME) -I $(PBBS_PCTL_INCLUDE_HOME) -I $(PBBS_PCTL_TEST_INCLUDE_HOME) -I $(QUICKCHECK_INCLUDE_HOME)

TCMALLOC= $(TCMALLOC_HOME) -ltcmalloc

CILK_PLUS= -DUSE_CILK_PLUS -DUSE_CILK_PLUS_RUNTIME -fcilkplus -lcilkrts

ALLFL=-fauto-inc-dec -fbranch-count-reg -fcombine-stack-adjustments -fcompare-elim -fcprop-registers -fdce -fdefer-pop -fdse -fforward-propagate -fguess-branch-probability -fif-conversion2 -fif-conversion -finline-functions-called-once -fipa-pure-const -fipa-profile -fipa-reference -fmerge-constants -fmove-loop-invariants -freorder-blocks -fshrink-wrap -fsplit-wide-types -fssa-backprop -fssa-phiopt -ftree-bit-ccp -ftree-ccp -ftree-ch -ftree-coalesce-vars -ftree-copy-prop -ftree-dce -ftree-dominator-opts -ftree-dse -ftree-forwprop -ftree-fre -ftree-phiprop -ftree-sink -ftree-slsr -ftree-sra -ftree-pta -ftree-ter -funit-at-a-time

ALLSL=-fthread-jumps -falign-functions  -falign-jumps -falign-loops  -falign-labels -fcaller-saves -fcrossjumping -fcse-follow-jumps  -fcse-skip-blocks -fdelete-null-pointer-checks -fdevirtualize -fdevirtualize-speculatively -fexpensive-optimizations -fgcse  -fgcse-lm  -fhoist-adjacent-loads -finline-small-functions -findirect-inlining -fipa-cp -fipa-cp-alignment -fipa-sra -fipa-icf -fisolate-erroneous-paths-dereference -flra-remat -foptimize-sibling-calls -foptimize-strlen -fpartial-inlining -fpeephole2 -freorder-blocks-algorithm=stc -freorder-blocks-and-partition -freorder-functions -frerun-cse-after-loop  -fsched-interblock  -fsched-spec -fschedule-insns  -fschedule-insns2 -fstrict-aliasing -fstrict-overflow -ftree-builtin-call-dce -ftree-switch-conversion -ftree-tail-merge  -ftree-pre -ftree-vrp -fipa-ra

COMMON_PREFIX= -fpermissive -pthread -std=gnu++11  $(ALLFL) $(ALLSL) -DNDEBUG # $(HWLOC_DIRECTIVES)
DEBUG_PREFIX= -fpermissive -pthread -O0 -std=gnu++11 -g -Winline

%.dbg: %.cpp $(INCLUDE_FILES)
	g++ -DENCORE_ENABLE_STATS $(DEBUG_PREFIX) $(INCLUDE_DIRECTIVES) -o $@ $<

%.opt: %.cpp $(INCLUDE_FILES)
	g++ -DENCORE_ENABLE_STATS $(COMMON_PREFIX) $(INCLUDE_DIRECTIVES) -fno-omit-frame-pointer -o $@ $<

clean: pbench_clean
	rm -f *.dbg *.opt
