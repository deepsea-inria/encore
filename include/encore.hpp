
#ifdef USE_CILK_PLUS
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#endif

#ifdef HAVE_HWLOC
#include <hwloc.h>
#endif

#ifdef TARGET_MAC_OS
#include <sys/param.h>
#include <sys/sysctl.h>
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
      dc::spawn_join([] (sar& s, par&, plt, stt st) {
        return s.f(st);
      }),
      dc::stmt([] (sar& s, par&) {
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<float> diff = end - s.start;
        printf ("exectime %.3lf\n", diff.count());
        sched::should_exit = true;
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
  
void initialize_cpuinfo() {
  float cpu_frequency_mhz = 0.0;
#ifdef TARGET_LINUX
  /* Get information from /proc/cpuinfo.     *
   * cpu MHz         : <float>             # cpu frequency in MHz
   */
  FILE *cpuinfo_file = fopen("/proc/cpuinfo", "r");
  char buf[1024];
  int cache_line_szb;
  if (cpuinfo_file != NULL) {
    while (fgets(buf, sizeof(buf), cpuinfo_file) != 0) {
      sscanf(buf, "cpu MHz : %f", &(cpu_frequency_mhz));
    }
    fclose (cpuinfo_file);
  }
#endif
#ifdef TARGET_MAC_OS
  uint64_t freq = 0;
  size_t size;
  size = sizeof(freq);
  if (sysctlbyname("hw.cpufrequency", &freq, &size, NULL, 0) < 0) {
    perror("sysctl");
  }
  cpu_frequency_mhz = (float)freq / 1000000.;
#endif
  if (cpu_frequency_mhz == 0.) {
    atomic::die("Failed to read CPU frequency\n");
  }
  edsl::dc::cpu_frequency_ghz = (double) (cpu_frequency_mhz / 1000.0);
}

} // end namespace
  
void initialize(int argc, char** argv) {
  cmdline::set(argc, argv);
  atomic::init_print_lock();
  initialize_hwloc();
  initialize_cpuinfo();
  auto scheduler = cmdline::parse_or_default_string("scheduler", "steal_half_work_stealing");
  if (scheduler == "steal_half_work_stealing") {
    sched::scheduler = sched::steal_half_work_stealing_tag;
  } else if (scheduler == "steal_one_work_stealing") {
    sched::scheduler = sched::steal_one_work_stealing_tag;
  } else {
    atomic::die("bogus scheduler\n");
  }
  edsl::pcfg::never_promote = cmdline::parse_or_default_bool("never_promote", edsl::pcfg::never_promote);
  if (edsl::pcfg::never_promote) {
    sched::promotion_threshold = 1 << 20;
  }
  sched::promotion_threshold = cmdline::parse_or_default("promotion_threshold", sched::promotion_threshold);
  sched::sharing_threshold = cmdline::parse_or_default("sharing_threshold", 2 * sched::promotion_threshold);
  edsl::dc::kappa = cmdline::parse_or_default_double("kappa", edsl::dc::kappa);
  edsl::dc::leaf_loop_min_change_pct =
    cmdline::parse_or_default_double("leaf_loop_min_change_pct", edsl::dc::leaf_loop_min_change_pct);
  edsl::dc::leaf_loop_max_change_pct =
    cmdline::parse_or_default_double("leaf_loop_max_change_pct", edsl::dc::leaf_loop_max_change_pct);
  cilk_set_nb_cores();
}
  
template <class Init>
void launch(int nb_workers, const Init& init) {
  logging::log_buffer::initialize();
  stats::initialize();
  sched::vertex* v = init();
  stats::on_enter_launch();
  sched::launch_scheduler(nb_workers, v);
  stats::on_exit_launch();
  stats::report();
  logging::log_buffer::output();
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
  edsl::dc::loop_threshold = cmdline::parse_or_default("loop_threshold", edsl::dc::loop_threshold);
  logging::log_buffer::initialize();
  stats::initialize();
  auto interp = new edsl::pcfg::interpreter;
  auto ty = edsl::pcfg::cactus::Parent_link_sync;
  auto f = [=] (edsl::pcfg::stack_type st) {
    return edsl::pcfg::push_call<Shared_activation_record>(st, ty, args...);
  };
  using t = call_and_report_elapsed<typeof(f)>;
  interp->stack = edsl::pcfg::push_call<t>(interp->stack, ty, f);
  stats::on_enter_launch();
  sched::launch_scheduler(nb_workers, interp);
  stats::on_exit_launch();
  stats::report();
  logging::log_buffer::output();
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
