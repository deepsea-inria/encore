#!/bin/sh

DEST_PATH=`pwd`/cilk-plus-rts

(
    cd cilk-plus-rts-with-stats
    rm -rf $DEST_PATH
    mkdir $DEST_PATH
    libtoolize
    aclocal
    automake --add-missing
    autoconf
    ./configure --prefix=$DEST_PATH
    make -j
    make install
)
