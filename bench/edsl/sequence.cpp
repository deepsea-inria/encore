
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

// sums a sequence of n boolean flags
// an optimized version that sums blocks of 4 booleans by treating
// them as an integer
// Only optimized when n is a multiple of 512 and Fl is 4byte aligned
template <class intT>
intT sumFlagsSerial(bool *Fl, intT n) {
  intT r = 0;
  if (n >= 128 && (n & 511) == 0 && ((long) Fl & 3) == 0) {
    int* IFl = (int*) Fl;
    for (int k = 0; k < (n >> 9); k++) {
      int rr = 0;
      for (int j=0; j < 128; j++) rr += IFl[j];
      r += (rr&255) + ((rr>>8)&255) + ((rr>>16)&255) + ((rr>>24)&255);
      IFl += 128;
    }
  } else for (intT j=0; j < n; j++) r += Fl[j];
  return r;
}

template <class ET, class intT, class F>
_seq<ET> packSerial(ET* Out, bool* Fl, intT s, intT e, F f) {
  if (Out == NULL) {
    intT m = sumFlagsSerial(Fl+s, e-s);
    Out = newA(ET,m);
  }
  intT k = 0;
  for (intT i=s; i < e; i++) if (Fl[i]) Out[k++] = f(i);
  return _seq<ET>(Out,k);
}

template <class ET, class intT, class PRED>
_seq<ET> filterSerial(ET* In, intT n, PRED p) {
  intT m = 0;
  for (int i = 0; i < n; i++) {
    if (p(In[i])) {
      m++;
    }
  }
  ET* A = malloc_array<ET>(m);
  _seq<ET> result(A, m);
  m = 0;
  for (int i = 0; i < n; i++) {
    if (p(In[i])) {
      A[m++] = In[i];
    }
  }
  return result;
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

template <class Item>
long count_diffs(long n, Item* trusted, Item* untrusted) {
  long nb_diffs = 0;
  for (long i = 0; i < n; i++) {
    Item& t = trusted[i];
    Item& u = untrusted[i];
    if (t != u) {
      nb_diffs++;
    }
  }
  return nb_diffs;
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
    input[i] = 1;
    output[i] = -1234;
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
    auto nb_diffs = count_diffs(n, output2, output);
    assert(nb_diffs == 0);
    free(output2);
#endif
    assert(result == result2);
  }
  free(input);
  free(output);
}

void bench_pack() {
  using intT = int;
  using value_type = int;
  intT n = std::max(2, cmdline::parse<intT>("n"));
  intT m = cmdline::parse_or_default("m", n / 2);
  bool check = cmdline::parse_or_default("check", false);
  std::string algorithm = cmdline::parse<std::string>("algorithm");
  value_type* input = malloc_array<value_type>(n);
  bool* flags = malloc_array<bool>(n);
  _seq<value_type> output;
  for (intT i = 0; i < n; i++) {
    input[i] = (value_type)i;
    flags[i] = rand() % 3 == 0;
  }
  auto f = sequence::getA<value_type,intT>(input);
  if (algorithm == "encore") {
    encore::launch_interpreter<sequence::pack<value_type, intT, typeof(f)>>((value_type*)nullptr, flags, (intT)0, n, f, &output);
  } else if (algorithm == "encore_sequential") {
    encore::launch_interpreter<sequence::packSerial<value_type, intT, typeof(f)>>((value_type*)nullptr, flags, (intT)0, n, f, &output);
  } else if (algorithm == "sequential") {
    output = packSerial((value_type*)nullptr, flags, (intT)0, n, f);
  }
  if (check) {
#ifndef NDEBUG
    _seq<value_type> output2 = packSerial((value_type*)nullptr, flags, (intT)0, n, f);
    assert(output.n == output2.n);
    for (int i = 0; i < output.n; i++) {
      auto u = output.A[i];
      auto t = output2.A[i];
      assert(u == t);
    }
    output2.del();
#endif
  }
  free(input);
  output.del();
}

void bench_filter() {
  using intT = int;
  using value_type = int;
  intT n = std::max(2, cmdline::parse<intT>("n"));
  intT m = cmdline::parse_or_default("m", n / 2);
  bool check = cmdline::parse_or_default("check", false);
  std::string algorithm = cmdline::parse<std::string>("algorithm");
  value_type* input = malloc_array<value_type>(n);
  bool* flags = malloc_array<bool>(n);
  _seq<value_type> output;
  for (intT i = 0; i < n; i++) {
    input[i] = (value_type)i;
    flags[i] = rand() % 3 == 0;
  }
  auto p = [&] (value_type v) {
    return flags[v];
  };
  if (algorithm == "encore") {
    encore::launch_interpreter<sequence::filter<value_type,intT,typeof(p)>>(input, n, p, &output);
  } else if (algorithm == "encore_sequential") {
    assert(false);
  } else if (algorithm == "sequential") {
    output = filterSerial(input, n, p);
  }
  if (check) {
#ifndef NDEBUG
    _seq<value_type> output2;
    output2 = filterSerial(input, n, p);
    assert(output.n == output2.n);
    for (int i = 0; i < output.n; i++) {
      auto u = output.A[i];
      auto t = output2.A[i];
      assert(t == u);
    }
    output2.del();
#endif
  }
  free(input);
  output.del();
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
  d.add("pack", [&] {
    bench_pack();
  });
  d.add("filter", [&] {
    bench_filter();
  });
  d.dispatch("operation");
  return 0;
}