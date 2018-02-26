% Heartbeat Scheduling: Provable Efficiency for Nested Parallelism
% Umut Acar; Arthur Charguéraud; Adrien Guatto; Mike Rainey; Filip Sieczkowski
% Experimental study guide

Overview
========

Source code
===========

The source code we used to evaluate our Heartbeat Scheduler is
available as a [Github
repository](https://github.com/deepsea-inria/encore).

How to repeat the experimental evaluation
=========================================

We encourage other interested parties to repeat our empirical
study. The instructions in this document show how

- to obtain the required software dependencies,
- to obtain the input data needed to run the experiments,
- to configure the scripts that run the experiments, 
- to run the experiments, and
- to interpret the results of the experiments.

If you encounter difficulties while using this guide, please email
[Mike Rainey](mailto:me@mike-rainey.site).

Prerequisites
-------------

To have enough room to run the experiments, your filesystem should
have about 300GB of free hard-drive space and your machine at least
128GB or RAM. These space requirements are so large because some of
the input graphs we use are huge.

The following packages should be installed on your test machine.

-----------------------------------------------------------------------------------
Package    Version         Details
--------   ----------      --------------------------------------------------------
gcc         >= 6.1         Recent gcc is required because pdfs 
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
                                               
hwloc        recent        *Optional dependency* (See instructions 
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

Now that all the prerequisites are installed on the test machine, we
can proceed to download the source files we need to build the
benchmarks. First, download

- [the downloader script](download-sources.sh)

and let `$DOWNLOADS` denote the path to the place where the
`download-sources.sh` script is stored. Now, create a new directory,
say, `heartbeat-pldi18` in which to put the source files and, from
that directory, run the download script.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
$ cd heartbeat-pldi18
$ $DOWNLOADS/download-sources.sh
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Even though the required version of GCC comes packaged with full
support for Cilk Plus, there are some additional features that we
needed from Cilk that are not provided in GCC. In particular, our
experiments need a Cilk Plus runtime that emits statistics, such as
number of threads spawned and overall processor utilization. To that
end, we have implemented our own custom version of the Cilk Plus
runtime and provided a script to build our custom Cilk Plus
runtime.

So, get the build process started, from the same directory, download

- [the custom Cilk Plus runtime installer script](build-cilk-plus-rts.sh)

and then run the script, ensuring the build process completes
successfully.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
$ $DOWNLOADS/build-cilk-plus-rts.sh
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Getting the input data
----------------------

We use IPFS as the tool to disseminate our input data files. After
installing IPFS, we need to initialize the local IPFS configuration.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
$ ipfs init
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::::: {#warning-disk-space .warning}

**Warning:** disk space. The default behavior of IPFS is to keep a
cache of all downloaded files in the folder `~/.ipfs/`. Because the
graph data is several gigabytes, the cache folder should have at least
twice this much free space. To select a different cache folder for
IPFS, before issuing the command ipfs init, set the environment
variable `$IPFS_PATH` to point to the desired path.

:::::

In order to use IPFS to download files, the IPFS daemon needs to be
running. You can start the IPFS daemon in the following way, or you
can start it in the background, like a system service. Make sure that
this daemon continues to run in the background until after all of the
input data files you want are downloaded on your test machine.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
$ ipfs daemon
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Our benchmarking script is configured to automatically download the
input data as needed. We can get started by changing to the
benchmarking directory and building the script.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
$ cd encore/bench
$ make bench.pbench
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The script supports running one benchmark at a time. Let's start by
running the convexhull benchmark. Let `$P` denote the number of
processors/cores that you wish to use in the experiments. This number
should be at least two and should be no more than the number of cores
in the system.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
$ bench.pbench compare -benchmark convexhull -proc 1,$P
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::::: {#note-hyperthreading .note}

*Note:* If your machine has hyperthreading enabled, then we recommend
running the experiments without and with hyperthreading. To run with
hyperthreading, just set `$P` to be the total number of cores or
hyperthreads in the system as desired. For example, if the machine has
eight cores, with each core having two hyperthreads, then to test
without hyperthreading, set `$P` to be `8`, and to test with
hyperthreading, set `$P$` to be `16`.

:::::

For a variety of reasons, one of the steps involved in the
benchmarking can fail. A likely cause is the failure to obtain the
required input data. The reason is that these files are large, and as
such, we are hosting the files ourselves, using a peer-to-peer
file-transfer protocol called [IPFS](http://ipfs.io). 

::::: {#note-ipfs-ping .note}

*Note:* If you notice that the benchmarking script gets stuck for a
long time while issuing the `ipfs get ...` commands, we recommend
that, in a separate window, you ping one of the machines that we are
using to host our input data.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
$ ipfs ping QmRBzXmjGFtDAy57Rgve5NbNDvSUJYeSjoGQkdtfBvnbWX
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Please email us if you have to wait for a long time or are having
trouble getting the input data.

:::::

How to run the experiments
--------------------------

After this step completes successfully, there should appear in the
`bench` folder a number of new text files of the form `results_*.txt`
and a PDF named `tables_compare.pdf`. The results in the table are,
however, premature at the moment, because there are too few samples to
make any conclusions.

It is possible to collect additional samples by running the following
command.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
$ bench.pbench compare -benchmark convexhull -proc $P -runs 29 -mode append
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::::: {#note-samples .note}

*Note:* In our example, we collect additional samples for runs
involving two or more processors. The reason is that the single-core
runs usually exhibit relatively little noise and, as such, we prefer
to save time running experiments by performing fewer single-core runs.

:::::


How to interpret the results
----------------------------

Team
====

