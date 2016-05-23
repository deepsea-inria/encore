
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

template <class ET, class intT, class F, class G>
intT maxIndexSerial(intT s, intT e, F f, G g) {
  ET r = g(s);
  intT k = 0;
  for (intT j=s+1; j < e; j++) {
    ET v = g(j);
    if (f(v,r)) { r = v; k = j;}
  }
  return k;
}

template <class ET, class intT, class F, class G>
ET scanSerial(ET* Out, intT s, intT e, F f, G g, ET zero, bool inclusive, bool back) {
  ET r = zero;
  
  if (inclusive) {
    if (back) for (intT i = e-1; i >= s; i--) Out[i] = r = f(r,g(i));
    else for (intT i = s; i < e; i++) Out[i] = r = f(r,g(i));
  } else {
    if (back)
      for (intT i = e-1; i >= s; i--) {
        ET t = g(i);
        Out[i] = r;
        r = f(r,t);
      }
    else
      for (intT i = s; i < e; i++) {
        ET t = g(i);
        Out[i] = r;
        r = f(r,t);
      }
  }
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

void bench_max_index() {
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
    encore::launch_interpreter<sequence::maxIndex<value_type,intT,typeof(f),typeof(g)>>(0, n, f, g, &result);
  } else if (algorithm == "encore_sequential") {
    encore::launch_interpreter<sequence::maxIndexSerial<value_type,intT,typeof(f),typeof(g)>>(0, n, f, g, &result);
  } else if (algorithm == "sequential") {
    result = maxIndexSerial<value_type>(0, n, f, g);
  }
  if (check) {
#ifndef NDEBUG
    value_type result2 = maxIndexSerial<value_type>(0, n, f, g);
#endif
    assert(result == result2);
  }
  free(input);
}

void bench_scan() {
  using intT = int;
  using value_type = int;
  intT n = cmdline::parse<intT>("n");
  bool check = cmdline::parse_or_default("check", false);
  std::string algorithm = cmdline::parse<std::string>("algorithm");
  bool inclusive = cmdline::parse_or_default("inclusive", false);
  bool back = cmdline::parse_or_default("back", false);
  value_type* input = malloc_array<value_type>(n);
  value_type* output = malloc_array<value_type>(n);
  value_type zero = 0;
  value_type result = (value_type)0;
  for (intT i = 0; i < n; i++) {
    input[i] = (value_type)i;
  }
  auto f = utils::addF<value_type>();
  auto g = sequence::getA<value_type,intT>(input);
  if (algorithm == "encore") {
    encore::launch_interpreter<sequence::scan<value_type,intT,typeof(f),typeof(g)>>(output, 0, n, f, g, zero, inclusive, back, &result);
  } else if (algorithm == "encore_sequential") {
    encore::launch_interpreter<sequence::scanSerial<value_type,intT,typeof(f),typeof(g)>>(output, 0, n, f, g, zero, inclusive, back, &result);
  } else if (algorithm == "sequential") {
    result = scanSerial<value_type>(output, (intT) 0, n, f, g, zero, inclusive, back);
  }
  if (check) {
#ifndef NDEBUG
    value_type* output2 = malloc_array<value_type>(n);
    value_type result2 = scanSerial(output2, (intT) 0, n, f, g, zero, inclusive, back);
    for (int i = 0; i < n; i++) {
      assert(output[i] == output2[i]);
    }
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
  d.add("max_index", [&] {
    bench_max_index();
  });
  d.add("scan", [&] {
    bench_scan();
  });
  d.dispatch("operation");
  return 0;
}