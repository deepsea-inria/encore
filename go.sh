#!/bin/bash

do_appendix_benchmarks=$1

# prepare a folder in which to store results
mkdir Results
results_dir=`pwd`/Results
nb_runs=30

# get source files

git clone https://github.com/deepsea-inria/cmdline.git
git clone https://github.com/deepsea-inria/chunkedseq.git
git clone https://github.com/deepsea-inria/pbench.git
git clone https://github.com/deepsea-inria/encore.git -b concurrent-counters

# build and run experiments
(
cd encore/example/

make bench.pbench
./bench.pbench proc -runs $nb_runs
./bench.pbench size -runs $nb_runs
./bench.pbench indegree2 -runs $nb_runs

if [ "$do_appendix_benchmarks" == "appendix" ]
then
    ./bench.pbench threshold -runs $nb_runs
    ./bench.pbench workload_fanin -runs $nb_runs
    ./bench.pbench workload_fanin_speedup -runs $nb_runs
fi

# leave results in the Results folder
cp plots_*.pdf $results_dir
cp results_*.txt $results_dir
)
