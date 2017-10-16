#ifdef USE_CILK_PLUS
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#endif

#include "encore.hpp"

#ifndef _ENCORE_BENCH_H_
#define _ENCORE_BENCH_H_

void cilk_set_nb_cores(int proc) {
#ifdef USE_CILK_PLUS
  __cilkrts_set_param("nworkers", std::to_string(proc).c_str());
#endif
} 

namespace encorebench {

namespace cmdline = deepsea::cmdline;
  
void initialize(int argc, char** argv) {
  encore::initialize_runtime(argc, argv);
  cilk_set_nb_cores(cmdline::parse_or_default("proc", 1));
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

#endif /*! _ENCORE_BENCH_H_ */
