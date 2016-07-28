#include <iostream>
#include "encore.hpp"

namespace sched = encore::sched;
using handle = sched::incounter_handle*;

/*---------------------------------------------------------------------*/

double workload = 0.0;

void microsleep(double t) {
  long r = 0;
  for (long k = 0; k < (long) t; k++) {
    r += k;
  }
  if (r == 1234) {
    printf("ERROR\n");
  }
}

void do_dummy_work() {
  if (workload == 0.0) {
    return;
  }
  microsleep(workload);
}

#ifndef NDEBUG
std::atomic<int> nb_async(0);
#endif

/*---------------------------------------------------------------------*/

namespace dyn {
  
#ifndef ENCORE_INCOUNTER_SIMPLE
  
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
    
    async_rec(int nb, handle inc, std::pair<handle,handle> dec)
    : nb(nb), inc(inc), dec(dec), first_dec(false) { }
    
    int run(int fuel) {
      fuel--;
      if (nb < 2) {
        do_dummy_work();
        nb = 0;
#ifndef NDEBUG
        nb_async++;
#endif
        counter_decrement(this);
      } else {
        nb = std::max(1, nb / 2);
        auto r = counter_increment(this);
        auto d = std::get<0>(r);
        handle i = std::get<1>(r);
        handle j = std::get<2>(r);
        dec = d;
        inc = i;
        auto v = new async_rec(nb, j, d);
        v->left = false;
        schedule(v);
        schedule(this);
      }
      return fuel;
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
  
  int threshold = 100;
  
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
    auto ab = u->inc->touch(threshold);
    handle a = nullptr;
    handle b = nullptr;
    if (ab != nullptr) {
      a = ab->first;
      b = ab->second;
    }
    if (a == nullptr && a == b) {
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
    snzi_depart(claim_dec(u));
  }
  
  class async : public sched::vertex {
  public:
    
    int nb;
    bool first = true;
    std::chrono::time_point<std::chrono::system_clock> start;
    
    async(int nb)
    : nb(nb) { }
    
    int run(int fuel) {
      fuel--;
      if (first) {
        start = std::chrono::system_clock::now();
        auto h = &(get_incounter()->t.root);
        snzi_arrive(h);
        schedule(new async_rec(nb, h, std::make_pair(h, h)));
        first = false;
      } else {
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<float> diff = end - start;
        printf ("exectime %.3lf\n", diff.count());
        assert(nb_async.load() == nb);
        nb = 0;
      }
      return fuel;
    }
    
    int nb_strands() {
      return (nb == 0) ? 0 : 1;
    }
    
    std::pair<vertex*, vertex*> split(int nb) {
      assert(false);
      return std::make_pair(nullptr, nullptr);
    }
    
  };
  
#endif
  
} // end namespace

/*---------------------------------------------------------------------*/

namespace stat {
  
#ifndef NDEBUG
  std::atomic<int> nb_async(0);
#endif
  
  class async_rec : public sched::vertex {
  public:
    
    int nb;
    sched::vertex* finish;
    
    async_rec(int nb, sched::vertex* finish)
    : nb(nb), finish(finish) { }
    
    int run(int fuel) {
      fuel--;
      if (nb < 2) {
        do_dummy_work();
        nb = 0;
#ifndef NDEBUG
        nb_async++;
#endif
      } else {
        nb = std::max(1, nb / 2);
        auto v = new async_rec(nb, finish);
        sched::new_edge(v, finish);
        schedule(v);
        schedule(this);
      }
      return fuel;
    }
    
    int nb_strands() {
      return (nb == 0) ? 0 : 1;
    }
    
    std::pair<vertex*, vertex*> split(int nb) {
      assert(false);
      return std::make_pair(nullptr, nullptr);
    }
    
  };
  
  class async : public sched::vertex {
  public:
    
    int nb;
    bool first = true;
    std::chrono::time_point<std::chrono::system_clock> start;
    
    async(int nb)
    : nb(nb) { }
    
    int run(int fuel) {
      fuel--;
      if (first) {
        start = std::chrono::system_clock::now();
        auto v = new async_rec(nb, this);
        sched::new_edge(v, this);
        schedule(v);
        first = false;
      } else {
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<float> diff = end - start;
        printf ("exectime %.3lf\n", diff.count());
        assert(nb_async.load() == nb);
        nb = 0;
      }
      return fuel;
    }
    
    int nb_strands() {
      return (nb == 0) ? 0 : 1;
    }
    
    std::pair<vertex*, vertex*> split(int nb) {
      assert(false);
      return std::make_pair(nullptr, nullptr);
    }
    
  };
  
} // end namespace

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("n");
  workload = cmdline::parse_or_default("workload", workload);
  cmdline::dispatcher d;
#ifndef ENCORE_INCOUNTER_SIMPLE
  d.add("dyn", [&] {
    dyn::threshold = cmdline::parse_or_default("threshold", dyn::threshold);
    encore::launch(new dyn::async(n));
  });
#endif
  d.add("sta", [&] {
    encore::launch(new stat::async(n));
  });
  d.add("sim", [&] {
    encore::launch(new stat::async(n));
  });
  d.dispatch("algo");
  return 0;
}
