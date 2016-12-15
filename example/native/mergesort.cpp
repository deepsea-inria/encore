
#include <iostream>
#ifdef USE_VALGRIND
#include <valgrind/valgrind.h>
#endif

#include "encore.hpp"
#include "native.hpp"

namespace cmdline = deepsea::cmdline;
using namespace encore::native;

void print_array(int* xs, int lo, int hi) {
  std::cout << "{";
  for (int i = lo; i < hi; i++) {
    std::cout << xs[i];
    if (i + 1 == hi) {
      std::cout << "}";
    } else {
      std::cout << ", ";
    }
  }
  std::cout << std::endl;
}

void print_array(int* xs, int n) {
  print_array(xs, 0, n);
}

template <class Item, class Compare>
long lower_bound(Item* xs, long lo, long hi, const Item& val, const Compare& compare) {
  return std::lower_bound(xs + lo, xs + hi, val, compare) - xs;
}

template <class Item, class Compare>
encore_function
void merge_par(Item* xs, Item* ys, Item* tmp,
               long lo_xs, long hi_xs,
               long lo_ys, long hi_ys,
               long lo_tmp,
               const Compare& compare) {
  long n1 = hi_xs - lo_xs;
  long n2 = hi_ys - lo_ys;
  if (n1 < n2) {
    merge_par(ys, xs, tmp, lo_ys, hi_ys, lo_xs, hi_xs, lo_tmp, compare);
  } else if (n1 == 0) {
    // do nothing
  } else if (n1 == 1) {
    if (n2 == 0) {
      tmp[lo_tmp] = xs[lo_xs];
    } else {
      tmp[lo_tmp+0] = std::min(xs[lo_xs], ys[lo_ys], compare);
      tmp[lo_tmp+1] = std::max(xs[lo_xs], ys[lo_ys], compare);
    }
  } else {
    long mid_xs = (lo_xs + hi_xs) / 2;
    long mid_ys = lower_bound(ys, lo_ys, hi_ys, xs[mid_xs], compare);
    long k = (mid_xs - lo_xs) + (mid_ys - lo_ys);
    merge_par(xs, ys, tmp, lo_xs, mid_xs, lo_ys, mid_ys, lo_tmp, compare);
    merge_par(xs, ys, tmp, mid_xs, hi_xs, mid_ys, hi_ys, lo_tmp+k, compare);
  }
}

template <class Item, class Compare>
void merge_seq(const Item* xs, const Item* ys, Item* tmp,
               long lo_xs, long hi_xs,
               long lo_ys, long hi_ys,
               long lo_tmp,
               const Compare& compare) {
  std::merge(&xs[lo_xs], &xs[hi_xs], &ys[lo_ys], &ys[hi_ys], &tmp[lo_tmp], compare);
}

template <class Item, class Compare>
encore_function
void merge(Item* xs, Item* tmp,
           long lo, long mid, long hi,
           const Compare& compare) {
  merge_par(xs, xs, tmp, lo, mid, mid, hi, lo, compare);
  std::copy(tmp + lo, tmp + hi, xs + lo);
}

template <class Item, class Compare>
encore_function
void mergesort(Item* xs, Item* tmp, long lo, long hi, const Compare& compare) {
  if (lo + 1 >= hi) {
    return;
  }
  long mid = (lo + hi) / 2;
  if (hi - lo <= 2) {
    int x = xs[lo];
    int y = xs[mid];
    xs[lo]  = std::min(x, y, compare);
    xs[mid] = std::max(x, y, compare);
    return;
  }
  mergesort(xs, tmp, lo, mid, compare);
  mergesort(xs, tmp, mid, hi, compare);
  merge(xs, tmp, lo, mid, hi, compare);
}

template <class Item>
void permute(Item* xs, int sz, int nb) {
  for (int i = 0; i < nb; i++) {
    int t1 = rand() % sz;
    int t2 = rand() % sz;
    if (t1 == t2) {
      continue;
    }
    std::swap(xs[t1], xs[t2]);
  }
}

void check(int* xs, int n) {
  for (int i = 0; i < n; i++) {
    int x = xs[i];
    if (x != i) {
      std::cout << "bogus value " << xs[i] << " at index " << i << std::endl;
    }
  }
}

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("n");
  int* xs = new int[n];
  for (int i = 0; i < n; i++) {
    xs[i] = i;
  }
//  print_array(xs, n);
  permute(xs, n, cmdline::parse_or_default("nb", n / 2));
  int* tmp = new int[n];
  auto v = new encore::native::vertex;
  v->mailbox.tag = encore::native::tag_start_function;
  v->mailbox.start_function = [&] {
    mergesort(xs, tmp, 0, n, [] (int x, int y) {
      return x <= y;
    });
  };
#if 0
  membar(encore::launch(v));
#else
//  print_array(xs, n);
  v->mailbox.start_function();
  delete v;
#endif
//  print_array(xs, n);
  check(xs, n);
  delete [] tmp;
  delete [] xs;
  return 0;
}
