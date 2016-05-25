
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
  
  std::pair<vertex*, vertex*> split(int nb) {
    assert(false); // impossible
  }
  
};

class fib_cfg : public encore::edsl::pcfg::shared_activation_record {
public:
  
  int n; int* dp;
  int d1; int d2;
  
  fib_cfg() { }
  
  fib_cfg(int n, int* dp)
  : n(n), dp(dp) { }
  
  encore_pcfg_default_private_activation_record(encore::edsl::pcfg)
  encore_pcfg_declare(encore::edsl, fib_cfg, sar, par, bb)
  
  static
  cfg_type get_cfg() {
    enum { entry, branch1, branch2, combine, nb_blocks };
    cfg_type cfg(nb_blocks);
    cfg[entry] = bb::conditional_jump([] (sar& s, par&) {
      if (s.n <= cutoff) {
        *s.dp = fib(s.n);
        return 0; // going to exit block
      }
      return 1;  // going to branch1
    }, { encore::edsl::pcfg::exit_block_label, branch1 });
    cfg[branch1] = bb::spawn2_join([] (sar& s, par&, stt st) {
      return ecall<fib_cfg>(st, s.n - 1, &s.d1);
    }, branch2);
    cfg[branch2] = bb::spawn_join([] (sar& s, par&, stt st) {
      return ecall<fib_cfg>(st, s.n - 2, &s.d2);
    }, combine);
    cfg[combine] = bb::unconditional_jump([] (sar& s, par&) {
      *s.dp = s.d1 + s.d2;
    }, encore::edsl::pcfg::exit_block_label);
    return cfg;
  }
  
};

encore_pcfg_allocate(fib_cfg, get_cfg)

class fib_dc : public encore::edsl::pcfg::shared_activation_record {
public:
  
  int n; int* dp;
  int d1; int d2;
  
  fib_dc() { }
  
  fib_dc(int n, int* dp)
  : n(n), dp(dp) { }

  encore_dc_declare(encore::edsl, fib_dc, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::mk_if([] (sar& s, par&) { return s.n <= cutoff; },
               dc::stmt([] (sar& s, par&) { *s.dp = fib(s.n); }),
               dc::spawn2_join(
                   [] (sar& s, par&, stt st) {
                       return ecall<fib_dc>(st, s.n - 1, &s.d1); },
                   [] (sar& s, par&, stt st) {
                       return ecall<fib_dc>(st, s.n - 2, &s.d2); })
                     );
  }

  
};

encore_pcfg_allocate(fib_dc, get_cfg)

namespace cmdline = deepsea::cmdline;

#ifdef USE_CILK_PLUS
void initialize_cilk() {
  int nb_workers = cmdline::parse_or_default("proc", 1);
  std::string nb_workers_str = std::to_string(nb_workers);
  __cilkrts_set_param("nworkers", nb_workers_str.c_str());
}
#endif

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
    encore::launch_interpreter<fib_cfg>(n, &result);
  });
  d.add("dc", [&] {
    encore::launch_interpreter<fib_dc>(n, &result);
  });
  encore::run_and_report_elapsed_time([&] {
    d.dispatch("algorithm");
  });
  assert(result == fib(n));
  return 0;
}