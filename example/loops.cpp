#include <iostream>
#include <chrono>

#include "encore.hpp"

namespace sched = encore::sched;
namespace cmdline = pasl::util::cmdline;
namespace dsl = encore::edsl;

int cutoff = 1;

class sequential_loop_0 : public encore::edsl::pcfg::shared_activation_record {
public:
  
  static constexpr int nb_loops = 1;
  
  int n;
  int* a; int lo; int hi;
  
  sequential_loop_0() { }
  
  sequential_loop_0(int n)
  : n(n) { }
  
  encore_dc_declare(encore::edsl, sequential_loop_0, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par&) {
        s.lo = 0;
        s.hi = s.n;
        s.a = new int[s.n];
      }),
      dc::sequential_loop([] (sar& s, par&) { return s.lo != s.hi; }, dc::stmt([] (sar& s, par&) {
        int lo = s.lo;
        int hi = std::min(s.hi, lo + cutoff);
        int* a = s.a;
        for (; lo != hi; lo++) {
          a[lo] = 0xdeadbeef;
        }
        s.lo = lo;
      })),
      dc::stmt([] (sar& s, par&) {
        for (int i = 0; i < s.n; i++) {
          assert(s.a[i] == 0xdeadbeef);
        }
        delete [] s.a;
      })
    });
  }
  
};

encore_pcfg_allocate(sequential_loop_0, get_cfg)

class sequential_loop_1 : public encore::edsl::pcfg::shared_activation_record {
public:
  
  static constexpr int nb_loops = 1;
  
  int n;
  int* a; int lo1; int hi1; int lo2; int hi2;
  
  sequential_loop_1() { }
  
  sequential_loop_1(int n)
  : n(n) { }
  
  encore_dc_declare(encore::edsl, sequential_loop_1, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par&) {
        s.lo1 = 0;
        s.hi1 = s.n;
        s.a = new int[s.n * s.n];
      }),
      dc::sequential_loop([] (sar& s, par&) { return s.lo1 != s.hi1; },
        dc::stmts({ dc::stmt([] (sar& s, par&) {
          s.lo2 = 0;
          s.hi2 = s.n;
        }),
        dc::sequential_loop([] (sar& s, par&) { return s.lo2 != s.hi2; }, dc::stmt([] (sar& s, par&) {
          int i = s.lo1;
          int j = s.lo2;
          s.a[i * s.n + j] = 0xdeadbeef;
          s.lo2++;
        })),
        dc::stmt([] (sar& s, par&) {
          s.lo1++;
        })
      })),
      dc::stmt([] (sar& s, par&) {
        for (int i = 0; i < s.n * s.n; i++) {
          assert(s.a[i] == 0xdeadbeef);
        }
        delete [] s.a;
      })
    });
  }
  
};

encore_pcfg_allocate(sequential_loop_1, get_cfg)

class parallel_loop_0 : public encore::edsl::pcfg::shared_activation_record {
public:
  
  static constexpr int nb_loops = 1;
  
  int n;
  int* a;
  
  parallel_loop_0() { }
  
  parallel_loop_0(int n)
  : n(n) { }
  
  class private_activation_record
  : public dsl::pcfg::parallel_loop_private_activation_record<parallel_loop_0,
  private_activation_record> {
  public:
    
    encore_parallel_loop_alloc_default(encore::edsl, nb_loops)

    
    int lo; int hi;
    
  };
  
  encore_dc_loop_declare(encore::edsl, parallel_loop_0, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        p.lo = 0;
        p.hi = s.n;
        s.a = new int[s.n];
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.lo != p.hi; },
                            [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                            dc::stmt([] (sar& s, par& p) {
        s.a[p.lo] = 0xdeadbeef;
        p.lo++;
      })),
      dc::stmt([] (sar& s, par&) {
        for (int i = 0; i < s.n; i++) {
          assert(s.a[i] == 0xdeadbeef);
        }
        delete [] s.a;
      })
    });
  }
  
};

encore_pcfg_allocate(parallel_loop_0, get_cfg)

class parallel_loop_1 : public encore::edsl::pcfg::shared_activation_record {
public:
  
  static constexpr int nb_loops = 2;
  
  int n;
  int* a;
  
  parallel_loop_1() { }
  
  parallel_loop_1(int n)
  : n(n) { }
  
  class private_activation_record
  : public dsl::pcfg::parallel_loop_private_activation_record<parallel_loop_1,
  private_activation_record> {
  public:
    
    encore_parallel_loop_alloc_default(encore::edsl, nb_loops)
    
    int lo1; int hi1;
    int lo2; int hi2;
    
  };
  
  encore_dc_loop_declare(encore::edsl, parallel_loop_1, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        p.lo1 = 0;
        p.hi1 = s.n;
        s.a = new int[s.n * s.n];
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.lo1 != p.hi1; },
                            [] (par& p) { return std::make_pair(&p.lo1, &p.hi1); },
                            dc::stmts({
        dc::stmt([] (sar& s, par& p) {
          p.lo2 = 0;
          p.hi2 = s.n;
        }),
        dc::parallel_for_loop([] (sar&, par& p) { return p.lo2 != p.hi2; },
                              [] (par& p) { return std::make_pair(&p.lo2, &p.hi2); },
                              dc::stmt([] (sar& s, par& p) {
          int i = p.lo1;
          int j = p.lo2;
          s.a[i * s.n + j] = 0xdeadbeef;
          p.lo2++;
        })),
        dc::stmt([] (sar& s, par& p) {
          p.lo1++;
        })
      })),
      dc::stmt([] (sar& s, par&) {
        for (int i = 0; i < s.n * s.n; i++) {
          assert(s.a[i] == 0xdeadbeef);
        }
        delete [] s.a;
      })
    });
  }
  
};

encore_pcfg_allocate(parallel_loop_1, get_cfg)

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("n");
  cutoff = cmdline::parse_or_default("cutoff", cutoff);
  encore::run_and_report_elapsed_time([&] {
    cmdline::dispatcher d;
    d.add("sequential_loop_0", [=] {
      encore::launch_interpreter<sequential_loop_0>(n);
    });
    d.add("sequential_loop_1", [=] {
      encore::launch_interpreter<sequential_loop_1>(n);
    });
    d.add("parallel_loop_0", [=] {
      encore::launch_interpreter<parallel_loop_0>(n);
    });
    d.add("parallel_loop_1", [=] {
      encore::launch_interpreter<parallel_loop_1>(n);
    });
    d.dispatch_or_default("function", "sequential_loop_0");
  });
  return 0;
}
