
#include <iostream>

#include "encore.hpp"
#include "native.hpp"

namespace sched = encore::sched;
namespace cmdline = deepsea::cmdline;
namespace par = encore::native;

int fib(int n) {
  if (n <= 1) {
    return n;
  }
  int n1, n2;
  par::activation_record arf, ara1, ara2;
  par::finish(arf, [&] {
    par::async(arf, ara1, [&] {
      n1 = fib(n - 1);
    });
    par::async(arf, ara2, [&] {
      n2 = fib(n - 2);
    });
  });
  return n1 + n2;
}

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("n");
  auto f = [&] {
    int r;
    r = fib(n);
    std::cout << "fib(" << n << ") = " << r << std::endl;
  };
  auto v = new encore::native::vertex;
  v->mailbox.start_function.reset(new std::function<void()> (f));
  encore::launch(v);
  return 0;
}
