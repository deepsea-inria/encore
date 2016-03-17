
#include "scheduler.hpp"
#include "edsl.hpp"
#include "cmdline.hpp"

#ifndef _ENCORE_H_
#define _ENCORE_H_

namespace cmdline = pasl::util::cmdline;

namespace encore {
  
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
  interp->stack = edsl::pcfg::push_call<Shared_activation_record>(interp->stack, args...);
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
