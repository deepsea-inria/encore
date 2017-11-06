// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <iostream>
#include <chrono>
#include <array>
#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>
#include <algorithm>

#include "encorebench.hpp"
#include "speculative_for.hpp"
#include "graph.h"

namespace encorebench {

struct matchStep {
  pbbs::graph::edge<int>* E;  
  int* R;  
  bool* matched;

  matchStep() : E(NULL), R(NULL), matched(NULL) {}

  matchStep(pbbs::graph::edge<int>* _E, int* _R, bool* m) : E(_E), R(_R), matched(m) {}

  bool reserve(int i) {
    int u = E[i].u;
    int v = E[i].v;
    if (matched[u] || matched[v] || (u == v)) return 0;
    reserveLoc(R[u], i);
    reserveLoc(R[v], i);
    return 1;
  }

  bool commit(int i) {
    int u = E[i].u;
    int v = E[i].v;
    if (R[v] == i) {
      R[v] = INT_T_MAX;
      if (R[u] == i) {
        matched[u] = matched[v] = 1;
        return 1;
      } 
    } else if (R[u] == i) R[u] = INT_T_MAX;
    return 0;
  }
};

intT imax = (intT) INT_T_MAX;
intT zero = 0;
int granularity = 150;

struct notMax { bool operator() (intT i) {return i < INT_T_MAX;}};
    
class maximalMatching : public encore::edsl::pcfg::shared_activation_record {
public:

  pbbs::graph::edgeArray<int> G; std::pair<intT*,intT>* dest;
  intT n; intT m; intT *R; bool *matched;
  matchStep mStep; _seq<intT> matchingIdx; intT foo;

  maximalMatching(pbbs::graph::edgeArray<int> G, std::pair<intT*,intT>* dest)
    : G(G), dest(dest) { }
  
  encore_dc_declare(encore::edsl, maximalMatching, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        s.n = std::max(s.G.numCols, s.G.numRows);
        s.m = s.G.nonZeros;
        s.R = malloc_array<intT>(s.n);
        s.matched = malloc_array<bool>(s.n);
      }),
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        return sequence::fill3(st, pt, s.R, s.R + s.n, &imax);
      }),
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        return sequence::fill3(st, pt, s.matched, s.matched + s.n, &zero);
      }),
      dc::stmt([] (sar& s, par& p) {
        new (&s.mStep) matchStep(s.G.E, s.R, s.matched);
      }),
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        return speculative_for5(st, pt, s.mStep, 0, s.m, granularity, 0, -1, &s.foo);
      }),
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        return sequence::filter4(st, pt, s.R, s.n, notMax(), &s.matchingIdx);
      }),
      dc::stmt([] (sar& s, par& p) {
        free(s.R);
        free(s.matched);
        *s.dest = std::make_pair(s.matchingIdx.A, s.matchingIdx.n);
      })
    });
  }
  
};

encore_pcfg_allocate(maximalMatching, get_cfg)

} // end namespace

#include "loaders.hpp"
#include "graphio.hpp"
#include "graphutils.hpp"

#include "matching.h"
#include "gettime.h"
#undef blocked_for

namespace pbbs {

pbbs::graph::edgeArray<int> to_pbbs(pasl::pctl::graph::edgeArray<int>& g) {
  pbbs::graph::edge<int>* e = (pbbs::graph::edge<int>*) malloc(sizeof(pbbs::graph::edge<int>) * g.nonZeros);
  for (int i = 0; i < g.nonZeros; i++) {
    e[i] = pbbs::graph::edge<int>(g.E[i].u, g.E[i].v);
  }
  return pbbs::graph::edgeArray<int>(e, g.numRows, g.numCols, g.nonZeros);
}
  
void benchmark(std::string infile) {
  encorebench::granularity = deepsea::cmdline::parse_or_default_bool("speculative_for_grain", encorebench::granularity);
  pasl::pctl::graph::graph<int> x = pasl::pctl::io::load<pasl::pctl::graph::graph<int>>(infile);
  pasl::pctl::graph::edgeArray<int> edges = pasl::pctl::graph::to_edge_array(x);
  auto edges2 = to_pbbs(edges);
  std::pair<intT*, intT> result;
  deepsea::cmdline::dispatcher d;
  d.add("encore", [&] {
    encore::launch_interpreter<encorebench::maximalMatching>(edges2, &result);
  });
  d.add("pbbs", [&] {
    encorebench::run_and_report_elapsed_time([&] {
      result = maximalMatching(edges2);
    });
  });
  d.dispatch("algorithm");
  if (deepsea::cmdline::parse_or_default_bool("check", false)) {
    auto result2 = maximalMatching(edges2);
    assert(result.second == result2.second);
    auto n = result2.second;
    auto res1 = result.first;
    auto res2 = result2.first;
    for (auto i = 0; i != n; i++) {
      if (res1[i] != res2[i]) {
        auto c1 = res1[i];
        auto c2 = res2[i];
        std::cout << "bogus i=" << i << " result1[i]= " << c1 << " result2[i]= " << c2 << std::endl;
      }
      assert(res1[i] == res2[i]);
    }
    free(res2);
  }
  assert(result.first != nullptr);
  free(result.first);
}

} // end namespace

int main(int argc, char** argv) {
  encorebench::initialize(argc, argv);
  sequence::initialize();
  std::string infile = deepsea::cmdline::parse_or_default_string("infile", "");
  if (infile == "") {
    std::cout << "bogus input file name" << std::endl;
    exit(0);
  }
  pbbs::benchmark(infile);
  return 0;
}
