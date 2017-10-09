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

#include "nearestneighbors.hpp"
#include "nearestNeighbors.h"
#undef blocked_for
#undef parallel_for
#include "loaders.hpp"

namespace pasl {
namespace pctl {

parray<pbbs::_point2d<double>> to_pbbs(parray<_point2d<double>>& points) {
  parray<pbbs::_point2d<double>> result(points.size());
  for (int i = 0; i < points.size(); i++) {
    result[i] = pbbs::_point2d<double>(points[i].x, points[i].y);
  }
  return result;
}

parray<pbbs::_point3d<double>> to_pbbs(parray<_point3d<double>>& points) {
  parray<pbbs::_point3d<double>> result(points.size());
  for (int i = 0; i < points.size(); i++) {
    result[i] = pbbs::_point3d<double>(points[i].x, points[i].y, points[i].z);
  }
  return result;
}
  
template <class Item1, class Item2, int K>
void benchmark(parray<Item1>& x, int k) {
  std::string algorithm = deepsea::cmdline::parse<std::string>("algorithm");
  deepsea::cmdline::dispatcher d;
  std::pair<intT*, intT*> res;
  int n = x.size();
  intT* result = malloc_array<intT>(k * n);
  d.add("encore", [&] {
    encore::launch_interpreter<encorebench::findNearestNeighbors<K, Item1>>(x.begin(), n, k, result);
  });
  d.add("pbbs", [&] {
    parray<Item2> y = to_pbbs(x);
    encore::run_and_report_elapsed_time([&] {
      pbbs::findNearestNeighbors<K, Item2>(&y[0], n, k, result);
    });
  });
  d.dispatch("algorithm");
  if (deepsea::cmdline::parse_or_default_bool("check", false)) {
    parray<Item2> y = to_pbbs(x);
    intT* result2 = malloc_array<intT>(k * n);
    pbbs::findNearestNeighbors<K, Item2>(&y[0], n, k, result2);
    for (auto i = 0; i < (k * n); i++) {
      auto x = result[i];
      auto y = result2[i];
      if (x != y) {
	encore::atomic::die("i=%d x=%d y=%d\n",i,x,y);
      }
      assert(x == y);
    }
    free(result2);
  }
  free(result);
}

void benchmark(std::string infile) {
  int k = deepsea::cmdline::parse_or_default_int("k", 1);
  deepsea::cmdline::dispatcher d;
  d.add("array_point2d", [&] {
    parray<_point2d<double>> x = io::load<parray<_point2d<double>>>(infile);
    benchmark<_point2d<double>, pbbs::_point2d<double>, 10>(x, k);
  });
  d.add("array_point3d", [&] {
    parray<_point3d<double>> x = io::load<parray<_point3d<double>>>(infile);
    benchmark<_point3d<double>, pbbs::_point3d<double>, 10>(x, k);
  });                                                                           
  d.dispatch("type");
}

} // end namespace
} // end namespace

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  std::string infile = deepsea::cmdline::parse_or_default_string("infile", "");
  if (infile == "") {
    std::cerr << "bogus input filename" << std::endl;
    exit(0);
  }
  pasl::pctl::benchmark(infile);
  return 0;
}
