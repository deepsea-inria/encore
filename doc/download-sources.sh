#!/bin/sh

git clone https://github.com/deepsea-inria/encore.git
git clone https://github.com/deepsea-inria/pbbs-include.git
git clone https://github.com/deepsea-inria/pbbs-pctl.git
git clone https://github.com/deepsea-inria/cactus-stack.git
git clone https://github.com/deepsea-inria/chunkedseq.git
git clone https://github.com/deepsea-inria/cmdline.git
git clone https://github.com/deepsea-inria/pbench.git
git clone https://github.com/deepsea-inria/pctl.git
git clone https://github.com/deepsea-inria/cilk-plus-rts-with-stats.git

# Seed the settings.sh configuration file so that encore uses our
# custom stats-enabled Cilk Plus runtime.
(
cat <<'EOF'
CUSTOM_CILKRTS_SETTINGS=-L ../../cilk-plus-rts/lib -I ../../cilk-plus-rts/include -DCUSTOM_CILK_PLUS_RUNTIME
EOF
) > encore/bench/settings.sh
