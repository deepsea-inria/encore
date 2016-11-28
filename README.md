# encore
A C++ library for writing fine-grain parallel programs

Compiler flags
==============

The following flag disables the scalable incounter/outset data
structures and uses instead the single-cell counter and Treiber
stack. These latter two data structures may be useful for debugging
purposes because they are simpler and may be faster in certain cases,
in particular when in/out degree are low.

`ENCORE_USE_SIMPLE_SYNCHRONIZATION`