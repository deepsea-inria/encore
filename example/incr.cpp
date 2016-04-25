
#include <iostream>
#include <chrono>

#include "encore.hpp"

namespace sched = encore::sched;
namespace cmdline = deepsea::cmdline;
namespace dsl = encore::edsl;

int cutoff = 1;

class incr : public dsl::pcfg::shared_activation_record {
public:
  
  enum { entry, header, body, exit, nb_blocks };
  
  enum { loop_0, nb_loops };
  
  class private_activation_record
  : public dsl::pcfg::parallel_for_private_activation_record<incr,
           private_activation_record, nb_loops> {
  public:
    
    int lo; int hi;
    
  };
  
  int n; int* a;
  
  incr(int n, int* a)
  : n(n), a(a) { }
  
  incr() { }
  
  encore_pcfg_declare(encore::edsl, incr, sar, par, bb)
  
  static
  cfg_type get_cfg() {
    cfg_type cfg(nb_blocks, nb_loops);
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
    register_loops();
    return cfg;
  }
  
  static
  void register_loops() {
    for (int i = 0; i < nb_blocks; i++) {
      cfg.loop_of[i] = dsl::pcfg::not_a_parallel_loop_id;
    }
    cfg.loop_of[header] = loop_0;
    cfg.loop_of[body] = loop_0;
    dsl::pcfg::parallel_loop_descriptor_type<private_activation_record> d;
    d.entry = { .pred=header, .succ=header };
    d.exit = { .pred=exit, .succ=exit };
    d.initializer = [] (private_activation_record& p, dsl::pcfg::parallel_for_descriptor& d) {
      d.lo = &p.lo;
      d.hi = &p.hi;
    };
    cfg.loop_descriptors[loop_0] = d;
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
