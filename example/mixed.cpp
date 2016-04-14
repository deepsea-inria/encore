#include <iostream>
#include <chrono>

#include "encore.hpp"

namespace sched = encore::sched;
namespace cmdline = pasl::util::cmdline;
namespace dsl = encore::edsl;

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
    dc::mk_if([] (sar& s, par& p) { return s.n >= 1; }, dc::stmts({
      dc::spawn_plus([] (sar& s, par& p, stt st) { return ecall<mixed>(st, s.n / 2); },
                     [] (sar& s, par& p) { return &s.f; }),
      dc::stmt([] (sar& s, par& p) {
        p.lo = 0;
        p.hi = s.n;
      }),
      dc::parallel_for_loop([] (sar& s, par& p) { return p.lo != p.hi; },
                            [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                            dc::stmts({
        dc::join_minus([] (sar& s, par& p) { return &s.f; }),
        dc::stmt([] (sar& s, par& p) { p.lo++; })
#ifndef NDEBUG
        , dc::stmt([] (sar&, par&) { nb++; })
#endif
      })),
      dc::stmt([] (sar& s, par& p) {
        sched::delete_future(s.f);
      })
    }));
  }
  
};

encore_pcfg_allocate(mixed, get_cfg)

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = 1 << cmdline::parse<int>("lgn");
  encore::run_and_report_elapsed_time([&] {
    encore::launch_interpreter<mixed>(n);
  });
  assert(nb.load() + 1 == 2 * n);
  return 0;
}
