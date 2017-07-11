
#include <iostream>
#include <chrono>

#include "merge.hpp"

namespace cmdline = deepsea::cmdline;

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  int n = cmdline::parse<int>("n");
  int* a1 = malloc_array<int>(n);
  int* a2 = malloc_array<int>(n);
  int* a3 = malloc_array<int>(2*n);
  for (int i = 0; i < n; i++) {
    a1[i] = rand() % 1000;
    a2[i] = rand() % 1000;
  }
  auto f = [] (int x, int y) {
    return x < y;
  };
  encore::launch_interpreter<encorebench::merge<int,decltype(f),int>>(a1, n, a2, n, a3, f);
  free(a1);
  free(a2);
  return 0;
}
