
#include <math.h>
#include <functional>
#include <stdlib.h>

#include "encorebench.hpp"
#include "delaunay.hpp"
#include "loaders.hpp"
#include "delaunay.h"

/***********************************************************************/

/*---------------------------------------------------------------------*/

namespace pasl {
namespace pctl {

parray<pbbs::_point2d<double>> to_pbbs(parray<pasl::pctl::_point2d<double>>& points) {
  parray<pbbs::_point2d<double>> result(points.size());
  for (int i = 0; i < points.size(); i++) {
    result[i] = pbbs::_point2d<double>(points[i].x, points[i].y);
  }
  return result;
}

template <class Item1, class Item2>
void benchmark(parray<Item1>& x) {
  std::string algorithm = deepsea::cmdline::parse<std::string>("algorithm");
  deepsea::cmdline::dispatcher d;
  encorebench::triangles<Item1> res;
  int n = x.size();
  d.add("encore", [&] {
    encore::launch_interpreter<encorebench::delaunay>(&x[0], n, &res);
  });
  d.add("pbbs", [&] {
    parray<Item2> y = to_pbbs(x);
    encorebench::run_and_report_elapsed_time([&] {
      pbbs::delaunay(&y[0], (int)y.size());
    });
  });
  d.dispatch("algorithm");
}
  
} // end namespace
} // end namespace

int main(int argc, char** argv) {
  encorebench::initialize(argc, argv);
  std::string infile = deepsea::cmdline::parse_or_default_string("infile", "");
  parray<pasl::pctl::_point2d<double>> x = pasl::pctl::io::load<parray<pasl::pctl::_point2d<double>>>(infile);
  pasl::pctl::benchmark(x);
  return 0;
}


