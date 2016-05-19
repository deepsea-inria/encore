
#include <iostream>
#include <chrono>
#include <array>
#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>

#include "encore.hpp"
#include "sequence.hpp"

namespace sched = encore::sched;
namespace cmdline = deepsea::cmdline;
namespace dsl = encore::edsl;

template <class OT, class intT, class F, class G>
OT reduceSerial(intT s, intT e, F f, G g) {
  OT r = g(s);
  for (intT j=s+1; j < e; j++) r = f(r,g(j));
  return r;
}

void bench_reduce() {
  using intT = int;
  using value_type = int;
  intT n = cmdline::parse<intT>("n");
  bool check = cmdline::parse_or_default("check", false);
  std::string algorithm = cmdline::parse<std::string>("algorithm");
  value_type* input = malloc_array<value_type>(n);
  value_type result = (value_type)0;
  for (intT i = 0; i < n; i++) {
    input[i] = (value_type)i;
  }
  auto f = utils::addF<value_type>();
  auto g = sequence::getA<value_type,intT>(input);
  if (algorithm == "encore") {
    encore::launch_interpreter<sequence::reduce<value_type,intT,typeof(f),typeof(g)>>(0, n, f, g, &result);
  } else if (algorithm == "encore_sequential") {
    encore::launch_interpreter<sequence::reduceSerial<value_type,intT,typeof(f),typeof(g)>>(0, n, f, g, &result);
  } else if (algorithm == "sequential") {
    result = reduceSerial<value_type>(0, n, f, g);
  }
  if (check) {
#ifndef NDEBUG
    value_type result2 = reduceSerial<value_type>(0, n, f, g);
#endif
    assert(result == result2);
  }
  free(input);
}

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  sequence::threshold = cmdline::parse_or_default("threshold", sequence::threshold);
  sequence::block_size = cmdline::parse_or_default("block_size", sequence::block_size);
  cmdline::dispatcher d;
  d.add("reduce", [&] {
    bench_reduce();
  });
  d.dispatch("operation");
  return 0;
}