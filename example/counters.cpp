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

std::atomic<int> nb_fanin(0);

std::atomic<int> nb_indegree2(0);

#endif

/*---------------------------------------------------------------------*/

namespace dyn {
  
#ifndef ENCORE_INCOUNTER_SIMPLE
  
  class fanin_rec;
  
  template <class T>
  std::tuple<std::pair<handle, handle>, handle, handle> counter_increment(T* u);
  
  template <class T>
  void counter_decrement(T* u);
  
  void snzi_arrive(handle h);
  
  class fanin_rec : public sched::vertex {
  public:
    
    int nb;
    handle inc;
    std::pair<handle, handle> dec;
    std::atomic<bool> first_dec;
    bool left = true;
    
    fanin_rec(int nb, handle inc, std::pair<handle,handle> dec)
    : nb(nb), inc(inc), dec(dec), first_dec(false), sched::vertex(false) { }
    
    int run(int fuel) {
      fuel--;
      if (nb < 2) {
        do_dummy_work();
        nb = 0;
#ifndef NDEBUG
        nb_fanin++;
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
        auto v = new fanin_rec(nb, j, d);
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
  
  class fanin : public sched::vertex {
  public:
    
    int nb;
    bool first = true;
    std::chrono::time_point<std::chrono::system_clock> start;
    
    fanin(int nb)
    : nb(nb) { }
    
    int run(int fuel) {
      fuel--;
      if (first) {
        start = std::chrono::system_clock::now();
        auto h = &(get_incounter()->t.root);
        snzi_arrive(h);
        schedule(new fanin_rec(nb, h, std::make_pair(h, h)));
        first = false;
      } else {
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<float> diff = end - start;
        printf ("exectime %.3lf\n", diff.count());
        assert(nb_fanin.load() == nb);
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
  
  class indegree2_rec : public sched::vertex {
  public:
    
    int nb;
    handle inc;
    std::pair<handle, handle> dec;
    std::atomic<bool> first_dec;
    bool left = true;
    bool first = true;
    sched::vertex* k = nullptr;
    
    indegree2_rec(int nb, handle inc, std::pair<handle,handle> dec)
    : nb(nb), inc(inc), dec(dec), first_dec(false), sched::vertex(false) { }
    
    int run(int fuel) {
      fuel--;
      if (! first) {
        nb = 0;
        if (k != nullptr) {
          release(k);
        } else {
          counter_decrement(this);
        }
        return fuel;
      }
      if (nb < 2) {
        do_dummy_work();
        nb = 0;
#ifndef NDEBUG
        nb_indegree2++;
#endif
        counter_decrement(this);
      } else {
        nb = std::max(1, nb / 2);
        auto i = &(get_incounter()->t.root);
        auto d = std::make_pair(i, i);
        auto v1 = new indegree2_rec(nb, i, d);
        auto r1 = counter_increment(v1);
        i = std::get<1>(r1);
        v1->dec = std::get<0>(r1);
        schedule(v1);
        auto v2 = new indegree2_rec(nb, i, d);
        auto r2 = counter_increment(v2);
        v2->dec = std::get<0>(r2);
        v2->left = false;
        schedule(v2);
        first = false;
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
  
  class indegree2 : public sched::vertex {
  public:
    
    int nb;
    bool first = true;
    std::chrono::time_point<std::chrono::system_clock> start;
    
    indegree2(int nb)
    : nb(nb) { }
    
    int run(int fuel) {
      fuel--;
      if (first) {
        start = std::chrono::system_clock::now();
        auto h = &(get_incounter()->t.root);
        snzi_arrive(h);
        auto v = new indegree2_rec(nb, h, std::make_pair(h, h));
        v->k = this;
        schedule(v);
        first = false;
      } else {
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<float> diff = end - start;
        printf ("exectime %.3lf\n", diff.count());
        assert(nb_indegree2.load() == nb);
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
  
  template <class T>
  handle claim_dec(T* u) {
    bool orig = false;
    if (u->first_dec.compare_exchange_strong(orig, true)) {
      return u->dec.first;
    } else {
      return u->dec.second;
    }
  }
  
  template <class T>
  std::tuple<std::pair<handle, handle>, handle, handle> counter_increment(T* u) {
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
  
  template <class T>
  void counter_decrement(T* u) {
    snzi_depart(claim_dec(u));
  }
  
#endif
  
} // end namespace

/*---------------------------------------------------------------------*/

namespace ori {
  
  class fanin_rec : public sched::vertex {
  public:
    
    int nb;
    sched::vertex* finish;
    handle h;
    
    fanin_rec(int nb, sched::vertex* finish)
    : nb(nb), finish(finish), sched::vertex(false) { }
    
    int run(int fuel) {
      fuel--;
      if (nb < 2) {
        do_dummy_work();
        nb = 0;
#ifndef NDEBUG
        nb_fanin++;
#endif
//        auto h1 = finish->get_incounter()->t.get_target_of_value(this);
//        assert(h1 == h);
        finish->get_incounter()->decrement(h);
      } else {
        nb = std::max(1, nb / 2);
        auto v = new fanin_rec(nb, finish);
        v->h = finish->get_incounter()->increment(v);
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
  
  class fanin : public sched::vertex {
  public:
    
    int nb;
    bool first = true;
    std::chrono::time_point<std::chrono::system_clock> start;
    
    fanin(int nb)
    : nb(nb) { }
    
    int run(int fuel) {
      fuel--;
      if (first) {
        start = std::chrono::system_clock::now();
        auto v = new fanin_rec(nb, this);
        v->h = get_incounter()->increment(v);
        schedule(v);
        first = false;
      } else {
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<float> diff = end - start;
        printf ("exectime %.3lf\n", diff.count());
        assert(nb_fanin.load() == nb);
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
  
  class indegree2_rec : public sched::vertex {
  public:
    
    int nb;
    handle h = nullptr;
    bool first = true;
    
    indegree2_rec(int nb)
    : nb(nb), sched::vertex(false) {
      get_incounter()->create_heap();
    }
    
    int run(int fuel) {
      fuel--;
      if (! first) {
        nb = 0;
        if (h != nullptr) {
//          auto h = k->get_incounter()->t.get_target_of_value(this);
          get_incounter()->decrement(h);
        }
        return fuel;
      }
      if (nb < 2) {
        do_dummy_work();
        nb = 0;
#ifndef NDEBUG
        nb_indegree2++;
#endif
//        auto h = k->get_incounter()->t.get_target_of_value(this);
        get_incounter()->decrement(h);
      } else {
        nb = std::max(1, nb / 2);
        auto v1 = new indegree2_rec(nb);
        v1->h = get_incounter()->increment(v1);
        schedule(v1);
        auto v2 = new indegree2_rec(nb);
        v2->h = get_incounter()->increment(v2);
        schedule(v2);
        first = false;
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
  
  class indegree2 : public sched::vertex {
  public:
    
    int nb;
    bool first = true;
    std::chrono::time_point<std::chrono::system_clock> start;
    
    indegree2(int nb)
    : nb(nb) { }
    
    int run(int fuel) {
      fuel--;
      if (first) {
        start = std::chrono::system_clock::now();
        auto v = new indegree2_rec(nb);
        sched::new_edge(v, this);
        schedule(v);
        first = false;
      } else {
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<float> diff = end - start;
        printf ("exectime %.3lf\n", diff.count());
        assert(nb_indegree2.load() == nb);
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

namespace stat {
  
#ifndef NDEBUG
  
  std::atomic<int> nb_fanin(0);
  
  std::atomic<int> nb_indegree2(0);
  
#endif
  
  class fanin_rec : public sched::vertex {
  public:
    
    int nb;
    sched::vertex* finish;
    
    fanin_rec(int nb, sched::vertex* finish)
    : nb(nb), finish(finish) { }
    
    int run(int fuel) {
      fuel--;
      if (nb < 2) {
        do_dummy_work();
        nb = 0;
#ifndef NDEBUG
        nb_fanin++;
#endif
      } else {
        nb = std::max(1, nb / 2);
        auto v = new fanin_rec(nb, finish);
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
  
  class fanin : public sched::vertex {
  public:
    
    int nb;
    bool first = true;
    std::chrono::time_point<std::chrono::system_clock> start;
    
    fanin(int nb)
    : nb(nb) { }
    
    int run(int fuel) {
      fuel--;
      if (first) {
        start = std::chrono::system_clock::now();
        auto v = new fanin_rec(nb, this);
        sched::new_edge(v, this);
        schedule(v);
        first = false;
      } else {
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<float> diff = end - start;
        printf ("exectime %.3lf\n", diff.count());
        assert(nb_fanin.load() == nb);
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
  
  class indegree2_rec : public sched::vertex {
  public:
    
    int nb;
    bool first = true;
    
    indegree2_rec(int nb)
    : nb(nb) { }
    
    int run(int fuel) {
      fuel--;
      if (! first) {
        nb = 0;
        return fuel;
      }
      if (nb < 2) {
        do_dummy_work();
        nb = 0;
#ifndef NDEBUG
        nb_indegree2++;
#endif
      } else {
        nb = std::max(1, nb / 2);
        auto v1 = new indegree2_rec(nb);
        auto v2 = new indegree2_rec(nb);
        sched::new_edge(v1, this);
        sched::new_edge(v2, this);
        release(v1);
        release(v2);
        first = false;
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
  
  class indegree2 : public sched::vertex {
  public:
    
    int nb;
    bool first = true;
    std::chrono::time_point<std::chrono::system_clock> start;
    
    indegree2(int nb)
    : nb(nb) { }
    
    int run(int fuel) {
      fuel--;
      if (first) {
        start = std::chrono::system_clock::now();
        auto v = new indegree2_rec(nb);
        sched::new_edge(v, this);
        release(v);
        first = false;
      } else {
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<float> diff = end - start;
        printf ("exectime %.3lf\n", diff.count());
        assert(nb_indegree2.load() == nb);
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

void launch_fanin(int n) {
  cmdline::dispatcher d;
#ifndef ENCORE_INCOUNTER_SIMPLE
  d.add("dyn", [&] {
    encore::launch(new dyn::fanin(n));
  });
#endif
  d.add("sta", [&] {
    encore::launch(new ori::fanin(n));
  });
  d.add("sta1", [&] {
    encore::launch(new ori::fanin(n));
  });
  d.add("sta2", [&] {
    encore::launch(new ori::fanin(n));
  });
  d.add("sta3", [&] {
    encore::launch(new ori::fanin(n));
  });
  d.add("sta4", [&] {
    encore::launch(new ori::fanin(n));
  });
  d.add("sta5", [&] {
    encore::launch(new ori::fanin(n));
  });
  d.add("sta6", [&] {
    encore::launch(new ori::fanin(n));
  });
  d.add("sta7", [&] {
    encore::launch(new ori::fanin(n));
  });
  d.add("sta8", [&] {
    encore::launch(new ori::fanin(n));
  });
  d.add("sta9", [&] {
    encore::launch(new ori::fanin(n));
  });
  d.add("sta10", [&] {
    encore::launch(new ori::fanin(n));
  });
  d.add("sim", [&] {
    encore::launch(new ori::fanin(n));
  });
  d.dispatch("algo");
}

void launc_indegree2(int n) {
  cmdline::dispatcher d;
#ifndef ENCORE_INCOUNTER_SIMPLE
  d.add("dyn", [&] {
    encore::launch(new dyn::indegree2(n));
  });
#endif
  d.add("sta", [&] {
    encore::launch(new ori::indegree2(n));
  });
  d.add("sta1", [&] {
    encore::launch(new ori::indegree2(n));
  });
  d.add("sta2", [&] {
    encore::launch(new ori::indegree2(n));
  });
  d.add("sta3", [&] {
    encore::launch(new ori::indegree2(n));
  });
  d.add("sta4", [&] {
    encore::launch(new ori::indegree2(n));
  });
  d.add("sta5", [&] {
    encore::launch(new ori::indegree2(n));
  });
  d.add("sta6", [&] {
    encore::launch(new ori::indegree2(n));
  });
  d.add("sta7", [&] {
    encore::launch(new ori::indegree2(n));
  });
  d.add("sta8", [&] {
    encore::launch(new ori::indegree2(n));
  });
  d.add("sta9", [&] {
    encore::launch(new ori::indegree2(n));
  });
  d.add("sta10", [&] {
    encore::launch(new ori::indegree2(n));
  });
  d.add("sim", [&] {
    encore::launch(new ori::indegree2(n));
  });
  d.dispatch("algo");
}

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("n");
  workload = cmdline::parse_or_default("workload", workload);
#ifndef ENCORE_INCOUNTER_SIMPLE
  dyn::threshold = cmdline::parse_or_default("threshold", dyn::threshold);
#endif
  cmdline::dispatcher d;
  d.add("fanin", [&] {
    launch_fanin(n);
  });
  d.add("indegree2", [&] {
    launc_indegree2(n);
  });
  d.dispatch("bench");
  return 0;
}
