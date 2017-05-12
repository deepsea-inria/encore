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

namespace pbbs {

template <class Item>
using parray = pasl::pctl::parray<Item>;

void benchmark(std::string infile) {
  parray<int> x = pasl::pctl::io::load<parray<int>>(infile);
  deepsea::cmdline::dispatcher d;
  d.add("encore", [&] {
    encore::launch_interpreter<integerSort<intT>>(x.begin(), (int)x.size());
  });
  d.add("pbbs", [&] {
    encore::run_and_report_elapsed_time([&] {
      integerSort<int>(x.begin(), (int)x.size());
    });
  });
  d.dispatch("algorithm"); 
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
