#include <assert.h>
#include <deque>
#include <unistd.h>
#include <memory>

#include "vertex.hpp"
#include "stats.hpp"

namespace encore {
namespace native {

class vertex;
class continuation_type;
vertex* my_vertex();
void resume_my_vertex();
void promote_if_needed();
void promote(vertex* v, continuation_type&);

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
  pthread_t self;
  pthread_attr_t attr;
  void *stack_addr;
  self = pthread_self();
  pthread_getattr_np(self, &attr);
  pthread_attr_getstack(&attr, &stack_addr, &stack_size);
#endif
  return stack_size;
}

static
void* new_stack() {
  void* stack = nullptr;
  size_t page_size = get_page_size();
  size_t stack_size = get_stack_size();
  posix_memalign(&stack, page_size, stack_size);
  VALGRIND_STACK_REGISTER(stack, stack + stack_size);
  return stack;
}

static
void* stack_setup(void* stack, size_t stack_size) {
  void** rsp = stack + stack_size;
  rsp -= 16; // reserve 128 bytes at the bottom
  return rsp;
}

static
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
void call_using_stack(void* s, const Function& f) {
  __asm__ ( "mov\t%0,%%rsp\n\t"
            : : "r" (stack_setup(s)) : "memory");
  f();
  resume_my_vertex();
}

using mailbox_message_tag = enum {
  tag_start_function,
  tag_promoted_join_body,
  tag_promoted_join_cont,
  tag_promoted_fork_body,
  tag_promoted_fork_cont,
  tag_promotion,
  tag_not_a_message
};

class mailbox_type {
public:

  mailbox_message_tag tag = tag_not_a_message;
  // to represent a new encore computation to that is to be run by
  // this vertex
  std::function<void()> start_function;
  // to represent the activation record that is to resume the body of a
  // promoted join point
  continuation_type promoted_join_body;
  // to represent the activation record that is to resume the continuation
  // of a promoted join point
  continuation_type promoted_join_cont;
  // to represent the activation record that is to resume the body of
  // a fork point
  continuation_type promoted_fork_body;
  // to represent the activation record that is to resume the continuation
  // of a fork point
  continuation_type promoted_fork_cont;
  // to represent the first phase of a promotion
  continuation_type promotion;

  bool empty() const {
    return tag == tag_not_a_message;
  }

  template <class Item>
  void send(mailbox_message_tag t, Item& src, Item& dst) {
    assert(empty());
    tag = t;
    dst = src;
  }

  template <class Item>
  Item recv(mailbox_message_tag t, Item& x) {
    assert(tag == t);
    tag = tag_not_a_message;
    return x;
  }

};

class vertex : public sched::vertex {
public:
  
  int fuel = 0;

  continuation_type trampoline;

  std::deque<activation_record*> markers;
  
  mailbox_type mailbox;

  __attribute__((noinline, hot, optimize("no-omit-frame-pointer")))
  int run(int fuel) {
    this->fuel = fuel;
    capture_stack_linkage(trampoline.stack);
    membar(capture_return_pointer(trampoline));
    if (mailbox.tag == tag_start_function) {
      auto f = mailbox.recv(tag_start_function, mailbox.start_function);
      void* s = new_stack();
      call_using_stack(s, f);
    } else if (mailbox.tag == tag_promoted_join_body) {
      auto c = mailbox.recv(tag_promoted_join_body, mailbox.promoted_join_body);
      longjmp(c, c.stack.top);
    } else if (mailbox.tag == tag_promoted_join_cont) {
      auto c = mailbox.recv(tag_promoted_join_cont, mailbox.promoted_join_cont);;
      longjmp(c, c.stack.top);
    } else if (mailbox.tag == tag_promoted_fork_body) {
      auto c = mailbox.recv(tag_promoted_fork_body, mailbox.promoted_fork_body);
      longjmp(c, c.stack.top);
    } else if (mailbox.tag == tag_promoted_fork_cont) {
      auto c = mailbox.recv(tag_promoted_fork_cont, mailbox.promoted_fork_cont);
      void* s = new_stack();
      longjmp(c, stack_setup(s));
    } else if (mailbox.tag == tag_promotion) {
      auto c = mailbox.recv(tag_promotion, mailbox.promotion);
      promote(this, c);
    }
    return this->fuel;
  }

  int nb_strands() {
    if (! mailbox.empty()) {
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
    my_vertex()->markers.push_back(&a);
    capture_stack_linkage(a.continuation.stack);
    membar(capture_return_pointer(a.continuation));
    if (a.operation.sync_var.finish_ptr == nullptr) {
      // take this non-local exit because this async call was promoted
      return;
    }
    f();
    if (a.operation.sync_var.finish_ptr == nullptr) {
      // take this non-local exit because this async call was promoted
      resume_my_vertex();
    } else {
      my_vertex()->markers.pop_back();
    }
  }

  template <class Function>
  __attribute__((always_inline))
  inline void finish(activation_record& a, const Function& f) {
    a.operation.tag = tag_finish;
    a.operation.sync_var.in = nullptr;
    capture_stack_linkage(a.continuation.stack);
    membar(capture_return_pointer(a.continuation));
    if (a.operation.sync_var.in != nullptr) {
      // take this non-local exit because this finish block was promoted
      return;
    }
    my_vertex()->markers.push_back(&a);
    f();
    if (a.operation.sync_var.in != nullptr) {
      //assert(my_vertex()->markers.empty());
      // finish block was promoted; hand control back to the vertex
      resume_my_vertex();
    } else {
      //assert(a.operation.sync_var.in == nullptr);
      //assert(! my_vertex()->markers.empty());
      //assert(my_vertex()->markers.back() == &a);
      my_vertex()->markers.pop_back();
    }
  }

  __attribute__((always_inline))
  void resume() {
    longjmp(trampoline, trampoline.stack.top);
  }

};

void promote(vertex* v, continuation_type& b) {
  assert(! v->markers.empty());
  activation_record& a = *(v->markers.front());
  v->markers.pop_front();
  auto tag = a.operation.tag;
  if (tag == tag_async) {
    // a == continuation of the async call to be promoted;
    // b == whatever piece of the body of the async call which triggered the promotion
    vertex* v2 = new vertex;
    auto finish_ptr = a.operation.sync_var.finish_ptr;
    // to force the associated call to async() method to take non-local exit
    a.operation.sync_var.finish_ptr = nullptr;
    v->mailbox.send(tag_promoted_fork_body, b, v->mailbox.promoted_fork_body);
    v2->mailbox.send(tag_promoted_fork_cont, a.continuation, v2->mailbox.promoted_fork_cont);
    assert(finish_ptr != nullptr);
    assert(finish_ptr->tag == tag_finish);
    assert(finish_ptr->sync_var.in != nullptr);
    sched::incounter* finish_in = finish_ptr->sync_var.in;
    new_edge(v2, finish_in);
    release(v2);
    schedule(v);
  } else if (tag == tag_finish) {
    // a == continuation of finish block;
    // b == whatever piece of the body of the finish block which triggered the promotion
    vertex* v2 = new vertex;
    a.operation.sync_var.in = v->get_incounter();
    v->mailbox.send(tag_promoted_join_cont, a.continuation, v->mailbox.promoted_join_cont);
    v2->mailbox.send(tag_promoted_join_body, b, v2->mailbox.promoted_join_body);
    std::swap(v->fuel, v2->fuel);
    v->markers.swap(v2->markers);
    new_edge(v2, v);
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
  if (! my_vertex()->markers.empty() && --my_vertex()->fuel == 0) {
    my_vertex()->fuel = sched::D;
    bool first = true;
    continuation_type c;
    capture_stack_linkage(c.stack);
    membar(capture_return_pointer(c));
    if (first) {
      first = false;
      my_vertex()->mailbox.send(tag_promotion, c, my_vertex()->mailbox.promotion);
      resume_my_vertex();
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

  //#undef membar

} // end namespace
} // end namespace
