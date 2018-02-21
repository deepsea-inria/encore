% Heartbeat Scheduling: Provable Efficiency for Nested Parallelism
% Umut Acar; Arthur Charguéraud; Adrien Guatto; Mike Rainey; Filip Sieczkowski
% Experimental study guide


Run our experimental evaluation
===============================


1. Prerequisites
----------------

To have enough room to run the experiments, your filesystem should
have about 300GB of free hard-drive space and your machine at least
128GB or RAM. These space requirements are so large because some of
the input graphs we use are huge.

The following packages should be installed on your test machine.

-----------------------------------------------------------------------------------
Package    Version        Details
--------   ----------     ---------------------------------------------------------
gcc         >= 6.1        Recent gcc is required because pdfs 
                          makes heavy use of features of C++1x,
                          such as lambda expressions and
                          higher-order templates.
                          ([Home page](https://gcc.gnu.org/))

ocaml        >= 4.02       Ocaml is required to build the
                           benchmarking script.
                           ([Home page](http://www.ocaml.org/))

R            >= 2.4.1      The R tools is used by our scripts to
                           generate plots.
                           ([Home page](http://www.r-project.org/))
                                               
hwloc        recent        ***Optional dependency*** (See instructions 
                           below). This package is used by pdfs to force
                           interleaved NUMA allocation; as
                           such this package is optional and only
                           really relevant for NUMA machines.
                           ([Home page](http://www.open-mpi.org/projects/hwloc/))

ipfs         recent        We are going to use this software to
                           download data sets for our experiments.
                           ([Home page](https://ipfs.io/))
-----------------------------------------------------------------------------------

Table: Software dependencies for the benchmarks.
