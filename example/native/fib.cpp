
#include <iostream>
#include <valgrind/valgrind.h>

#include "encore.hpp"
#include "native.hpp"

namespace sched = encore::sched;
namespace cmdline = deepsea::cmdline;
namespace par = encore::native;

using namespace encore::native;

#if 0
__attribute__((noinline, hot, optimize("no-omit-frame-pointer")))
int fib(int n) {
  //  std::cout << "fib(" << n << ")" << std::endl;
  if (n <= 1) {
    return n;
  }
  int n1, n2;
  par::activation_record arf, ara;
  //  par::finish(arf, [&] {
      /*
    par::async(arf, ara, [&] {
      n1 = fib(n - 1);
      }); */

  do {
    arf.operation.tag = tag_finish;
    arf.operation.sync_var.in = nullptr;
    capture_stack_linkage(arf.continuation.stack);
    membar(capture_return_pointer(arf.continuation));
    if (arf.operation.sync_var.in != nullptr) {
      // take this non-local exit because this finish block was promoted
      break;
    }
    my_vertex()->markers.push_back(&arf);

    auto foo123 = [&] {
    do {
      promote_if_needed();
      ara.operation.tag = tag_async;
      ara.operation.sync_var.finish_ptr = &(arf.operation);
      my_vertex()->markers.push_back(&ara);
      capture_stack_linkage(ara.continuation.stack);
      membar(capture_return_pointer(ara.continuation));
      if (ara.operation.sync_var.finish_ptr == nullptr) {
        // take this non-local exit because this async call was promoted
        break;
      }
      n1 = fib(n-1);
      if (ara.operation.sync_var.finish_ptr == nullptr) {
        // take this non-local exit because this async call was promoted
        resume_my_vertex();
      } else  {
        my_vertex()->markers.pop_back();
      }
    } while (false);

    n2 = fib(n - 2);
    __asm__ ( "mov\t%0,%%rsp\n\t"
              : : "r" (ara.continuation.stack.top) : "memory");

    };
    foo123();
    
    if (arf.operation.sync_var.in != nullptr) {
      //      assert(arf.operation.sync_var.in != nullptr);
      //      assert(my_vertex()->markers.empty());
      // finish block was promoted; hand control back to the vertex
      resume_my_vertex(); // what happens after this???
    } else {
            assert(! my_vertex()->markers.empty());
      assert(arf.operation.sync_var.in == nullptr);
      assert(my_vertex()->markers.back() == &arf);
      my_vertex()->markers.pop_back();
    }
  } while (false);
  //  });
  return n1 + n2;
}
#else

encore_function
int fib(int n) {
  if (n <= 1) {
    return n;
  }
  int n1, n2;
  par::activation_record arf, ara;
  par::finish(arf, [&] {
    par::async(arf, ara, [&] {
      n1 = fib(n - 1);
    });
    n2 = fib(n - 2);
    __asm__ ( "mov\t%0,%%rsp\n\t"
              : : "r" (ara.continuation.stack.top) : "memory");
  });
  return n1 + n2;
}

#endif

int r = 0xdeadbeef;

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("n");
  auto v = new encore::native::vertex;
  v->mailbox.tag = encore::native::tag_start_function;
  v->mailbox.start_function = [&] {
    r = fib(n);
  };
  membar(encore::launch(v));
  std::cout << "fib(" << n << ") = " << r << std::endl;
  return 0;
}
