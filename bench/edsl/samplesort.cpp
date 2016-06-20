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
#include "sequence.hpp"
#include "samplesort.hpp"

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  transpose<intT, int> t;
  blockTrans<intT, int> t2;
  intT* a;
  intT n;
  auto p = [] (int,int) {
    return true;
  };
  sampleSort<int, typeof(p), int> s(a,n,p);
  return 0;
}