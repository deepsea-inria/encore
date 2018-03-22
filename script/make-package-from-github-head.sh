#!/bin/bash

git clone https://github.com/deepsea-inria/cilk-plus-rts-with-stats.git

git clone https://github.com/deepsea-inria/cactus-stack.git

git clone https://github.com/deepsea-inria/cmdline.git

git clone https://github.com/deepsea-inria/pbench.git

git clone https://github.com/deepsea-inria/chunkedseq.git

git clone https://github.com/deepsea-inria/sptl.git

git clone https://github.com/deepsea-inria/pbbs-include.git

git clone https://github.com/deepsea-inria/pbbs-sptl.git

git clone https://github.com/deepsea-inria/encore.git

OUT_FOLDER=benchmark-encore
rm -rf $OUT_FOLDER
mkdir $OUT_FOLDER

for fname in $( find . -name *.nix ); do
    file=$(basename $fname)
    dir=$(dirname $fname)
    target="$OUT_FOLDER/$dir"
    mkdir -p $target
    cp $fname $target
done

tar -czvf benchmark-encore.tar.gz benchmark-encore
