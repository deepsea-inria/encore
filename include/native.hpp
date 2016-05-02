
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
  
  void throwto() {
    v = false;
    setcontext(&c);
  }
  
  static
  void swap(context_type& c1, context_type& c2) {
    swapcontext(&c1.c, &c2.c);
  }
  
  static
  void create(context_type& c1, context_type& c2, void* data, void* funptr) {
    size_t stack_szb = SIGSTKSZ;
    char* stack = (char*)cactus::aligned_alloc(16, stack_szb);
    c2.c.uc_link = nullptr;
    c2.c.uc_stack.ss_sp = stack;
    c2.c.uc_stack.ss_size = stack_szb;
    getcontext(&c2.c);
    makecontext(&(c2.c), (void (*)(void))funptr, 1, data);
    swap(c1, c2);
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
        return;
      }
    }
    using ar_type = activation_record<Function>;
    encore_stack = cactus::push_back<ar_type>(encore_stack, callee);
    if (! cactus::is_overflow(encore_stack)) {
      callee();
      encore_stack = cactus::pop_back<ar_type>(encore_stack);
      if (cactus::empty(encore_stack)) {
        context.throwto();
      }
    } else {
      ar_type& newest = cactus::peek_back<ar_type>(encore_stack);
      context_type::create(newest.exit, newest.enter, &newest, (void*)ar_type::call_nonlocally);
      encore_stack = cactus::pop_back<ar_type>(encore_stack);
    }
  }
  
  std::unique_ptr<std::function<void()>> f;
  
  static
  void enter(vertex* v) {
    using ar_type = activation_record<typeof(f)>;
    assert(v->f);
    auto& f = *(v->f);
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
      context_type::swap(context, newest.get_resume());
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

data::perworker::array<vertex*> vertices;
  
vertex* my_vertex() {
  auto v = vertices.mine();
  assert(v != nullptr);
  return v;
}
  
template <class Function>
void activation_record<Function>::call_nonlocally(activation_record* ar) {
  ar->f();
  vertex* v = my_vertex();
  if (cactus::empty(v->encore_stack)) {
    v->context.throwto();
  } else {
    ar->exit.throwto();
  }
}
  
template <class Function>
void call(const Function& f) {
  my_vertex()->call(f);
}
  
#include <deque>
  
void recur(int n) {
  std::cout << "recur(" << n << ")" << std::endl;
  if (n <= 0) {
    return;
  }
  call([=] { recur(n - 1); });
}
  
void scheduler() {
  auto v = new vertex;
  auto f = [&] {
    std::cout << "hi" << std::endl;
    recur(0);
  };
  v->f.reset(new std::function<void()>(f));
  std::deque<vertex*> ready;
  ready.push_back(v);
  while (! ready.empty()) {
    vertex* s = ready.back();
    ready.pop_back();
    vertices.mine() = s;
    s->run(100);
  }
}
  
} // end namespace
} // end namespace
