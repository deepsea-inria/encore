#include <assert.h>
#include <deque>
#include <unistd.h>
#include <memory>

#include "vertex.hpp"
#include "stats.hpp"

namespace encore {
namespace native {

class vertex;
vertex* my_vertex();
void resume_my_vertex();
void promote_if_needed();
void promote(vertex* v);

using operation_tag_type = enum {
    tag_async,
    tag_finish,
    tag_future,
    tag_force,
    tag_not_an_operation
};

class operation_descriptor_type {
public:
  
  operation_tag_type tag = tag_not_an_operation;
  
  union {
    // to store either a pointer to the record of the finish block 
    // enclosing an async call, if the finish block was already 
    // promoted, or the null pointer otherwise.
    operation_descriptor_type*    finish_ptr;
    // to store either a pointer to the incounter associated with
    // the continuation of a finish block, if the finish block
    // was already promoted, or the null pointer otherwise
    sched::incounter*  in;
    // to store either a pointer to the outset associated with
    // a parallel future, if the future was already promoted, or
    // the null pointer otherwise
    sched::outset*     out;
    // to store either a pointer to the record of a future that
    // is being targeted by a force operation, if the future has
    // already been promoted, or the null pointer otherwise
    operation_descriptor_type**   future_ptr;
  } sync_var;

    operation_descriptor_type() {
      sync_var.finish_ptr = nullptr;
    }

};

class stack_linkage_type {
public:
  void* btm = nullptr; // frame pointer
  void* top = nullptr; // stack pointer
};

class continuation_type {
public:
  stack_linkage_type stack;
  void* pc = nullptr; // program counter
};

__attribute__((noreturn)) static
void longjmp(continuation_type& c, void* rsp) {
  //  std::cout << "longjmp rbp = " << c.stack.btm << " rsp = " << rsp << " pc = " << c.pc << std::endl;
  assert(c.stack.btm != nullptr && c.pc != nullptr && rsp != nullptr);
  __asm__ ( "mov\t%1,%%rsp\n\t"
	    "mov\t%0,%%rbp\n\t"
	    "jmp\t*%2\n\t"
	    : : "r" (c.stack.btm), "r" (rsp), "r" (c.pc) : "memory");
  __builtin_unreachable();
}

#define membar(call) do { \
    call; \
    __asm__ ( "nop" : : : "rbx", "r12", "r13", "r14", "r15", "memory" ); \
  } while (0)

static
size_t get_page_size() {
  return sysconf(_SC_PAGESIZE);
}
  
static
size_t get_stack_size() {
  size_t stack_size;
#ifdef TARGET_MAC_OS
  stack_size = 1048576; //pthread_get_stacksize_np(pthread_self());
#else
  void* dummy;
  pthread_attr_t attr;
  pthread_getattr_np(pthread_self(), &attr);
  pthread_attr_getstack(&attr, &dummy, size);
#endif
  return stack_size;
}

static
void* new_stack() {
  void* stack = nullptr;
  size_t page_size = get_page_size();
  size_t stack_size = get_stack_size();
  posix_memalign(&stack, page_size, stack_size);
  return stack;
}

static
void* stack_setup(void* stack, size_t stack_size) {
  void ** rsp = (void**)((char*)stack + stack_size);
  rsp -= 16; // reserve 128 bytes at the bottom
  return rsp;
}

void* stack_setup(void* stack) {
  return stack_setup(stack, get_stack_size());
}

class activation_record {
public:

  operation_descriptor_type operation;
  
  continuation_type continuation;

  virtual
  int nb_strands() {
    return 1;
  }

};

__attribute__((always_inline)) extern inline
void capture_stack_linkage(stack_linkage_type& s) {
  register void * rbp asm ("rbp");
  register void * rsp asm ("rsp");
  s.btm = rbp;
  s.top = rsp;
}
  
__attribute__((noinline, optimize("no-omit-frame-pointer")))
void capture_return_pointer(continuation_type& c) {
  c.pc = __builtin_return_address(0);
}

__attribute__((noinline, optimize("no-omit-frame-pointer")))
void capture_return_pointer_and_longjmp(continuation_type& c, void* rsp) {
  c.pc = __builtin_return_address(0);
  longjmp(c, rsp);
}

template <class Function>
__attribute__((optimize("no-omit-frame-pointer")))
void call_using_stack(void* s, continuation_type& c, const Function& f) {
  continuation_type c2;
  capture_stack_linkage(c2.stack);
  membar(capture_return_pointer_and_longjmp(c2, stack_setup(s)));
  f();
  resume_my_vertex();
}

template <class Item>
using channel = std::unique_ptr<Item>;

template <class Item>
Item recv(channel<Item>& c) {
  assert(c);
  Item result = *c;
  c.reset();
  return result;
}

class vertex : public sched::vertex {
public:
  
  int fuel = 0;

  continuation_type trampoline;

  std::deque<activation_record*> markers;

  struct mailbox_struct {
    // to represent a new encore computation to that is to be run by
    // this vertex
    channel<std::function<void()>> start_function;
    // to represent the activation record that is to resume the body of a
    // promoted join point
    channel<activation_record> promoted_join_body;
    // to represent the activation record that is to resume the continuation
    // of a promoted join point
    channel<activation_record> promoted_join_cont;
    // to represent the activation record that is to resume the body of
    // a fork point
    channel<activation_record> promoted_fork_body;
    // to represent the activation record that is to resume the continuation
    // of a fork point
    channel<activation_record> promoted_fork_cont;
    // to represent the first phase of a promotion
    channel<activation_record> promotion;
  } mailbox;

  static
  int nb_messages(const struct mailbox_struct& m) {
    int nb = 0;
    if (m.start_function)     nb++;
    if (m.promoted_join_body) nb++;
    if (m.promoted_join_cont) nb++;
    if (m.promoted_fork_body) nb++;
    if (m.promoted_fork_cont) nb++;
    if (m.promotion)          nb++;
    return nb;
  }

  bool empty_mailbox() const {
    return nb_messages(mailbox) == 0;
  }

  __attribute__((noinline, hot, optimize("no-omit-frame-pointer")))
  int run(int fuel) {
    activation_record a;
    this->fuel = fuel;
    assert(markers.empty());
    capture_stack_linkage(trampoline.stack);
    while (true) {
      assert(! empty_mailbox());
      membar(capture_return_pointer(trampoline));
      if (mailbox.start_function) {
        auto f = recv(mailbox.start_function);
        void* s = new_stack();
        call_using_stack(s, trampoline, f);
      } else if (mailbox.promoted_join_body) {
        a = recv(mailbox.promoted_join_body);
        longjmp(a.continuation, a.continuation.stack.top);
      } else if (mailbox.promoted_join_cont) {
        a = recv(mailbox.promoted_join_cont);
        longjmp(a.continuation, a.continuation.stack.top);
      } else if (mailbox.promoted_fork_body) {
        a = recv(mailbox.promoted_fork_body);
        longjmp(a.continuation, a.continuation.stack.top);
      } else if (mailbox.promoted_fork_cont) {
        a = recv(mailbox.promoted_fork_cont);
        assert(a.continuation.stack.top == nullptr);
        void* s = new_stack();
        longjmp(a.continuation, stack_setup(s));
      } else if (mailbox.promotion) {
        a = recv(mailbox.promotion);
        promote(this);
        longjmp(a.continuation, a.continuation.stack.top);
      } else {
        break;
      }
    }
    assert(markers.empty());
    return this->fuel;
  }

  int nb_strands() {
    if (! empty_mailbox()) {
      return 1;
    } else if (markers.empty()) {
      return 0;
    } else {
      assert(! markers.empty());
      auto a = markers.front();
      return a->nb_strands();
    }
  }

  std::pair<sched::vertex*, sched::vertex*> split(int nb) {
    std::pair<sched::vertex*, sched::vertex*> r;
    assert(false); // todo
    return r;
  }

  template <class Function>
  __attribute__((always_inline))
  inline void async(activation_record& finish, activation_record& a, const Function& f) {
    promote_if_needed();
    a.operation.tag = tag_async;
    a.operation.sync_var.finish_ptr = &(finish.operation);
    markers.push_back(&a);
    membar(capture_return_pointer(a.continuation));
    if (a.operation.sync_var.finish_ptr == nullptr) {
      // take this non-local exit because this async call was promoted
      return;
    }
    f();    
  }

  template <class Function>
  __attribute__((always_inline))
  inline void finish(activation_record& a, const Function& f) {
    promote_if_needed();
    a.operation.tag = tag_finish;
    a.operation.sync_var.in = nullptr;
    capture_stack_linkage(a.continuation.stack);
    membar(capture_return_pointer(a.continuation));
    if (a.operation.sync_var.in != nullptr) {
      // take this non-local exit because this finish block was promoted
      return;
    }
    markers.push_back(&a);
    activation_record root_async;
    async(a, root_async, f);
    if (markers.empty()) {
      // finish block was promoted; hand control back to the vertex
      resume_my_vertex();
    } else {
      assert(markers.back() == &a);
      markers.pop_back();
    }
  }

  void resume() {
    longjmp(trampoline, trampoline.stack.top);
  }

};

void promote(vertex* v) {
  if (v->markers.empty()) {
    return;
  }
  activation_record a = *(v->markers.front());
  v->markers.pop_front();
  auto tag = a.operation.tag;
  if (tag == tag_async) {
    vertex* v2 = new vertex;
    v2->mailbox.promoted_fork_cont.reset(new activation_record(a));
    v->mailbox.promoted_fork_body.swap(v->mailbox.promotion);
    auto finish_ptr = a.operation.sync_var.finish_ptr;
    // to force the associated call to async() method to take non-local exit
    a.operation.sync_var.finish_ptr = nullptr;
    assert(finish_ptr != nullptr);
    assert(finish_ptr->tag == tag_finish);
    assert(finish_ptr->sync_var.in != nullptr);
    sched::incounter* finish_in = finish_ptr->sync_var.in;
    new_edge(v2, finish_in);
    release(v2);
  } else if (tag == tag_finish) {
    vertex* v2 = new vertex;
    v->mailbox.promoted_join_body.swap(v->mailbox.promotion);
    v2->mailbox.promoted_join_cont.reset(new activation_record(a));
    a.operation.sync_var.in = v2->get_incounter();
    new_edge(v, v2);
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

__attribute__((noinline, hot, optimize("no-omit-frame-pointer")))
void promote_if_needed() {
  vertex* v = my_vertex();
  if (--v->fuel == 0) {
    v->fuel = sched::D;
    bool first = true;
    activation_record a;
    capture_stack_linkage(a.continuation.stack);
    membar(capture_return_pointer(a.continuation));
    if (first) {
      first = false;
      v->mailbox.promotion.reset(new activation_record(a));
      v->resume();
    }
  }
}

vertex* my_vertex() {
  return (vertex*)sched::my_vertex();
}

void resume_my_vertex() {
  my_vertex()->resume();
}
  
template <class Function>
__attribute__((always_inline))
inline void async(activation_record& finish, activation_record& a, const Function& f) {
  my_vertex()->async(finish, a, f);
}

template <class Function>
__attribute__((always_inline))
inline void finish(activation_record& a, const Function& f) {
  my_vertex()->finish(a, f);
}

#undef membar

} // end namespace
} // end namespace
