#include "encorebench.hpp"
#include "loaders.hpp"
#include "kdtree.hpp"
#include "kdTree.h"

namespace encorebench {

template <class Item>
using parray = pasl::pctl::parray<Item>;

template <class Item1, class Item2>
parray<Item1> to_pbbs(parray<Item2>& a) {
  parray<Item1> result(a.size());
  for (int i = 0; i < a.size(); i++) {
    result[i] = to_pbbs(a[i]);
  }
  return result;
}

void benchmark(pasl::pctl::io::ray_cast_test& x) {
  deepsea::cmdline::dispatcher d;
  d.add("encore", [&] {
    pasl::pctl::triangles<pasl::pctl::_point3d<double>> tri(x.points.size(), x.triangles.size(), x.points.begin(), x.triangles.begin());
    intT* dest;
    encore::launch_interpreter<encorebench::rayCast>(tri, x.rays.begin(), x.rays.size(), &dest);
  });
  d.add("pbbs", [&] {
    parray<pbbs::_point3d<double>> points = to_pbbs<pbbs::_point3d<double>>(x.points);
    parray<pbbs::triangle> triangles = to_pbbs<pbbs::triangle>(x.triangles);
    parray<pbbs::ray<pbbs::_point3d<double>>> rays = to_pbbs<pbbs::ray<pbbs::_point3d<double>>>(x.rays);
    pbbs::triangles<pbbs::_point3d<double>> tri(points.size(), triangles.size(), points.begin(), triangles.begin());
    encorebench::run_and_report_elapsed_time([&] {
      pbbs::rayCast(tri, rays.begin(), rays.size());
    });
  });
  d.dispatch("algorithm");
}

} // end namespace

int main(int argc, char** argv) {
  encorebench::initialize(argc, argv);
  std::string infile = deepsea::cmdline::parse_or_default_string("infile", "");
  std::string base;
  std::string extension;
  pasl::pctl::io::parse_filename(infile, base, extension);
  if (extension == "txt") {
    std::string infile2 = deepsea::cmdline::parse_string("infile2");
    pasl::pctl::io::ray_cast_test x = pasl::pctl::io::load<pasl::pctl::io::ray_cast_test>(infile, infile2);
    benchmark(x);
  } else {
    pasl::pctl::io::ray_cast_test x = pasl::pctl::io::load<pasl::pctl::io::ray_cast_test>(infile);
    benchmark(x);
  }
  return 0;
}
