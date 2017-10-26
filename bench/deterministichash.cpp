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

#include "encorebench.hpp"
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
  std::string lib_type = deepsea::cmdline::parse_or_default_string("algorithm", "encore");
  if (lib_type == "pbbs") {
    encorebench::run_and_report_elapsed_time([&] {
        pbbs::_seq<Item> res = pbbs::removeDuplicates(pbbs::_seq<Item>(&x[0], x.size()));
    });
  } else {
    auto ptr = x.begin();
    auto n = x.size();
    encore::launch_interpreter_via_lambda([=] (encore::edsl::pcfg::stack_type st) {
      auto ty = encore::edsl::pcfg::cactus::Parent_link_sync;
      auto s = _seq<Item>(ptr, n);
      _seq<Item> res;
      return encorebench::removeDuplicates(st, ty, s, &res);
    });
  }
}

int main(int argc, char** argv) {
  encorebench::initialize(argc, argv);
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
