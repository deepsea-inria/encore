// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

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
#include "speculative_for.hpp"
#include "graph.hpp"

// **************************************************************
//    MAXIMAL INDEPENDENT SET
// **************************************************************

// For each vertex:
//   Flags = 0 indicates undecided
//   Flags = 1 indicates chosen
//   Flags = 2 indicates a neighbor is chosen
struct MISstep {
  char flag;
  char *Flags;  vertex<intT>*G;
  MISstep(char* _F, vertex<intT>* _G) : Flags(_F), G(_G) {}
  
  bool reserve(intT i) {
    intT d = G[i].degree;
    flag = 1;
    for (intT j = 0; j < d; j++) {
      intT ngh = G[i].Neighbors[j];
      if (ngh < i) {
        if (Flags[ngh] == 1) { flag = 2; return 1;}
        // need to wait for higher priority neighbor to decide
        else if (Flags[ngh] == 0) flag = 0;
      }
    }
    return 1;
  }
  
  bool commit(intT i) { return (Flags[i] = flag) > 0;}
};

class maximalIndependentSet : public encore::edsl::pcfg::shared_activation_record {
public:
  
  graph<intT> GS; char** dest;
  char* Flags; intT tmp;
  
  maximalIndependentSet() { }
    
  maximalIndependentSet(graph<intT> GS, char** dest)
  : GS(GS), dest(dest) { }
  
  encore_dc_declare(encore::edsl, maximalIndependentSet, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        s.Flags = malloc_array<char>(s.GS.n);
        // todo: parallelize this loop
        for (intT i = 0; i < s.GS.n; i++) {
          s.Flags[i] = 0;
        }
      }),
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        MISstep mis(s.Flags, s.GS.V);
        return speculative_for4(st, pt, mis, 0, s.GS.n, 20, &s.tmp);
      }),
      dc::stmt([] (sar& s, par& p) {
        *s.dest = s.Flags;
      })
    });
  }
  
};

encore_pcfg_allocate(maximalIndependentSet, get_cfg)

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  maximalIndependentSet mis;
  return 0;
}
