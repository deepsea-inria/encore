
#include <iostream>
#include <chrono>
#ifdef USE_CILK_PLUS
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#endif

#include "encore.hpp"

int fib(int n) {
  if (n <= 1) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}

int cutoff = 1;

#ifdef USE_CILK_PLUS
int fib_cilk(int n) {
  if (n <= cutoff) {
    return fib(n);
  }
  int d1 = cilk_spawn fib_cilk(n - 1);
  int d2 = fib_cilk(n - 2);
  cilk_sync;
  return d1 + d2;
}
#endif

namespace sched = encore::sched;

class fib_manual : public sched::vertex {
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
        if (n <= cutoff) {
          *dp = fib(n);
          yield_with(exit);
          break;
        }
        fib_manual* b1 = new fib_manual(n - 1, &d1);
        fib_manual* b2 = new fib_manual(n - 2, &d2);
        yield_with(join);
        sched::new_edge(b2, this);
        sched::new_edge(b1, this);
        sched::release(b2);
        sched::release(b1);
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

namespace dsl = encore::edsl::pcfg;

class fib_cfg : public dsl::shared_activation_record {
public:
  
  class private_activation_record : public dsl::private_activation_record { };
  
  int n; int* dp;
  int d1; int d2;
  
  fib_cfg() { }
  
  fib_cfg(int n, int* dp)
  : n(n), dp(dp) { }
  
  using cfg_type = dsl::cfg_type<fib_cfg>;
  using bb = typename cfg_type::value_type;
  using sar = fib_cfg;
  using par = private_activation_record;
  
  static
  cfg_type get_cfg() {
    enum { entry=0, branch1, branch2, combine, nb_blocks };
    cfg_type cfg(nb_blocks);
    cfg[entry] = bb::conditional_jump([] (sar& s, par&) {
      if (s.n <= cutoff) {
        *s.dp = fib(s.n);
        return 0;
      }
      return 1;
    }, { dsl::exit_block_label, branch1 });
    cfg[branch1] = bb::spawn2_join([] (sar& s, par&, dsl::stack_type st) {
      return dsl::procedure_call<fib_cfg>(st, s.n - 1, &s.d1);
    }, branch2);
    cfg[branch2] = bb::spawn_join([] (sar& s, par&, dsl::stack_type st) {
      return dsl::procedure_call<fib_cfg>(st, s.n - 2, &s.d2);
    }, combine);
    cfg[combine] = bb::unconditional_jump([] (sar& s, par&) {
      *s.dp = s.d1 + s.d2;
    }, dsl::exit_block_label);
    return cfg;
  }
  
  static
  cfg_type cfg;
  
  std::pair<dsl::stack_type, int> run(dsl::stack_type stack, int fuel) const {
    return dsl::step(cfg, stack, fuel);
  }
  
  void promote(dsl::interpreter* interp) const {
     dsl::promote(cfg, interp);
  }
  
};

fib_cfg::cfg_type fib_cfg::cfg = fib_cfg::get_cfg();

namespace cmdline = pasl::util::cmdline;

#ifdef USE_CILK_PLUS
void initialize_cilk() {
  int nb_workers = cmdline::parse_or_default("proc", 1);
  std::string nb_workers_str = std::to_string(nb_workers);
  __cilkrts_set_param("nworkers", nb_workers_str.c_str());
}
#endif

template <class Function>
void run_and_report_elapsed_time(const Function& f) {
  auto start = std::chrono::system_clock::now();
  f();
  auto end = std::chrono::system_clock::now();
  std::chrono::duration<float> diff = end - start;
  printf ("exectime %.3lf\n", diff.count());
}

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("n");
  cutoff = cmdline::parse_or_default("cutoff", cutoff);
  int result = -1;
  cmdline::dispatcher d;
  d.add("sequential", [&] {
    result = fib(n);
  });
  d.add("dag", [&] {
    encore::launch(new fib_manual(n, &result));
  });
#ifdef USE_CILK_PLUS
  d.add("cilk", [&] {
    result = fib_cilk(n);
  });
#endif
  d.add("pcfg", [&] {
    dsl::interpreter* interp = new dsl::interpreter;
    interp->stack = dsl::procedure_call<fib_cfg>(interp->stack, n, &result);
    encore::launch(interp);
  });
  run_and_report_elapsed_time([&] {
    d.dispatch("algorithm");
  });
  assert(result == fib(n));
  return 0;
}
