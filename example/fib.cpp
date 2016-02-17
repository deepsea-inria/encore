#include <iostream>
#include "scheduler.hpp"
#include "edsl.hpp"

namespace dsl = pasl::edsl::pcfg;

int fib(int n) {
  if (n <= 1) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}

class fib_manual : public pasl::sched::vertex {
public:
  
  enum { entry, join, exit };
  int trampoline = entry;
  
  void yield_with(int target) {
    trampoline = target;
  }
  
  int n; int* dp;
  int d1, d2;
  
  fib_manual(int n, int* dp)
  : vertex(), n(n), dp(dp) { }
  
  int nb_strands() {
    if (trampoline == exit) {
      return 0;
    }
    return 1;
  }
  
  int run(int fuel) {
    switch (trampoline) {
      case entry: {
        if (n <= 1) {
          *dp = n;
          yield_with(exit);
          break;
        }
        fib_manual* b1 = new fib_manual(n - 1, &d1);
        fib_manual* b2 = new fib_manual(n - 2, &d2);
        yield_with(join);
        pasl::sched::new_edge(b2, this);
        pasl::sched::new_edge(b1, this);
        pasl::sched::release(b2);
        pasl::sched::release(b1);
        break;
      }
      case join: {
        *dp = d1 + d2;
        yield_with(exit);
        break;
      }
      default:
        assert(false);
    }
    return fuel - 1;
  }
  
  vertex* split(int nb) {
    assert(false); // impossible
  }
  
};

class fib_cfg : public dsl::activation_record {
public:
  
  int n; int* dp;
  int d1; int d2;
  
  fib_cfg() { }
  
  fib_cfg(int n, int* dp)
  : n(n), dp(dp) { }
  
  using env = fib_cfg;
  using cfg_type = dsl::cfg_type<env>;
  using bty = dsl::basic_block_type<env>;
  
  static
  cfg_type get_cfg() {
    cfg_type cfg;
    // 0
    cfg.push_back(bty::conditional_jump([] (env& e) {
      if (e.n <= 1) {
        *e.dp = e.n;
        return 0;
      }
      return 1;
    }, { dsl::exit_block_label, 1 }));
    // 1
    cfg.push_back(bty::fork2([&] (env& e, dsl::stack_type st) {
      return pasl::cactus::push_back<env>(st, e.n - 1, &e.d1);
    }, 2));
    // 2
    cfg.push_back(bty::fork1([] (env& e, dsl::stack_type st) {
      return pasl::cactus::push_back<env>(st, e.n - 2, &e.d2);
    }, 3));
    // 3
    cfg.push_back(bty::unconditional_jump([&] (env& e) {
      *e.dp = e.d1 + e.d2;
    }, -1));
    return cfg;
  }
  
  static
  cfg_type cfg;
  
  dsl::stack_type run(dsl::stack_type st) {
    return dsl::step(cfg, st);
  }
  
  void promote(dsl::interpreter* interp) {
     dsl::promote(cfg, interp);
  }
  
};

fib_cfg::cfg_type fib_cfg::cfg = fib_cfg::get_cfg();

void test1() {
  int n = 10;
  int d = -1;
  dsl::interpreter interp;
  assert(false); // push frame here
  interp.run(1000);
  assert(d == fib(n));
}

/*
void test2() {
  int n = 10;
  int d = -1;
  pasl::sched::release(new fib_manual(n, &d));
  pasl::sched::uniprocessor::scheduler_loop();
  assert(d == fib(n));
}

void test3() {
  int n = 3;
  dsl::dps_cell<int> d(-1);
  pasl::sched::release(new dsl::interpreter(new fib_cfg(n, &d)));
  pasl::sched::uniprocessor::scheduler_loop();
  assert(d.read() == fib(n));
} */

int main(int argc, const char * argv[]) {
//  test3();
  return 0;
}
