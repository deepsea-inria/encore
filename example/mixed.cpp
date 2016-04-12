#include <iostream>
#include <chrono>

#include "encore.hpp"

namespace sched = encore::sched;
namespace cmdline = pasl::util::cmdline;
namespace dsl = encore::edsl;

int cutoff = 1;

std::atomic<int> nb(0);

class mixed : public encore::edsl::pcfg::shared_activation_record {
public:
  
  static constexpr int nb_loops = 1;
  
  int n;
  sched::outset* f;
  
  mixed() { }
  
  mixed(int n)
  : n(n) { }
  
  class private_activation_record
  : public dsl::pcfg::parallel_for_private_activation_record<mixed,
  private_activation_record, nb_loops> {
  public:
    
    int lo; int hi;
    
  };
  
  encore_dc_loop_declare(encore::edsl, mixed, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return
    dc::mk_if([] (sar& s, par& p) { return s.n > 1; }, dc::stmts({
      dc::spawn_plus([] (sar& s, par& p, stt s) { return ecall<mixed>(s, n / 2); },
                     [] (sar& s, par& p) { return &s.f; }),
      dc::parallel_for_loop([] (sar& s, par& p) { return p.lo != p.hi; },
                            [] (sar& s, par& p) { return std::make_pair(&p.lo, p.hi); },
                            dc::stmt(
          dc::join_minus([] (sar& s, par& p) { return &s.f; })
#ifndef NDEBUG
          ,dc::stmt([] (sar&, par*) { nb++; })
#endif
        ))
    }));
  }
  
};

encore_pcfg_allocate(mixed, get_cfg)

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("n");
  cutoff = cmdline::parse_or_default("cutoff", cutoff);
  encore::run_and_report_elapsed_time([&] {
    encore::launch_interpreter<mixed>(n);
  });
  assert(nb.load() == n);
  return 0;
}
