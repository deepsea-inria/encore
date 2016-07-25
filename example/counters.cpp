#include <iostream>
#include "encore.hpp"

namespace sched = encore::sched;
using handle = sched::incounter_handle*;

class async_rec;

std::tuple<std::pair<handle, handle>, handle, handle> counter_increment(async_rec* u);

void counter_decrement(async_rec* u);

class async_rec : public sched::vertex {
public:
  
  int nb;
  handle inc;
  std::pair<handle, handle> dec;
  std::atomic<bool> first_dec;
  bool left = true;
  
  async_rec() { }
  
  async_rec(int nb, handle inc, std::pair<handle,handle> dec)
  : nb(nb), inc(inc), dec(dec) { }
  
  int run(int) {
    if (nb >= 2) {
      int m = nb / 2;
      auto r1 = counter_increment(this);
      auto d = std::get<0>(r1);
      handle i = std::get<1>(r1);
      handle j = std::get<2>(r1);
      inc = i;
      auto v1 = new async_rec(m, j, d);
      auto r2 = counter_increment(this);
      d = std::get<0>(r2);
      i = std::get<1>(r2);
      j = std::get<2>(r2);
      inc = i;
      auto v2 = new async_rec(m, j, d);
      v2->left = false;
      schedule(v1);
      schedule(v2);
    }
    nb = 0;
    counter_decrement(this);
    return 1;
  }
  
  bool is_left_child() {
    return left;
  }
  
  int nb_strands() {
    return (nb == 0) ? 0 : 1;
  }
  
  std::pair<vertex*, vertex*> split(int nb) {
    assert(false);
    return std::make_pair(nullptr, nullptr);
  }
  
};

float threshold = 0.1;

void snzi_arrive(handle h) {
  h->increment();
}

void snzi_depart(handle h) {
  if (h->decrement()) {
    sched::vertex* v = sched::incounter_handle::get_root_annotation<sched::vertex*>(h);
    schedule(v);
  }
}

handle claim_dec(async_rec* u) {
  bool orig = false;
  if (u->first_dec.compare_exchange_strong(orig, true)) {
    return u->dec.first;
  } else {
    return u->dec.second;
  }
}

std::tuple<std::pair<handle, handle>, handle, handle> counter_increment(async_rec* u) {
  handle d1;
  handle d2;
  handle i1;
  handle i2;
  auto ab = u->inc->touch(1.0 / threshold);
  auto a = ab->first;
  auto b = ab->second;
  if (a == b && a == nullptr) {
    i1 = u->inc;
    i2 = u->inc;
    d2 = u->inc;
  } else {
    i1 = a;
    i2 = b;
    if (u->is_left_child()) {
      d2 = a;
    } else {
      d2 = b;
    }
  }
  snzi_arrive(d2);
  d1 = claim_dec(u);
  return std::make_tuple(std::make_pair(d1, d2), i1, i2);
}

void counter_decrement(async_rec* u) {
  handle d1 = claim_dec(u);
  snzi_depart(d1);
}

class async : public sched::vertex {
public:
  
  int nb;
  bool first = true;
  
  async(int nb)
  : nb(nb) { }
  
  int run(int) {
    if (first) {
      auto h = &(get_incounter()->t.root);
      h->increment();
      std::pair<handle, handle> d = std::make_pair(nullptr, nullptr);
      schedule(new async_rec(nb, h, d));
      first = false;
    } else {
      nb = 0;
    }
    return 1;
  }
  
  int nb_strands() {
    return (nb == 0) ? 0 : 1;
  }
  
  std::pair<vertex*, vertex*> split(int nb) {
    assert(false);
    return std::make_pair(nullptr, nullptr);
  }
  
};

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse_int("n");
  threshold = cmdline::parse_or_default("threshold", threshold);
  encore::launch(new async(n));
  return 0;
}
