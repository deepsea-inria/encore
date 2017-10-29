
#include "encorebench.hpp"
#include "geometryio.hpp"
#include "refine.hpp"
#include "loaders.hpp"
#include "refine.h"

namespace pasl {
namespace pctl {

pbbs::triangles<pbbs::_point2d<double>> to_pbbs(pasl::pctl::triangles<pasl::pctl::_point2d<double>>& x) {
  pbbs::triangles<pbbs::_point2d<double>> result;
  result.numPoints = x.num_points;
  result.P = (pbbs::_point2d<double>*) malloc(sizeof(pbbs::_point2d<double>) * x.num_points);
  for (int i = 0; i < x.num_points; i++) {
    result.P[i] = pbbs::_point2d<double>(x.p[i].x, x.p[i].y);
  }
  result.numTriangles = x.num_triangles;
  result.T = (pbbs::triangle*) malloc(sizeof(pbbs::triangle) * x.num_triangles);
  for (int i = 0; i < x.num_triangles; i++) {
    result.T[i] = pbbs::triangle(x.t[i].vertices[0], x.t[i].vertices[1], x.t[i].vertices[2]);
  }
  return result;
}

void benchmark() {
  std::string infile = deepsea::cmdline::parse_or_default_string("infile", "");
  auto x = pasl::pctl::io::load<pasl::pctl::triangles<pasl::pctl::_point2d<double>>>(infile);
  std::string algorithm = deepsea::cmdline::parse<std::string>("algorithm");
  deepsea::cmdline::dispatcher d;
  //  encorebench::triangles<_point2d<double>> res;
  d.add("encore", [&] {
      //    encore::launch_interpreter<encorebench::refine>(x, &res);
  });
  d.add("pbbs", [&] {
    auto y = to_pbbs(x);
    encorebench::run_and_report_elapsed_time([&] {
        //      pbbs::refine(y);
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


