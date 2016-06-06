
#include "scheduler.hpp"
#include "edsl.hpp"
#include "cmdline.hpp"

#ifndef _ENCORE_H_
#define _ENCORE_H_

namespace cmdline = deepsea::cmdline;

namespace encore {
  
namespace {
  
template <class Function>
class call_and_report_elapsed : public edsl::pcfg::shared_activation_record {
public:
  
  Function f;
  std::chrono::time_point<std::chrono::system_clock> start;
  
  call_and_report_elapsed() { }
  
  call_and_report_elapsed(const Function& f)
  : f(f) { }
  
  encore_dc_declare(encore::edsl, call_and_report_elapsed, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par&) {
        s.start = std::chrono::system_clock::now();
      }),
      dc::spawn_join([] (sar& s, par&, stt st) {
        return s.f(st);
      }),
      dc::stmt([] (sar& s, par&) {
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<float> diff = end - s.start;
        printf ("exectime %.3lf\n", diff.count());
      })
    });
  }
  
};

template <class Function>
typename call_and_report_elapsed<Function>::cfg_type call_and_report_elapsed<Function>::cfg = call_and_report_elapsed<Function>::get_cfg();
  
} // end namespace
  
void initialize(int argc, char** argv) {
  cmdline::set(argc, argv);
  sched::D = cmdline::parse_or_default("dag_freq", sched::D);
  sched::K = cmdline::parse_or_default("sharing_freq", 2 * sched::D);
}

void launch(sched::vertex* v, int nb_workers) {
  sched::launch_scheduler(nb_workers, v);
  stats::report();
}
  
void launch(sched::vertex* v) {
  launch(v, cmdline::parse_or_default("proc", 1));
}
  
template <class Shared_activation_record, class ...Args>
void launch_interpreter(Args... args) {
  auto interp = new edsl::pcfg::interpreter<edsl::pcfg::stack_type>;
  auto f = [=] (edsl::pcfg::stack_type st) {
    return edsl::pcfg::push_call<Shared_activation_record>(st, args...);
  };
  using t = call_and_report_elapsed<typeof(f)>;
  interp->stack = edsl::pcfg::push_call<t>(interp->stack, f);
  launch(interp);
}
  
template <class Function>
void run_and_report_elapsed_time(const Function& f) {
  auto start = std::chrono::system_clock::now();
  f();
  auto end = std::chrono::system_clock::now();
  std::chrono::duration<float> diff = end - start;
  printf ("exectime %.3lf\n", diff.count());
}

} // end namespace

#endif /*! _ENCORE_H_ */
