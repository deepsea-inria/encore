#include <iostream>
#include "scheduler.hpp"
#include "edsl.hpp"

namespace dsl = pasl::sched::edsl::pcfg;

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
  : n(n), dp(dp) { }
  
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
        pasl::sched::new_edge(b1, this); pasl::sched::new_edge(b2, this);
        pasl::sched::release(this); pasl::sched::release(b2); pasl::sched::release(b1);
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
  
  int n; dsl::dps_cell<int>* dp;
  
  dsl::dps_cell<int> d1; dsl::dps_cell<int> d2;
  
  fib_cfg() { }
  
  fib_cfg(int n, dsl::dps_cell<int>* dp)
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
        e.dp->write(e.n);
        return 0;
      }
      return 1;
    }, { dsl::exit_block_label, 1 }));
    // 1
    cfg.push_back(bty::fork2([&] (env& e, dsl::deque& dq) {
      dq.push_back<fib_cfg>(e.n - 1, &e.d1);
    }, 2));
    // 2
    cfg.push_back(bty::fork1([] (env& e, dsl::deque& dq) {
      dq.push_back<fib_cfg>(e.n - 2, &e.d2);
    }, 3));
    // 3
    cfg.push_back(bty::unconditional_jump([&] (env& e) {
      e.dp->write(e.d1.read() + e.d2.read());
    }, -1));
    return cfg;
  }
  
  static
  cfg_type cfg;
  
  void run(dsl::deque& dq) {
    dsl::step(cfg, dq);
  }
  
  void activate(dsl::deque& dq) {
    dq.push_back<env>(*this);
  }
  
  void copy(activation_record* _destination) {
    fib_cfg& destination = *((fib_cfg*)_destination);
    destination = *this;
  }
  
  void promote(activation_record* _destination) {
    fib_cfg& destination = *((fib_cfg*)_destination);
    d1.forward(destination.d1);
    d2.forward(destination.d2);
  }
  
  void discharge(dsl::deque& dq, dsl::interpreter* interp) {
    dsl::discharge(cfg, dq, interp);
  }
  
};

fib_cfg::cfg_type fib_cfg::cfg = fib_cfg::get_cfg();

int main(int argc, const char * argv[]) {
  dsl::dps_cell<int> result(-1);
  dsl::interpreter interp;
  dsl::deque& dq = interp.get_deque();
  dq.push_back<fib_cfg>(10, &result);
  interp.run(1000);
  std::cout << "result = " << result.read() << std::endl;
  return 0;
}
