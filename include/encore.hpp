
#ifdef USE_CILK_PLUS
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#endif

#ifdef HAVE_HWLOC
#include <hwloc.h>
#endif

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
  
void cilk_set_nb_cores(int proc) {
#ifdef USE_CILK_PLUS
  __cilkrts_set_param("nworkers", std::to_string(proc).c_str());
#endif
}

void cilk_set_nb_cores() {
  cilk_set_nb_cores(cmdline::parse_or_default("proc", 1));
}

#ifdef HAVE_HWLOC
hwloc_topology_t topology;
#endif

void initialize_hwloc(int nb_workers) {
#ifdef HAVE_HWLOC
  hwloc_topology_init(&topology);
  hwloc_topology_load(topology);
  bool numa_alloc_interleaved = (nb_workers == 0) ? false : true;
  numa_alloc_interleaved = cmdline::parse_or_default("numa_alloc_interleaved", numa_alloc_interleaved);
  if (numa_alloc_interleaved) {
    hwloc_cpuset_t all_cpus =
      hwloc_bitmap_dup(hwloc_topology_get_topology_cpuset(topology));
    int err = hwloc_set_membind(topology, all_cpus, HWLOC_MEMBIND_INTERLEAVE, 0);
    if (err < 0) {
      printf("Warning: failed to set NUMA round-robin allocation policy\n");
    }
  }
#endif
}

void initialize_hwloc() {
  initialize_hwloc(cmdline::parse_or_default("proc", 1));
}

} // end namespace
  
void initialize(int argc, char** argv) {
  cmdline::set(argc, argv);
  initialize_hwloc();
  sched::D = cmdline::parse_or_default("dag_freq", sched::D);
  sched::K = cmdline::parse_or_default("sharing_freq", 2 * sched::D);
  cilk_set_nb_cores();
}
  
template <class Init>
void launch(int nb_workers, const Init& init) {
  stats::initialize();
  sched::vertex* v = init();
  sched::launch_scheduler(nb_workers, v);
  stats::report();
  data::perworker::reset();
}

void launch(sched::vertex* v, int nb_workers) {
  launch(nb_workers, [=] { return v; });
}
  
void launch(sched::vertex* v) {
  launch(v, cmdline::parse_or_default("proc", 1));
}
  
template <class Shared_activation_record, class ...Args>
void launch_interpreter(Args... args) {
  /* thanks to buggy GCC, the code here will crash the compiler...
  launch(cmdline::parse_or_default("proc", 1), [&] {
    auto interp = new edsl::pcfg::interpreter<edsl::pcfg::stack_type>;
    auto f = [=] (edsl::pcfg::stack_type st) {
      return edsl::pcfg::push_call<Shared_activation_record>(st, args...);
    };
    using t = call_and_report_elapsed<typeof(f)>;
    interp->stack = edsl::pcfg::push_call<t>(interp->stack, f);
    return interp;
  });
  */
  int nb_workers = cmdline::parse_or_default("proc", 1);
  stats::initialize();
  auto interp = new edsl::pcfg::interpreter<edsl::pcfg::stack_type>;
  auto f = [=] (edsl::pcfg::stack_type st) {
    return edsl::pcfg::push_call<Shared_activation_record>(st, args...);
  };
  using t = call_and_report_elapsed<typeof(f)>;
  interp->stack = edsl::pcfg::push_call<t>(interp->stack, f);
  sched::launch_scheduler(nb_workers, interp);
  stats::report();
  data::perworker::reset();
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
