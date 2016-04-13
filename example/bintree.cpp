
#include <iostream>

#include "encore.hpp"

namespace sched = encore::sched;
namespace cmdline = pasl::util::cmdline;
namespace dsl = encore::edsl;

template <class T>
T* malloc_array(size_t n) {
  return (T*)malloc(n * sizeof(T));
}

int cutoff = 1;

class bintree_rec : public encore::edsl::pcfg::shared_activation_record {
public:
  
  int lo; int hi; int* a;
  sched::incounter** join;
  
  int mid;
  
  bintree_rec() { }
  
  bintree_rec(int lo, int hi, int* a, sched::incounter** join)
  : lo(lo), hi(hi), a(a), join(join) { }
  
  encore_dc_declare(encore::edsl, bintree_rec, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return
    dc::mk_if([] (sar& s, par&) {
      return s.hi - s.lo <= cutoff; },
      dc::stmt([] (sar& s, par&) {
        for (int i = s.lo; i < s.hi; i++) {
          s.a[s.lo]++;
        }
      }),
      dc::stmts({
        dc::stmt([] (sar& s, par&) { s.mid = (s.lo + s.hi) / 2; }),
        dc::spawn_minus([] (sar& s, par&, stt st) {
          return ecall<bintree_rec>(st, s.lo, s.mid, s.a, s.join); },
          [] (sar& s, par&) { return s.join; }),
        dc::spawn_minus([] (sar& s, par&, stt st) {
          return ecall<bintree_rec>(st, s.mid, s.hi, s.a, s.join); },
          [] (sar& s, par&) { return s.join; })
      }));
  }
  
};

encore_pcfg_allocate(bintree_rec, get_cfg)

class bintree : public encore::edsl::pcfg::shared_activation_record {
public:
  
  int n; int* a;
  sched::incounter* join = nullptr;
    
  bintree() { }
  
  bintree(int n, int* a)
  : n(n), a(a) { }
  
  encore_dc_declare(encore::edsl, bintree, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return
    dc::join_plus([] (sar& s, par&, stt st) {
      return ecall<bintree_rec>(st, 0, s.n, s.a, &s.join); },
      [] (sar& s, par&) { return &s.join; });
  }
  
};

encore_pcfg_allocate(bintree, get_cfg)

namespace cmdline = pasl::util::cmdline;

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("n");
  cutoff = cmdline::parse_or_default("cutoff", cutoff);
  int* a = malloc_array<int>(n);
  for (int i = 0; i < n; i++) {
    a[i] = 0xdeadbeef;
  }
  encore::launch_interpreter<bintree>(n, a);
#ifndef NDEBUG
  for (int i = 0; i < n; i++) {
    assert(a[i] == 0xdeadbeef + 1);
  }
#endif
  free(a);
  return 0;
}
