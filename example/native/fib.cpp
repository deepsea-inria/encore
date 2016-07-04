
#include <iostream>

#include "encore.hpp"
#include "native.hpp"

namespace sched = encore::sched;
namespace cmdline = deepsea::cmdline;
namespace nt = encore::native;

int fib(int n) {
  if (n <= 1) {
    return n;
  }
  int n1, n2;
  nt::activation_record fb, a1;
  nt::initialize_finish(fb);
  nt::initialize_async(a1, fb);
  nt::my_vertex()->finish(fb, [&] {
    nt::my_vertex()->async(a1, [&] {
      n1 = fib(n - 1);
    });
    n2 = fib(n - 2);
  });
  return n1 + n2;
}

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("n");
  int r = -1;
  auto v = new encore::native::vertex;
  v->f.reset(new std::function<void()> ([&] {
    r = fib(n);
  }));
  encore::launch(v);
  std::cout << "fib(" << n << ") = " << r << std::endl;
  return 0;
}
