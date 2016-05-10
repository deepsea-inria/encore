
#include <ucontext.h>
#include <signal.h>

#include "vertex.hpp"
#include "cactus.hpp"
#include "stats.hpp"
#include "context.hpp"

namespace encore {
namespace native {
  
using stack_type = cactus::stack_type;

context_type dummy;
  
class activation_record_template {
public:
  
  int szb;
  
  virtual inline
  context_type& get_context() {
    assert(false);
    return dummy;
  }
  
  virtual
  int nb_strands() {
    return 1;
  }
  
};
  
template <class Function>
class activation_record : public activation_record_template {
public:
  
  context_type context;
  
  Function f;
  
  activation_record(const Function& f)
  : f(f) {
    szb = sizeof(activation_record);
  }
  
  inline
  context_type& get_context() {
    return context;
  }
  
  static inline
  void call_nonlocally();
  
};
  
static constexpr
int stack_szb = 1<<14;
  
class vertex;

vertex* my_vertex();
  
class vertex : public sched::vertex {
public:
  
  stack_type encore_stack;
  
  int fuel;
  
  context_type context;
  
  std::unique_ptr<std::function<void()>> f;
  
  vertex() {
    fuel = 0;
    encore_stack = cactus::new_stack();
  }
  
  ~vertex() {
    cactus::delete_stack(encore_stack);
  }
  
  template <class Function>
  inline
  void call(const Function& callee) {
    fuel--;
    if (! cactus::empty(encore_stack)) {
      auto& newest = cactus::peek_back<activation_record_template>(encore_stack);
      if (! newest.get_context().capture()) {
        // make control flow back to the caller; can be triggered only if
        // the caller is promoted
        return;
      }
    }
    if (fuel == 0 && ! cactus::empty(encore_stack)) {
      vertex* join = this;
      int szb = cactus::peek_front<activation_record_template>(encore_stack).szb;
      auto stacks = cactus::fork_front(encore_stack, szb);
      join->encore_stack = stacks.first;
      vertex* branch = new vertex;
      branch->encore_stack = stacks.second;
      new_edge(branch, join);
      release(branch);
      fuel = sched::D;
      stats::on_promotion();
    }
    using callee_ar_type = activation_record<Function>;
    bool use_same_system_stack = ! cactus::overflow(encore_stack);
    encore_stack = cactus::push_back<callee_ar_type>(encore_stack, callee);
    if (use_same_system_stack) {
      // initiate the call on the same system stack
      callee();
      if (cactus::empty(encore_stack)) {
        // hand control back to the DAG vertex because the caller was promoted
        context.switch_to();
      }
      encore_stack = cactus::pop_back<callee_ar_type>(encore_stack);
    } else {
      // handle stack overflow by activating the callee on a new system stack
      callee_ar_type& newest = cactus::peek_back<callee_ar_type>(encore_stack);
      char* stack = (char*)malloc(stack_szb);
      newest.context.create(stack, stack_szb, (void*)callee_ar_type::call_nonlocally);
      newest.context.switch_to();
    }
  }
  
  static
  void enter() {
    vertex* v = my_vertex();
    assert(v->f);
    auto f = *(v->f);
    v->f.reset();
    v->call(f);
    v->context.switch_to();
  }
  
  int run(int fuel) {
    this->fuel = fuel;
    if (cactus::empty(encore_stack)) {
      if (! f) {
        return this->fuel;
      }
      // create initial call
      context_type tmp;
      char* stack = (char*)malloc(stack_szb);
      tmp.create(stack, stack_szb, (void*)enter);
      context.swap(tmp);
    } else {
      // resume pending call
      auto& newest = cactus::peek_back<activation_record_template>(encore_stack);
      context.swap(newest.get_context());
    }
    return this->fuel;
  }
  
  int nb_strands() {
    if (f) {
      return 1;
    } else if (cactus::empty(encore_stack)) {
      return 0;
    } else {
      return cactus::peek_front<activation_record_template>(encore_stack).nb_strands();
    }
  }
  
  std::pair<sched::vertex*, sched::vertex*> split(int nb) {
    std::pair<sched::vertex*, sched::vertex*> r;
    assert(false); // todo
    return r;
  }
  
};

vertex* my_vertex() {
  return (vertex*)sched::my_vertex();
}
  
template <class Function>
inline
void activation_record<Function>::call_nonlocally() {
  using callee_ar_type = activation_record<Function>;
  vertex* v = my_vertex();
  assert(! cactus::empty(v->encore_stack));
  callee_ar_type& callee_ar = cactus::peek_back<callee_ar_type>(v->encore_stack);
  callee_ar.f();
  if (cactus::empty(v->encore_stack)) {
    v->context.switch_to();
  } else {
    v->encore_stack = cactus::pop_back<callee_ar_type>(v->encore_stack);
    auto& caller_ar = cactus::peek_back<activation_record_template>(v->encore_stack);
    caller_ar.get_context().switch_to();
  }
}
  
template <class Function>
void call(const Function& f) {
  my_vertex()->call(f);
}
  
} // end namespace
} // end namespace
