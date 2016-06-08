all: basic

PBENCH_PATH=../..

include $(PBENCH_PATH)/Makefile_common

bench.pbench:
	ocamlc $(XLIB_INCLUDES) $(XLIB_LIBS) $(XLIB_SRC) nums.cma bench.ml -o $@
