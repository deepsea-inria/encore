
#include <math.h>
#include <functional>
#include <stdlib.h>

#include "encorebench.hpp"
#include "geometryio.hpp"
#include "loaders.hpp"
#include "delaunay.hpp"
#undef blocked_for
#include "delaunay.h"

namespace pasl {
namespace pctl {

parray<pbbs::_point2d<double>> to_pbbs(parray<pasl::pctl::_point2d<double>>& points) {
  parray<pbbs::_point2d<double>> result(points.size());
  for (int i = 0; i < points.size(); i++) {
    result[i] = pbbs::_point2d<double>(points[i].x, points[i].y);
  }
  return result;
} 

void benchmark() {
  std::string infile = deepsea::cmdline::parse_or_default_string("infile", "");
  auto x = io::load<pasl::pctl::parray<_point2d<double>>>(infile);
  std::string algorithm = deepsea::cmdline::parse<std::string>("algorithm");
  deepsea::cmdline::dispatcher d;
  encorebench::triangles<_point2d<double>> res;
  int n = x.size();
  d.add("encore", [&] {
    encore::launch_interpreter<encorebench::delaunay>(&x[0], n, &res);
  });
  d.add("pbbs", [&] {
    auto y = to_pbbs(x);
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
  pasl::pctl::benchmark();
  return 0;
}


