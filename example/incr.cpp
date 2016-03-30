
#include <iostream>
#include <chrono>

#include "encore.hpp"

namespace sched = encore::sched;
namespace cmdline = pasl::util::cmdline;

int cutoff = 1;

class incr : public encore::edsl::pcfg::shared_activation_record {
public:
  
  enum { entry=0, header, body, nb_blocks };
  
  class private_activation_record : public encore::edsl::pcfg::private_activation_record {
  public:
    
    int lo; int hi;
    
    int nb_strands() {
      if (trampoline.pred == encore::edsl::pcfg::exit_block_label) {
        return 0;
      }
      return std::max(1, hi - lo);
    }
    
    template <class Stack>
    std::pair<sched::vertex*, sched::vertex*> split2(encore::edsl::pcfg::interpreter<Stack>* interp, int nb) {
      assert(lo + nb <= hi);
      incr& oldest_shared = encore::edsl::pcfg::peek_oldest_shared_frame<incr>(interp->stack);
      auto* oldest_private = this;
      sched::vertex* interp1 = nullptr;
      auto interp2 = new encore::edsl::pcfg::interpreter<encore::edsl::pcfg::extended_stack_type>(&oldest_shared);
      interp2->release_handle->decrement();
      if (oldest_shared.join == nullptr) {
        oldest_private->trampoline = { .pred=header, .succ=header };
        auto stacks = encore::edsl::pcfg::slice_stack<incr>(interp->stack);
        interp->stack = stacks.first;
        auto v = new encore::edsl::pcfg::interpreter<encore::edsl::pcfg::extended_stack_type>(&oldest_shared);
        v->release_handle->decrement();
        encore::edsl::pcfg::extended_stack_type& stack1 = v->stack;
        stack1.stack.second = encore::cactus::push_back<private_activation_record>(stack1.stack.second, *this);
        auto& priv1 = encore::edsl::pcfg::peek_oldest_private_frame<private_activation_record>(stack1);
        priv1.trampoline = { .pred=header, .succ=header };
        lo = hi = 0;
        interp1 = v;
        oldest_shared.join = interp;
        sched::new_edge(interp1, oldest_shared.join);
        oldest_private = &priv1;
      } else {
        interp1 = interp;
      }
      encore::edsl::pcfg::extended_stack_type& stack2 = interp2->stack;
      stack2.stack.second = encore::cactus::push_back<private_activation_record>(stack2.stack.second);
      auto& priv2 = encore::edsl::pcfg::peek_oldest_private_frame<private_activation_record>(stack2);
      int mid = oldest_private->lo + nb;
      priv2.lo = mid;
      priv2.hi = oldest_private->hi;
      oldest_private->hi = mid;
      priv2.trampoline = { .pred=header, .succ=header };
      sched::new_edge(interp2, oldest_shared.join);
      return std::make_pair(interp1, interp2);
    }
    
    encore_pcfg_private_shared_driver(encore::edsl::pcfg, split2)
    
  };
  
  int n; int* a; sched::vertex* join;
  
  incr(int n, int* a)
  : n(n), a(a), join(nullptr) { }
  
  incr() { }
  
  encore_pcfg_declare(encore::edsl, incr, sar, par, bb)
  
  static
  cfg_type get_cfg() {
    cfg_type cfg(nb_blocks);
    cfg[entry] = bb::unconditional_jump([] (sar& s, par& p) {
      p.lo = 0;
      p.hi = s.n;
    }, header);
    cfg[header] = bb::conditional_jump([] (sar& s, par& p) {
      return (p.hi == p.lo) ? 1 : 0;
    }, { body, encore::edsl::pcfg::exit_block_label });
    cfg[body] = bb::unconditional_jump([] (sar& s, par& p) {
      int* a = s.a;
      int i = p.lo;
      int n = std::min(p.hi, i + cutoff);
      for (; i < n; i++) {
        a[i]++;
      }
      p.lo = n;
    }, header);
    return cfg;
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
