
#include <iostream>
#include <chrono>
#ifdef USE_CILK_PLUS
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#endif

#include "encore.hpp"

namespace sched = encore::sched;
namespace dsl = encore::edsl::pcfg;
namespace cmdline = pasl::util::cmdline;

class incr : public dsl::shared_activation_record {
public:
  
  enum { entry=0, header, body, nb_blocks };
  
  class private_activation_record : public dsl::private_activation_record {
  public:
    
    int lo; int hi;
    
    int nb_strands() {
      return hi - lo;
    }
    
    split_result_type split(dsl::interpreter<dsl::stack_type>* interp, int nb) {
      assert(lo + nb <= hi);
      sched::vertex* interp1 = nullptr;
      incr& oldest_shared = dsl::peek_oldest_shared_frame<incr>(interp->stack);
      auto& oldest_private = *this;
      if (oldest_shared.join == nullptr) {
        auto stacks = dsl::slice_stack<incr>(interp->stack);
        interp->stack = stacks.first;
        auto v = new dsl::interpreter<dsl::extended_stack_type>(&oldest_shared);
        v->release_handle->decrement();
        dsl::extended_stack_type& stack1 = v->stack;
        stack1.stack.second = encore::cactus::push_back<private_activation_record>(stack1.stack.second);
        auto& priv1 = dsl::peek_oldest_private_frame<private_activation_record>(v->stack);
        priv1.trampoline = { .pred=dsl::exit_block_label, .succ=dsl::exit_block_label };
        priv1.lo = lo; priv1.hi = hi;
        lo = hi = 0;
        interp1 = v;
        oldest_shared.join = interp;
        sched::new_edge(interp1, oldest_shared.join);
        oldest_private = priv1;
      } else {
        interp1 = interp;
      }
      auto interp2 = new dsl::interpreter<dsl::extended_stack_type>(&oldest_shared);
      dsl::extended_stack_type& stack2 = interp2->stack;
      stack2.stack.second = encore::cactus::push_back<private_activation_record>(stack2.stack.second);
      auto& priv2 = dsl::peek_oldest_private_frame<private_activation_record>(stack2);
      int mid = oldest_private.lo + nb;
      priv2.lo = mid;
      priv2.hi = oldest_private.hi;
      oldest_private.hi = mid;
      priv2.trampoline = { .pred=header, .succ=header };
      sched::new_edge(interp2, oldest_shared.join);
      interp2->release_handle->decrement();
      return std::make_pair(interp1, interp2);
    }
    
  };
  
  int n; int* a; sched::vertex* join;
  
  using cfg_type = dsl::cfg_type<incr>;
  using bb = typename cfg_type::value_type;
  using sar = incr;
  using par = private_activation_record;
  
  static
  cfg_type get_cfg() {
    cfg_type cfg(nb_blocks);
    cfg[entry] = bb::unconditional_jump([] (sar& s, par& p) {
      p.lo = 0;
      p.hi = s.n;
    }, header);
    cfg[header] = bb::conditional_jump([] (sar& s, par& p) {
      return (p.hi == p.lo) ? 1 : 0;
    }, { body, dsl::exit_block_label });
    cfg[body] = bb::unconditional_jump([] (sar& s, par& p) {
      int* a = s.a;
      int i = p.lo;
      a[i]++;
      p.lo++;
    }, header);
    return cfg;
  }
  
  static
  cfg_type cfg;
  
  std::pair<dsl::stack_type, int> run(dsl::stack_type stack, int fuel) const {
    return dsl::step(cfg, stack, fuel);
  }
  
  std::pair<dsl::extended_stack_type, int> run(dsl::extended_stack_type stack, int fuel) const {
    return dsl::step(cfg, stack, fuel);
  }
  
  void promote(dsl::interpreter<dsl::stack_type>* interp) const {
    dsl::promote(cfg, interp);
  }
  
  void promote(dsl::interpreter<dsl::extended_stack_type>* interp) const {
    dsl::promote(cfg, interp);
  }
  
};

incr::cfg_type incr::cfg = incr::get_cfg();

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  cmdline::dispatcher d;
  d.add("sequential", [&] {
  });
  d.add("dag", [&] {
  });
  encore::run_and_report_elapsed_time([&] {
    d.dispatch("algorithm");
  });
  return 0;
}
