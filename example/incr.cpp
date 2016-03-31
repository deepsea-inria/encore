
#include <iostream>
#include <chrono>

#include "encore.hpp"

namespace sched = encore::sched;
namespace cmdline = pasl::util::cmdline;
namespace dsl = encore::edsl;

int cutoff = 1;

class incr : public dsl::pcfg::shared_activation_record {
public:
  
  enum { entry, header, body, exit, nb_blocks };
  
  enum { loop_nest_0, nb_loop_nests };
  
  enum { loop_0, nb_loops };
  
  class private_activation_record
  : public dsl::pcfg::parallel_for_private_activation_record<incr,
           private_activation_record, nb_loop_nests, nb_loops> {
  public:
    
    int lo; int hi;
    
    private_activation_record() {
      tables[loop_nest_0].nb = 1;
      dsl::pcfg::trampoline_type entry = { .pred=header, .succ=header };
      dsl::pcfg::trampoline_type eexit = { .pred=exit, .succ=exit };
      dsl::pcfg::parallel_for_descriptor d(lo, hi, entry, eexit);
      tables[loop_nest_0].descriptors[loop_0] = d;
    }
    
    loop_nest_id_type my_loop_nest_id() {
      return loop_nest_0;
    }
    
    loop_id_type my_loop_id() {
      return loop_0;
    }
    
  };
  
  int n; int* a;
  
  incr(int n, int* a)
  : n(n), a(a) { }
  
  incr() { }
  
  encore_pcfg_declare(encore::edsl, incr, sar, par, bb)
  
  static
  cfg_type get_cfg() {
    cfg_type cfg(nb_blocks);
    cfg[entry] = bb::unconditional_jump([] (sar& s, par& p) {
      p.lo = 0;
      p.hi = s.n;
    }, header);
    cfg[header] = bb::conditional_jump([] (sar& s, par& p) {
      return (p.hi == p.lo) ? 1 : 0;
    }, { body, exit });
    cfg[body] = bb::unconditional_jump([] (sar& s, par& p) {
      int* a = s.a;
      int i = p.lo;
      int n = std::min(p.hi, i + cutoff);
      for (; i < n; i++) {
        a[i]++;
      }
      p.lo = n;
    }, header);
    cfg[exit] = bb::unconditional_jump([] (sar&, par& p) {
      // nothing to do here
    }, encore::edsl::pcfg::exit_block_label);
    return cfg;
  }
  
};

encore_pcfg_allocate(incr, get_cfg)

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("n");
  cutoff = cmdline::parse_or_default("cutoff", cutoff);
  int* a = new int[n];
  const int initial_value = 23;
  for (int i = 0; i < n; i++) {
    a[i] = initial_value;
  }
  encore::run_and_report_elapsed_time([&] {
    encore::launch_interpreter<incr>(n, a);
  });
  for (int i = 0; i < n; i++) {
    assert(a[i] == initial_value + 1);
  }
  delete [] a;
  return 0;
}
