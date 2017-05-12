#include <iostream>
#include <chrono>
#include <array>
#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>
#include <algorithm>

#include "encore.hpp"
#include "blockradixsort.hpp"
#include "loaders.hpp"
#include "blockRadixSort.h"

namespace pbbs {

template <class Item>
using parray = pasl::pctl::parray<Item>;

void benchmark(std::string infile) {
  parray<int> x = pasl::pctl::io::load<parray<int>>(infile);
  bool should_check = deepsea::cmdline::parse_or_default_bool("check", false);
  parray<int> ref;
  if (should_check) {
    ref = x;
  }
  deepsea::cmdline::dispatcher d;
  d.add("encore", [&] {
    encore::launch_interpreter<encorebench::integerSort<intT>>(x.begin(), (int)x.size());
  });
  d.add("pbbs", [&] {
    encore::run_and_report_elapsed_time([&] {
      integerSort<int>(&x[0], (int)x.size());
    });
  });
  d.dispatch("algorithm");
  if (should_check) {
    std::sort(ref.begin(), ref.end());
    auto it_ref = ref.begin();
    for (auto it = x.begin(); it != x.end(); it++) {
      if (*it != *it_ref) {
        std::cerr << "bogus result" << std::endl;
        exit(0);
      }
      it_ref++;
    }
  }
}

} // end namespace

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  std::string infile = deepsea::cmdline::parse_or_default_string("infile", "");
  if (infile == "") {
    std::cout << "bogus input file name" << std::endl;
    exit(0);
  }
  pbbs::benchmark(infile);
  return 0;
}
