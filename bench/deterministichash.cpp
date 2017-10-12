/*!
 * \file deterministichash_bench.cpp
 * \brief Benchmarking script for parallel hash table
 * \date 2016
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include <math.h>
#include <functional>
#include <stdlib.h>
#include "bench.hpp"
#include "deterministichash.hpp"
#include "loaders.hpp"
#include "deterministicHash.h"
#undef parallel_for
/***********************************************************************/

/*---------------------------------------------------------------------*/

template <class Item>
using parray = pasl::pctl::parray<Item>;

template <class Item>
void encore_bench(parray<Item>& x) {
  std::string lib_type = deepsea::cmdline::parse_or_default_string("lib_type", "pctl");
  if (lib_type == "pbbs") {
    encore::run_and_report_elapsed_time([&] {
      pbbs::removeDuplicates(pbbs::_seq<Item>(&x[0], x.size()));
    });
  } else {
    encore::call_and_
    encore::launch_interpreter<encorebench::Enter_removeDuplicates<Item>(encorebench::sequence::_seq(&x[0], x.size()));
  }
}

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
    std::string infile = deepsea::cmdline::parse_or_default_string("infile", "");
    if (infile != "") {
      deepsea::cmdline::dispatcher d;
      d.add("array_int", [&] {
        parray<int> x = pasl::pctl::io::load<parray<int>>(infile);
        encore_bench(x);
      });
      d.add("array_string", [&] {
        parray<char*> x = pasl::pctl::io::load<parray<char*>>(infile);
        encore_bench(x);
      });
      d.add("array_pair_string_int", [&] {
        parray<std::pair<char*, int>*> x = pasl::pctl::io::load<parray<std::pair<char*, int>*>>(infile);
        encore_bench(x);
      });
      d.dispatch("type");
    }
  return 0;
}

/***********************************************************************/
