#include <iostream>
#include <chrono>
#include <array>
#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>
#include <algorithm>

#include "encore.hpp"
#include "blockradixsort.hpp"

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  auto f = [] (int) {
    return 0;
  };
  intSort::iSort<int, typeof(f), int> is(nullptr, nullptr, 0, 0, false, nullptr, f);
  return 0;
}