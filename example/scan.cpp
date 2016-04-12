#include <iostream>
#include <chrono>

#ifdef USE_CILK_PLUS
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#else
#define cilk_for for
#endif

#include "encore.hpp"

namespace sched = encore::sched;
namespace cmdline = pasl::util::cmdline;
namespace dsl = encore::edsl;

int cutoff = 1;
int branching_factor = 1024;

using value_type = int;

template <class T>
T* malloc_array(size_t n) {
  return (T*)malloc(n * sizeof(T));
}

value_type reduce_serial(int lo, int hi, value_type z, value_type* src) {
  value_type r = z;
  for (int i = lo; i < hi; i++) {
    r += src[i];
  }
  return r;
}

value_type scan_serial(int lo, int hi, value_type z, value_type* src, value_type* dst) {
  value_type r = z;
  for (int i = lo; i < hi; i++) {
    dst[i] = r;
    r += src[i];
  }
  return r;
}

static inline
int get_nb_blocks(int k, int n) {
  return 1 + ((n - 1) / k);
}

static inline
std::pair<int, int> get_rng(int k, int n, int i) {
  int lo = i * k;
  int hi = std::min(lo + k, n);
  return std::make_pair(lo, hi);
}

class scan_dc : public encore::edsl::pcfg::shared_activation_record {
public:
  
  static constexpr int nb_loops = 2;
  
  int n;
  value_type z;
  value_type* src;
  value_type* dst;
  
  scan_dc() { }
  
  scan_dc(int n, value_type z, value_type* src, value_type* dst)
  : n(n), z(z), src(src), dst(dst) { }
  
  value_type* partials;
  value_type* scans;
  int m;
  
  class private_activation_record
  : public dsl::pcfg::parallel_for_private_activation_record<scan_dc,
  private_activation_record, nb_loops> {
  public:
    
    int lo; int hi;
    
  };
  
  encore_dc_loop_declare(encore::edsl, scan_dc, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return
    dc::mk_if([] (sar& s, par& p) { return s.n <= branching_factor; }, dc::stmt([] (sar& s, par& p) {
      scan_serial(0, s.n, s.z, s.src, s.dst);
    }), dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        s.m = get_nb_blocks(branching_factor, s.n);
        s.partials = malloc_array<value_type>(s.m);
        p.lo = 0;
        p.hi = s.m;
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.lo != p.hi; },
                            [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                            dc::stmt([] (sar& s, par& p) {
        int i = p.lo;
        int lo = get_rng(branching_factor, s.n, i).first;
        int hi = get_rng(branching_factor, s.n, i).second;
        s.partials[i] = reduce_serial(lo, hi, 0, s.src);
        p.lo++;
      })),
      dc::stmt([] (sar& s, par& p) {
        s.scans = malloc_array<value_type>(s.m);
      }),
      dc::spawn_join([] (sar& s, par& p, stt st) {
        return ecall<scan_dc>(st, s.m, s.z, s.partials, s.scans);
      }),
      dc::stmt([] (sar& s, par& p) {
        p.lo = 0;
        p.hi = s.m;
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.lo != p.hi; },
                            [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                            dc::stmt([] (sar& s, par& p) {
        int i = p.lo;
        int lo = get_rng(branching_factor, s.n, i).first;
        int hi = get_rng(branching_factor, s.n, i).second;
        scan_serial(lo, hi, s.scans[i], s.src, s.dst);
        p.lo++;
      })),
      dc::stmt([] (sar& s, par& p) {
        free(s.partials);
        free(s.scans);
      })
    }));
  }
  
};

encore_pcfg_allocate(scan_dc, get_cfg)

value_type scan_cilk(int n, value_type z, value_type* src, value_type* dst) {
  int k = branching_factor;
  value_type r = z;
  if (n <= k) {
    r = scan_serial(0, n, r, src, dst);
  } else {
    int m = get_nb_blocks(k, n);
    value_type* partials = malloc_array<int>(m);
    cilk_for (int i = 0; i < m; i++) {
      int lo = get_rng(k, n, i).first;
      int hi = get_rng(k, n, i).second;
      partials[i] = reduce_serial(lo, hi, 0, src);
    }
    value_type* scans = malloc_array<int>(m);
    r = scan_cilk(m, z, partials, scans);
    cilk_for (int i = 0; i < m; i++) {
      int lo = get_rng(k, n, i).first;
      int hi = get_rng(k, n, i).second;
      scan_serial(lo, hi, scans[i], src, dst);
    }
    free(partials);
    free(scans);
  }
  return r;
}

void cilk_set_nb_cores(int proc) {
#ifdef USE_CILK_PLUS
  __cilkrts_set_param("nworkers", std::to_string(proc).c_str());
#endif
}

void cilk_set_nb_cores() {
  cilk_set_nb_cores(cmdline::parse_or_default("proc", 1));
}

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("n");
  cutoff = cmdline::parse_or_default("cutoff", cutoff);
  branching_factor = cmdline::parse_or_default("branching_factor", branching_factor);
  cilk_set_nb_cores();
  value_type* src = malloc_array<value_type>(n);
  value_type* dst = malloc_array<value_type>(n);
  for (int i = 0; i < n; i++) {
    src[i] = i;
  }
  cmdline::dispatcher d;
  d.add("serial", [&] {
    scan_serial(0, n, 0, src, dst);
  });
  d.add("cilk", [&] {
    scan_cilk(n, 0, src, dst);
  });
  d.add("encore", [&] {
    encore::launch_interpreter<scan_dc>(n, 0, src, dst);
  });
  encore::run_and_report_elapsed_time([&] {
    d.dispatch("algorithm");
  });
#ifndef NDEBUG
  value_type* src2 = malloc_array<value_type>(n);
  value_type* dst2 = malloc_array<value_type>(n);
  std::copy(src, src + n, src2);
  scan_serial(0, n, 0, src2, dst2);
  for (int i = 0; i < n; i++) {
    assert(dst[i] == dst2[i]);
  }
  free(src2);
  free(dst2);
#endif
  free(src);
  free(dst);
  return 0;
}
