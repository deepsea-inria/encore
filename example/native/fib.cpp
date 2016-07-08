
#include <iostream>

#include "encore.hpp"
#include "native.hpp"

namespace sched = encore::sched;
namespace cmdline = deepsea::cmdline;
namespace nt = encore::native;

__attribute__((optimize("no-omit-frame-pointer")))
int fib(int n) {

                  register void * rbp2 asm ("rbp");
      register void * rsp2 asm ("rsp");

      std::cout << "fib = " << n << " rbp = " << rbp2 << " rsp = " << rsp2 << std::endl;

  if (n <= 1) {
    return n;
  }
  int n1 = 0; int n2 = 0;
  nt::activation_record fb, a1;
  nt::initialize_finish(fb);
  nt::initialize_async(a1, fb);
  nt::my_vertex()->finish(fb, [&] {

        std::cout << "enter finish v = " << nt::my_vertex() << std::endl;
    nt::my_vertex()->async(a1, [&] {
	      std::cout << "enter b1 v = " << nt::my_vertex() << std::endl;
      n1 = fib(n - 1);
      	      std::cout << "exit b1 v = " << nt::my_vertex() << std::endl;
    });
          	      std::cout << "enter b2 v = " << nt::my_vertex() << std::endl;
    n2 = fib(n - 2);
              	      std::cout << "exit b2 v = " << nt::my_vertex() << std::endl;
  });
  std::cout << "exit finish v = " << nt::my_vertex() << " n = " << n << " res = " << (n1+n2) << " fa = " << __builtin_frame_address(1) << std::endl;
    return n1 + n2;
}

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("n");
  int r = -1;
  auto v = new encore::native::vertex;
  v->f.reset(new std::function<void()> ([&] {
	                  register void * rbp2 asm ("rbp");
      register void * rsp2 asm ("rsp");

      std::cout << "entrypoint = " << n << " rbp = " << rbp2 << " rsp = " << rsp2 << std::endl;

    r = fib(n);

    nt::my_vertex()->resume();
  }));
  encore::launch(v);
  std::cout << "fib(" << n << ") = " << r << std::endl;
  return 0;
}
