# encore

A C++ library for writing fine-grain parallel programs



How to install
==============

Check out the following repository in the same folder

   mkdir ~/exp
   cd exp
   git clone https://github.com/deepsea-inria/encore.git
   git clone https://github.com/deepsea-inria/pbbs-include.git
   git clone https://github.com/deepsea-inria/pbbs-pctl.git
   git clone https://github.com/deepsea-inria/cactus-stack.git
   git clone https://github.com/deepsea-inria/chunkedseq.git
   git clone https://github.com/deepsea-inria/cmdline.git
   git clone https://github.com/deepsea-inria/pbench.git
   git clone https://github.com/deepsea-inria/pctl.git

For running the benchmarks, you need "~/exp/encore/bench/_data" 
to be alias to a folder storing the input data files. 

GCC >= 6.1 is required. If you have a local installation, you might
need the following lines in your ~/.bashrc:

   PATH=/home/rainey/Installs/GCC-6.1.0/bin:$PATH
   export GCC_HOME="/home/rainey/Installs/GCC-6.1.0"
   export CPATH=$GCC_HOME/include:$CPATH
   export LIBRARY_PATH=$GCC_HOME/lib:$GCC_HOME/lib64
   export LD_LIBRARY_PATH=$GCC_HOME/lib:$GCC_HOME/lib64:$LD_LIBRARY_PATH
   # next line is to avoid printf reports for big allocs
   export TCMALLOC_LARGE_ALLOC_REPORT_THRESHOLD=100000000000

Currently, the code requires for compiling quickcheck source code, e.g.:

   ln -s /home/rainey/quickcheck quickcheck

Try it out:

   cd ~/exp/encore/example
   make fib.dbg
   # (warnings to be ignored)
   ./fib.dbg -algorithm dc -n 20

Example output:

   exectime 0.367
   nb_promotions 5978
   nb_steals 0
   nb_stacklet_allocations 0
   nb_stacklet_deallocations 0
   launch_duration 0.367092
   utilization 1
   exectime 0.367

Another example

   ./fib.dbg -algorithm dc -n 20 -proc 40

Example output:

   exectime 0.076
   nb_promotions 6144
   nb_steals 1220
   nb_stacklet_allocations 0
   nb_stacklet_deallocations 0
   launch_duration 0.0771156
   utilization 0.8211
   exectime 0.078


Now real example
     
   cd ~/exp/encore/bench
   make merge.log
   ./merge.log -algorithm encore -n 10000000 -proc 40

Example output:

   exectime 0.020
   nb_promotions 1590
   nb_steals 335
   nb_stacklet_allocations 0
   nb_stacklet_deallocations 0
   launch_duration 0.0215029
   utilization 0.702188


Another example

   convexhull.cilk -algorithm pbbs -proc 40 -type 2d -infile _data/array_point2d_in_circle_large.bin 
   
   convexhull.encore -algorithm encore -proc 40 -type 2d -infile _data/array_point2d_in_circle_large.bin 



For viewing the execution:

   cd ~/exp
   git clone https://github.com/deepsea-inria/pasl.git
   cd ~/exp/pasl/tools/pview
   make
   alias pview=~/exp/pasl/tools/pview/pview

Then:
   
   cd ~/exp/encore/bench 
   ./merge.log -algorithm encore -n 10000000 -proc 40 --pview
   pview


For efficient programs, we use TCMALLOC. 
If you have a system installation, then it might work out of the box.

   sudo apt-get install gperftools

Else, if you have a local installation, you need to link it, e.g.

   echo "TCMALLOC_HOME=-L /home/rainey/Installs/gperftools/lib/" > ~/exp/encore/bench/settings.sh

Then:

   cd ~/exp/encore/bench 
   make merge.encore
   ./merge.encore -algorithm encore -n 10000000 -proc 40 

To build the Cilk version instead:

   make merge.cilk
   ./merge.cilk -algorithm pbbs -n 10000000 -proc 40 


Now, for running all experiments

   cd ~/exp/encore/bench 
   make bench.pbench

   # add this because the prefix "./" is not always used (currently)
   export PATH=.:$PATH

   # to simulate what the bench would do:
   ./bench.pbench compare -benchmark convexhull -only run --virtual_run

   # then to execute it:
   ./bench.pbench compare -benchmark convexhull



For further comparison using the instrumented version of cilk

   cd ~/exp
   git clone https://github.com/deepsea-inria/cilk-plus-rts-with-stats.git
   cd ~/exp/cilk-plus-rts-with-stats
   ./build.sh
   # you might need to edit this script to replace the relative path "../cilk-plus-rts/"

   # now, we need to overwrite default cilk with our own patched version, e.g.:
   cd ~/exp/encore/bench
   export LD_LIBRARY_PATH=../../cilk-plus-rts/lib:$LD_LIBRARY_PATH
   echo "CUSTOM_CILKRTS_SETTINGS=-L ../../cilk-plus-rts/lib -I ../..//cilk-plus-rts/include -DCUSTOM_CILK_PLUS_RUNTIME" >> settings.sh

   # testing (need to clear first, because "make" won't automatically detect changes
   rm -f merge.cilk
   make merge.cilk
   ./merge.cilk -algorithm pbbs -n 10000000 -proc 40  &> output.txt
   more output.txt



Compiler flags
==============

The following flag disables the scalable incounter/outset data
structures and uses instead the single-cell counter and Treiber
stack. These latter two data structures may be useful for debugging
purposes because they are simpler and may be faster in certain cases,
in particular when in/out degree are low.

`ENCORE_USE_SIMPLE_SYNCHRONIZATION`