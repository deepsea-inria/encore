
#include <iostream>

#include "encore.hpp"
#include "native.hpp"

namespace sched = encore::sched;
namespace cmdline = deepsea::cmdline;
namespace par = encore::native;

using namespace encore::native;

__attribute__((noinline, hot, optimize("no-omit-frame-pointer")))
int fib(int n) {
  //  std::cerr << "fib(" << n << ")" << std::endl;
  if (n <= 1) {
    return n;
  }
  int n1, n2;
  par::activation_record arf, ara;
  //std::cerr << "before finish [" << n << "]" << std::endl;  
  par::finish(arf, [&] {
      //std::cerr << "before async1 [" << n << "]" << std::endl;
      /*
    par::async(arf, ara, [&] {
      n1 = fib(n - 1);
      }); */

      {
        promote_if_needed();
        ara.operation.tag = tag_async;
        ara.operation.sync_var.finish_ptr = &(arf.operation);
        my_vertex()->markers.push_back(&ara);
        capture_stack_linkage(ara.continuation.stack);
        membar(capture_return_pointer(ara.continuation));
        if (ara.operation.sync_var.finish_ptr == nullptr) {
          // take this non-local exit because this async call was promoted
          return;
        }
        n1 = fib(n-1);
        if (ara.operation.sync_var.finish_ptr == nullptr) {
          // take this non-local exit because this async call was promoted
          resume_my_vertex();
        } else  {
          my_vertex()->markers.pop_back();
        }

      }
          //std::cerr << "before async2 [" << n << "]" << std::endl;    
    n2 = fib(n - 2);
              //std::cerr << "after async2 [" << n << "]" << std::endl;    
  });
  //std::cerr << "after finish [" << n << "]" << std::endl;    
  return n1 + n2;
}

  int r = 0xdeadbeef;

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("n");

  auto f = [&] {
    r = fib(n);
  };
  auto v = new encore::native::vertex;
  v->mailbox.start_function.reset(new std::function<void()> (f));
  membar(encore::launch(v));
  std::cout << "fib(" << n << ") = " << r << std::endl;
  return 0;
}
