
#include <iostream>
#include <chrono>

#include "encore.hpp"

namespace sched = encore::sched;
namespace dsl = encore::edsl::pcfg;
namespace cmdline = pasl::util::cmdline;

class incr : public dsl::shared_activation_record {
public:
  
  encore_pcfg_driver
  
  enum { entry=0, header, body, nb_blocks };
  
  class private_activation_record : public dsl::private_activation_record {
  public:
    
    int lo; int hi;
    
    int nb_strands() {
      if (trampoline.pred == entry) {
        return 1;
      }
      return hi - lo;
    }
    
    template <class Stack>
    std::pair<sched::vertex*, sched::vertex*> split2(dsl::interpreter<Stack>* interp, int nb) {
      assert(lo + nb <= hi);
      incr& oldest_shared = dsl::peek_oldest_shared_frame<incr>(interp->stack);
      auto& oldest_private = *this;
      sched::vertex* interp1 = nullptr;
      auto interp2 = new dsl::interpreter<dsl::extended_stack_type>(&oldest_shared);
      interp2->release_handle->decrement();
      if (oldest_shared.join == nullptr) {
        auto stacks = dsl::slice_stack<incr>(interp->stack);
        interp->stack = stacks.first;
        auto v = new dsl::interpreter<dsl::extended_stack_type>(&oldest_shared);
        v->release_handle->decrement();
        dsl::extended_stack_type& stack1 = v->stack;
        stack1.stack.second = encore::cactus::push_back<private_activation_record>(stack1.stack.second, *this);
        auto& priv1 = dsl::peek_oldest_private_frame<private_activation_record>(v->stack);
        priv1.trampoline = { .pred=dsl::exit_block_label, .succ=dsl::exit_block_label };
        lo = hi = 0;
        interp1 = v;
        oldest_shared.join = interp;
        sched::new_edge(interp1, oldest_shared.join);
        oldest_private = priv1;
      } else {
        interp1 = interp;
      }
      dsl::extended_stack_type& stack2 = interp2->stack;
      stack2.stack.second = encore::cactus::push_back<private_activation_record>(stack2.stack.second);
      auto& priv2 = dsl::peek_oldest_private_frame<private_activation_record>(stack2);
      int mid = oldest_private.lo + nb;
      priv2.lo = mid;
      priv2.hi = oldest_private.hi;
      oldest_private.hi = mid;
      priv2.trampoline = { .pred=header, .succ=header };
      sched::new_edge(interp2, oldest_shared.join);
      return std::make_pair(interp1, interp2);
    }
    
    encore_pcfg_private_shared_driver(split2)
    
  };
  
  int n; int* a; sched::vertex* join;
  
  incr(int n, int* a)
  : n(n), a(a), join(nullptr) { }
  
  incr() { }
  
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
  
};

incr::cfg_type incr::cfg = incr::get_cfg();

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("n");
  int* a = new int[n];
  const int initial_value = 23;
  for (int i = 0; i < n; i++) {
    a[i] = initial_value;
  }
  encore::run_and_report_elapsed_time([&] {
    dsl::interpreter<dsl::stack_type>* interp = new dsl::interpreter<dsl::stack_type>;
    interp->stack = dsl::push_call<incr>(interp->stack, n, a);
    encore::launch(interp);
  });
  for (int i = 0; i < n; i++) {
    assert(a[i] == initial_value + 1);
  }
  delete [] a;
  return 0;
}
