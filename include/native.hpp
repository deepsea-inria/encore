
#include <ucontext.h>
#include <signal.h>

#include "vertex.hpp"
#include "cactus.hpp"
#include "stats.hpp"

namespace encore {
namespace native {
  
using stack_type = cactus::stack_type;

class context_type {
public:
  
  ucontext_t c;
  
  bool v = false;
  
  bool capture() {
    v = true;
    getcontext(&c);
    return v;
  }
  
  void throw_to() {
    v = false;
    setcontext(&c);
  }
  
  void swap_with(context_type& target) {
    swapcontext(&c, &target.c);
  }
  
  static
  void create(context_type& save, context_type& target, void* data, void* funptr) {
    size_t stack_szb = SIGSTKSZ;
    char* stack = (char*)cactus::aligned_alloc(16, stack_szb);
    target.c.uc_link = nullptr;
    target.c.uc_stack.ss_sp = stack;
    target.c.uc_stack.ss_size = stack_szb;
    getcontext(&target.c);
    makecontext(&(target.c), (void (*)(void))funptr, 1, data);
    save.swap_with(target);
  }
  
};
  
context_type dummy;
  
class activation_record_template {
public:
  
  virtual context_type& get_resume() {
    assert(false);
    return dummy;
  }
  
};
  
template <class Function>
class activation_record : public activation_record_template {
public:
  
  context_type resume;
  
  context_type enter;
  
  context_type exit;
  
  Function f;
  
  activation_record(const Function& f)
  : f(f) { }
  
  context_type& get_resume() {
    return resume;
  }
  
  static
  void call_nonlocally(activation_record* ar);
  
};
  
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
  
  template <class Function>
  inline
  void call(const Function& callee) {
    if (--fuel == 0) {
      // todo: promote here
    }
    if (! cactus::empty(encore_stack)) {
      auto& newest = cactus::peek_back<activation_record_template>(encore_stack);
      if (! newest.get_resume().capture()) {
        // make control flow back to the caller; can be triggered only if
        // the caller is promoted
        return;
      }
    }
    using ar_type = activation_record<Function>;
    encore_stack = cactus::push_back<ar_type>(encore_stack, callee);
    if (! cactus::overflow(encore_stack)) {
      // initiate the call on the same system stack
      callee();
      encore_stack = cactus::pop_back<ar_type>(encore_stack);
      if (cactus::empty(encore_stack)) {
        // hand control back to the DAG vertex because the caller was promoted
        context.throw_to();
      }
    } else {
      // handle stack overflow by activating the callee on a new system stack
      ar_type& newest = cactus::peek_back<ar_type>(encore_stack);
      context_type::create(newest.exit, newest.enter, &newest, (void*)ar_type::call_nonlocally);
      encore_stack = cactus::pop_back<ar_type>(encore_stack);
    }
  }
  
  static
  void enter(vertex* v) {
    using ar_type = activation_record<typeof(f)>;
    assert(v->f);
    auto f = *(v->f);
    v->f.reset();
    v->call(f);
  }
  
  int run(int fuel) {
    this->fuel = fuel;
    if (cactus::empty(encore_stack)) {
      context_type tmp;
      context_type::create(context, tmp, this, (void*)enter);
    } else {
      auto& newest = cactus::peek_back<activation_record_template>(encore_stack);
      context.swap_with(newest.get_resume());
    }
    return this->fuel;
  }
  
  int nb_strands() {
    return 1;
  }
  
  std::pair<sched::vertex*, sched::vertex*> split(int nb) {
    std::pair<sched::vertex*, sched::vertex*> r;
    return r;
  }
  
};

vertex* my_vertex() {
  return (vertex*)sched::my_vertex();
}
  
template <class Function>
void activation_record<Function>::call_nonlocally(activation_record* ar) {
  ar->f();
  vertex* v = my_vertex();
  if (cactus::empty(v->encore_stack)) {
    v->context.throw_to();
  } else {
    ar->exit.throw_to();
  }
}
  
template <class Function>
void call(const Function& f) {
  my_vertex()->call(f);
}
  
} // end namespace
} // end namespace
