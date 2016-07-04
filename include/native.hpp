#include <assert.h>
#include <deque>
#include <unistd.h>

#include "vertex.hpp"
#include "stats.hpp"

namespace encore {
namespace native {

using operation_tag_type = enum {
    tag_async,
    tag_finish,
    tag_future,
    tag_force,
    nb_operations
};

class operation_type {
public:
  operation_tag_type tag;
  union {
    operation_type*    finish_ptr;
    sched::incounter*  in;
    sched::outset*     out;
    operation_type**   future_ptr;
  } sync_var;
};

using stack_type = struct {
  void* btm; // frame pointer
  void* top; // stack pointer
  void* ptr; // pointer to starting address of stack
};

using context_type = struct {
  operation_type operation;
  stack_type stack;
  void* pc; // program counter
};

__attribute__((noreturn)) static
void longjmp(context_type& context, void * rsp) {
  __asm__ ( "mov\t%1,%%rsp\n\t"
	    "mov\t%0,%%rbp\n\t"
	    "jmp\t*%2\n\t"
	    : : "r" (context.stack.btm), "r" (rsp), "r" (context.pc) : "memory");
  __builtin_unreachable();
}

#define membar(call) do { \
    call; \
    __asm__ ( "nop" : : : "rbx", "r12", "r13", "r14", "r15", "memory" ); \
  } while (0)

class activation_record {
public:

  context_type context;

  virtual
  int nb_strands() {
    return 1;
  }

};

__attribute__((always_inline)) extern inline
void initialize_stack(stack_type& s) {
  register void * rbp asm ("rbp");
  register void * rsp asm ("rsp");
  s.btm = rbp;
  s.top = rsp;
}

__attribute__((always_inline)) extern inline
void initialize_context(context_type& c) {
  initialize_stack(c.stack);
  c.pc = nullptr;
  c.operation.tag = nb_operations;
  c.operation.sync_var.finish_ptr = nullptr; // any field of sync_var should do
}

__attribute__((always_inline)) extern inline
void initialize_async(activation_record& ar, activation_record& finish_ar) {
  initialize_context(ar.context);
  ar.context.operation.tag = tag_async;
  ar.context.operation.sync_var.finish_ptr = &(finish_ar.context.operation);
}

__attribute__((always_inline)) extern inline
void initialize_finish(activation_record& a) {
  initialize_context(a.context);
  a.context.operation.tag = tag_finish;
  a.context.operation.sync_var.in = nullptr;
}

class vertex;

template <class Function>
__attribute__((noinline, hot, optimize(3)))      
void fork(vertex*, const Function&, activation_record&);

void promote(vertex*);

class vertex : public sched::vertex {
public:

  int fuel = 0;

  context_type context;

  std::deque<activation_record*> frames;

  std::unique_ptr<std::function<void()>> f;

  static __attribute__((noinline, hot, optimize(3)))
  void doit(vertex* v, const std::function<void()>& f) {
    v->context.pc = __builtin_return_address(0);
    f();
    v->resume();
  }

  int run(int fuel) {
    this->fuel = fuel;
    initialize_context(context);
    if (f) {
      auto ff = *f;
      f.reset();
      doit(this, ff);
    } else if (! frames.empty()) {
      assert(frames.size() == 1);
      auto ar = frames.back();
      frames.pop_back();
      longjmp(ar->context, ar->context.stack.top);
    }
    std::cout << "done = " << frames.size() << std::endl;
    return this->fuel;
  }

  int nb_strands() {
    if (f) {
      return 1;
    } else if (frames.empty()) {
      return 0;
    } else {
      assert(! frames.empty());
      auto ar = frames.front();
          std::cout << "nb_strands ar = " << &ar << std::endl;
      return ar->nb_strands();
    }
  }

  std::pair<sched::vertex*, sched::vertex*> split(int nb) {
    std::pair<sched::vertex*, sched::vertex*> r;
    assert(false); // todo
    return r;
  }

  template <class Function>
  __attribute__((always_inline))
  inline void async(activation_record& ar, const Function& f) {
    assert(ar.context.operation.tag == tag_async);
    assert(ar.context.operation.sync_var.finish_ptr != nullptr);
    membar(fork(this, f, ar));
  }

  template <class Function>
  __attribute__((always_inline))
  inline void finish(activation_record& ar, const Function& f) {
    assert(ar.context.operation.tag == tag_finish);
    frames.push_back(&ar);
    std::cout << "finish ar = " << &ar << std::endl;
    activation_record async_ar;
    initialize_async(async_ar, ar);
    async(async_ar, f);
    if (frames.empty()) { // finish block was promoted
      resume();
    }
  }

  void resume() {
    longjmp(context, context.stack.top);
  }

};

static size_t get_page_size()
{
  int pagesize = sysconf(_SC_PAGESIZE);
  return pagesize;
}

static void get_stack_size(void ** addr, size_t * size)
{
  pthread_attr_t attr;

  pthread_getattr_np(pthread_self(), &attr);
  pthread_attr_getstack(&attr, addr, size);
}

void * stack_setup(void* stack, size_t stack_size) {
  void ** rsp = stack + stack_size;

  /** Reserve 128 byte at the bottom. */
  rsp -= 16;
  return rsp;
}

void promote(vertex* v) {
  if (v->frames.empty()) {
    return;
  }
  activation_record& ar = *(v->frames.front());
  v->frames.pop_front();
  auto tag = ar.context.operation.tag;
  if (tag == tag_async) {
    vertex* v2 = new vertex;
    v2->frames.push_back(&ar);
    assert(ar.context.operation.sync_var.finish_ptr != nullptr);
    void* stack = nullptr;
    void* dummy;
    size_t stack_size;
    get_stack_size(&dummy, &stack_size);
    posix_memalign(&stack, get_page_size(), stack_size);
    ar.context.stack.top = stack_setup(stack, stack_size);    
    schedule(v);
    release(v2);
  } else if (tag == tag_finish) {
    vertex* v2 = new vertex;
    v2->frames.push_back(&ar);
    ar.context.operation.sync_var.in = v2->get_incounter();
    new_edge(v, v2);
    schedule(v);
    release(v2);
  } else if (tag == tag_future) {
    assert(false);
  } else if (tag == tag_force) {
    assert(false);
  } else {
    assert(false);
  }
  stats::on_promotion();
}

template <class Function>
__attribute__((noinline, hot, optimize(3)))
void fork(vertex* v, const Function& f, activation_record& ar) {
  if (--v->fuel == 0) {
    promote(v);
    v->fuel = sched::D;
  }
  ar.context.pc = __builtin_return_address(0);
  v->frames.push_back(&ar);
  std::cout << "fork pb = " << &ar << std::endl;
  f();
  if (v->frames.empty()) { // this fork point was promoted
    v->resume();  // hand control back to run method of vertex v
  } else {
    v->frames.pop_back();
    return; // hand control back to caller of f()
  }
}

vertex* my_vertex() {
  return (vertex*)sched::my_vertex();
}

} // end namespace
} // end namespace
