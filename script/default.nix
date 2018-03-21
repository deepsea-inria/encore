{ pkgs   ? import <nixpkgs> {},
  stdenv ? pkgs.stdenv,
  fetchurl,
  cactus-stack,
  pbench,
  sptl,
  pbbs-sptl,
  pbbs-include,
  cmdline,
  chunkedseq,
  cilk-plus-rts-with-stats,
  gperftools,
  useHwloc ? false,
  hwloc,
  buildDocs ? false
}:

stdenv.mkDerivation rec {
  name = "encore-${version}";
  version = "v0.1-alpha";

  src = fetchurl {
    url = "https://github.com/deepsea-inria/encore/archive/${version}.tar.gz";
    sha256 = "11bpq69r2fa5wjvza4yz65bg4hijgv0bzzn7dksc6x31jmjcyv75";
  };

  buildInputs =
    let docs =
      if buildDocs then [
        pkgs.pandoc
        pkgs.texlive.combined.scheme-full
      ] else
        [];
    in
    [ pbench sptl pbbs-include cmdline chunkedseq ] ++ docs;
        
  buildPhase =
    let hwlocConfig =
      if useHwloc then ''
        USE_HWLOC=1
        USE_MANUAL_HWLOC_PATH=1
        MY_HWLOC_FLAGS=-I ${hwloc.dev}/include/
        MY_HWLOC_LIBS=-L ${hwloc.lib}/lib/ -lhwloc
      '' else "";
    in
    ''
    cat >> settings.sh <<__EOT__
    CACTUS_PATH=${cactus-stack}/include/
    PBENCH_PATH=../pbench/
    CMDLINE_PATH=${cmdline}/include/
    CHUNKEDSEQ_PATH=${chunkedseq}/include/
    SPTL_PATH=${sptl}/include/
    PBBS_INCLUDE_PATH=${pbbs-include}/include/
    PBBS_SPTL_PATH=${pbbs-sptl}/include/
    PBBS_SPTL_BENCH_PATH=${pbbs-sptl}/bench/
    ENCORE_INCLUDE_PATH=$out/include/
    USE_32_BIT_WORD_SIZE=1
    USE_CILK=1
    CUSTOM_MALLOC_PREFIX=-ltcmalloc -L${gperftools}/lib
    CILK_EXTRAS_PREFIX=-L ${cilk-plus-rts-with-stats}/lib -I  ${cilk-plus-rts-with-stats}/include -ldl -DCILK_RUNTIME_WITH_STATS
    __EOT__
    cat >> settings.sh <<__EOT__
    ${hwlocConfig}
    __EOT__
    '';

  installPhase = ''
    mkdir -p $out/bench/
    cp settings.sh bench/Makefile bench/bench.ml bench/*.cpp bench/*.hpp $out/bench/
    mkdir -p $out/include/
    cp include/*.hpp $out/include/
    '';

  meta = {
    description = "The C++ library for benchmarking the Heartbeat Scheduling algorithm for fine-grain, irregular parallelism.";
    license = "MIT";
    homepage = http://deepsea.inria.fr/heartbeat/;
  };
}