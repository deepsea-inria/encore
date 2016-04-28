
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
    assert(! v);
    v = true;
    getcontext(&c);
    return v;
  }
  
  void throwto() {
    assert(v);
    v = false;
    setcontext(&c);
  }
  
  static
  void swap(context_type& c1, context_type& c2) {
    swapcontext(&c1.c, &c2.c);
  }
  
  static
  void create(context_type& enter, context_type& exit, void* data, void* funptr) {
    char* stack = (char*)malloc(SIGSTKSZ);
    enter.c.uc_link = nullptr;
    enter.c.uc_stack.ss_sp = stack;
    enter.c.uc_stack.ss_size = SIGSTKSZ;
    makecontext(&enter.c, (void (*)(void))funptr, 1, data);
    swap(exit, enter);
  }
  
};
  
data::perworker::array<context_type> schedulers;
  
context_type dummy;
  
class activation_record_template {
public:
  
  virtual context_type& get_resume() {
    assert(false);
    return dummy;
  }
  
};
  
template <class Function>
class activation_record {
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
  
  vertex() {
    fuel = 0;
    encore_stack = cactus::new_stack();
  }
  
  template <class Function>
  void call(const Function& callee) {
    if (--fuel == 0) {
      // todo: promote here
    }
    if (! cactus::empty(encore_stack)) {
      auto& newest = cactus::peek_back<activation_record_template>(encore_stack);
      auto& gr = newest.get_resume();
      auto b = gr.capture();
      if (b) {
        return;
      }
    }
    using ar_type = activation_record<Function>;
    encore_stack = cactus::push_back<ar_type>(encore_stack, callee);
    ar_type& newest = cactus::peek_back<ar_type>(encore_stack);
    if (! cactus::is_overflow(encore_stack)) {
      callee();
      encore_stack = cactus::pop_back<ar_type>(encore_stack);
      if (cactus::empty(encore_stack)) {
        schedulers.mine().throwto();
      }
    } else {
      context_type::create(newest.enter, newest.exit, &newest, (void*)ar_type::call_nonlocally);
      encore_stack = cactus::pop_back<ar_type>(encore_stack);
    }
  }
  
  std::unique_ptr<std::function<void()>> f;
  
  static
  void enter(vertex* v) {
    using ar_type = activation_record<typeof(f)>;
    assert(v->f);
    auto& f = *(v->f);
    v->call(f);
  }
  
  int run(int fuel) {
    this->fuel = fuel;
    if (cactus::empty(encore_stack)) {
      context_type tmp;
      context_type::create(schedulers.mine(), tmp, this, (void*)enter);
    } else {
      auto& newest = cactus::peek_back<activation_record_template>(encore_stack);
      context_type::swap(schedulers.mine(), newest.get_resume());
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
  if (cactus::empty(my_vertex()->encore_stack)) {
    schedulers.mine().throwto();
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
  call([&] { recur(n - 1); });
}
  
void scheduler() {
  auto v = new vertex;
  auto f = [&] {
    std::cout << "hi" << std::endl;
    recur(5);
  };
  v->f.reset(new std::function<void()>(f));
  std::deque<vertex*> ready;
  ready.push_back(v);
  assert(schedulers.mine().capture());
  while (! ready.empty()) {
    vertex* s = ready.back();
    vertices.mine() = s;
    ready.pop_back();
    s->run(100);
  }
}
  
} // end namespace
} // end namespace