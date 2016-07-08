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
  std::cout << "longjmp rbp = " << context.stack.btm << " rsp = " << rsp << " pc = " << context.pc << std::endl;
  assert(context.pc != nullptr);
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

static
size_t get_page_size() {
  int pagesize = sysconf(_SC_PAGESIZE);
  return pagesize;
}

static
void get_stack_size(void ** addr, size_t * size) {
  pthread_attr_t attr;
  pthread_getattr_np(pthread_self(), &attr);
  pthread_attr_getstack(&attr, addr, size);
}

static
void * stack_setup(void* stack, size_t stack_size) {
  void ** rsp = stack + stack_size;
  rsp -= 16; // reserve 128 bytes at the bottom
  return rsp;
}

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
  std::cout << "init with rbp = " << rbp << " rsp = " << rsp << std::endl;
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

  void resume();

    __attribute__((noinline))
  static void doit(context_type& c) {
    c.pc = __builtin_return_address(0);
    longjmp(c, c.stack.top);
  }

      __attribute__((noinline))
      static void doit2(std::function<void()> f, context_type& c) {
    c.pc = __builtin_return_address(0);
                      register void * rbp2 asm ("rbp");
      register void * rsp2 asm ("rsp");

      std::cout << "doit2 =  rbp = " << rbp2 << " rsp = " << rsp2 << std::endl;

    f();
    std::cout << " after fff " << std::endl;
    resume();
  }

    __attribute__((noinline))
  static void doit3(context_type& c) {
    c.pc = __builtin_return_address(0);
  }


  void * get_pc () { return __builtin_return_address(0); }
  
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

  activation_record init_ar;

  void* rrsp;
  __attribute__((noinline, hot, optimize("no-omit-frame-pointer")))
  int run(int fuel) {
    this->fuel = fuel;
    initialize_context(context);
    if (f) {
      std::cout << "entry = " << this << std::endl;
      void* stack = nullptr;
      void* dummy;
      size_t stack_size;
      size_t page_size = get_page_size();
      get_stack_size(&dummy, &stack_size);
      posix_memalign(&stack, page_size, stack_size);
      context.stack.top = stack_setup(stack, stack_size);  
      register void * rbp asm ("rbp");
      register void * rsp asm ("rsp");
      rrsp = rsp;
      membar(doit(context));
      context.stack.top = rrsp;
      std::cout << "middle v = " << this << " f = " << f.get() << std::endl;
      auto ff = *f;
      f.reset();
      membar(doit2(ff, context));
      register void * rbp2 asm ("rbp");
      register void * rsp2 asm ("rsp");
      std::cout << "rsp = " << rsp2 << std::endl;
    } else if (! frames.empty()) {
      assert(frames.size() == 1);
      auto ar = frames.back();
      std::cout << "running from context v = " << this << " ar = " << ar << std::endl;
      doit3(context);
      if (! frames.empty()) { // resume suspended frame
	frames.pop_back();
	assert(ar->context.pc != nullptr);
	/*      void* stack = nullptr;
		void* dummy;
		size_t stack_size;
		size_t page_size = get_page_size();
		get_stack_size(&dummy, &stack_size);
		posix_memalign(&stack, page_size, stack_size); */
	longjmp(ar->context, /* stack_setup(stack, stack_size) */ ar->context.stack.top);
      }
      // resume suspended vertex
    } else {
      assert(false);
    }
    std::cout << "done v = " << this << " sz = " << frames.size() << std::endl;
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
      std::cout << "nb_strands ar = " << &ar << " v = " << this << std::endl;
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
      std::cout << "call finish = " << &ar << std::endl;
          initialize_finish(ar);
      doit3(ar.context);
      if (ar.context.operation.sync_var.in != nullptr) {
	std::cout << "finish nonlocal v= " << sched::my_vertex() << std::endl;
	return;
      }
    frames.push_back(&ar);
    activation_record async_ar;
    initialize_async(async_ar, ar);
    async(async_ar, f);
    if (frames.empty()) { // finish block was promoted
      std::cout << "resume after finish v = " << sched::my_vertex()  << std::endl;
      resume();
    } else {
      assert(frames.back() == &ar);
      frames.pop_back();
    }
  }

  void resume() {
    longjmp(context, context.stack.top);
  }

};

void promote(vertex* v) {
  if (v->frames.empty()) {
    return;
  }
  activation_record& ar = *(v->frames.front());
  v->frames.pop_front();
  auto tag = ar.context.operation.tag;
  std::cout << "prom = " << &ar << std::endl;
  if (tag == tag_async) {
    vertex* v2 = new vertex;
    v2->frames.push_back(&ar);
    assert(ar.context.operation.sync_var.finish_ptr != nullptr);
    new_edge(v, v2);
    release(v2);
    //    schedule(v);
    std::cout << "promote async body = " << v << " join =  "  << v2 << " ar.pc = " << ar.context.pc << std::endl;
  } else if (tag == tag_finish) {
    vertex* v2 = new vertex;
    v2->init_ar = ar;
    v2->frames.push_back(&(v2->init_ar));
    ar.context.operation.sync_var.in = v2->get_incounter();
    new_edge(v, v2);
    //    schedule(v);
    release(v2);
    std::cout << "promote finish body = " << v << " cont =  "  << v2 << " ar = " << &ar << " ar.pc = " << ar.context.pc << std::endl;
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
  std::cout << "push v = " << v << " size = " << v->frames.size() << std::endl;
  std::cout << "fork pb = " << &ar << std::endl;
  f();
  if (v->frames.empty()) { // this fork point was promoted
    std::cout << "resume v == " << v << std::endl;
    v->resume();  // hand control back to run method of vertex v
  } else {
    std::cout << "pop v = " << v <<  " size = " << v->frames.size() << std::endl;
    v->frames.pop_back();
    return; // hand control back to caller of f()
  }
}

vertex* my_vertex() {
  return (vertex*)sched::my_vertex();
}

  void resume() {
    my_vertex()->resume();
  }

} // end namespace
} // end namespace
