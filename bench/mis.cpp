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
#include "graph.h"

namespace encorebench {

// **************************************************************
//    MAXIMAL INDEPENDENT SET
// **************************************************************

// For each vertex:
//   Flags = 0 indicates undecided
//   Flags = 1 indicates chosen
//   Flags = 2 indicates a neighbor is chosen
struct MISstep {
  char flag;
  char *Flags;  pbbs::graph::vertex<intT>*G;
  MISstep(char* _F, pbbs::graph::vertex<intT>* _G) : Flags(_F), G(_G) {}
  
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
  
  pbbs::graph::graph<intT> GS; char** dest;
  char* Flags; intT tmp;
  
  maximalIndependentSet() { }
    
  maximalIndependentSet(pbbs::graph::graph<intT> GS, char** dest)
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
        return encorebench::speculative_for4(st, pt, mis, 0, s.GS.n, 20, &s.tmp);
      }),
      dc::stmt([] (sar& s, par& p) {
        *s.dest = s.Flags;
      })
    });
  }
  
};

encore_pcfg_allocate(maximalIndependentSet, get_cfg)

} // end namespace

#include "sequence.h"
#include "parallel.h"
#include "speculative_for.h"
#include "gettime.h"
//#include "loaders.hpp"

namespace pbbs {

// For each vertex:
//   Flags = 0 indicates undecided
//   Flags = 1 indicates chosen
//   Flags = 2 indicates a neighbor is chosen
struct MISstep {
  char flag;
  char *Flags;  graph::vertex<intT>*G;
  MISstep(char* _F, graph::vertex<intT>* _G) : Flags(_F), G(_G) {}

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

char* maximalIndependentSet(pbbs::graph::graph<intT> GS) {
  intT n = GS.n;
  graph::vertex<intT>* G = GS.V;
  char* Flags = newArray(n, (char) 0);
  MISstep mis(Flags, G);
  speculative_for(mis,0,n,20);
  return Flags;
}
  
graph::graph<int> to_pbbs(graph::graph<int>& g) {
  graph::vertex<int>* v = (graph::vertex<int>*) malloc(sizeof(graph::vertex<int>) * g.n);
  for (int i = 0; i < g.n; i++) {
    v[i] = graph::vertex<int>(g.V[i].Neighbors, g.V[i].degree);
  }
  return graph::graph<int>(v, g.n, g.m, g.allocatedInplace);
}

template <class Item>
struct read_from_pasl_file_struct {
};

template <class intT>
struct read_from_pasl_file_struct<graph::graph<intT>> {
  template <class VertexType>
  void initialize(std::ifstream& in, int n, int m, intT* e, graph::vertex<intT>* v) {
    std::cerr << n << " " << m << std::endl;
    VertexType* offsets = new VertexType[n + 1];
    in.read(reinterpret_cast<char*>(offsets), sizeof(VertexType) * (n + 1));

    VertexType* eor = new VertexType[m];
    in.read(reinterpret_cast<char*>(eor), sizeof(VertexType) * m);

    for (int i = 0; i < m; i++) {
      e[i] = (intT)eor[i];
    }
    for (int i = 0; i < n; i++) {
      v[i] = graph::vertex<intT>(e + offsets[i], offsets[i + 1] - offsets[i]);
    }
    delete [] eor;
    delete [] offsets;
  }

  graph::graph<intT> operator()(std::ifstream& in) {
    uint64_t header[5];
    in.read(reinterpret_cast<char*>(header), sizeof(header));
    intT n = (intT)header[2];
    intT m = (intT)header[3];

    graph::vertex<intT>* v = (graph::vertex<intT>*)malloc(sizeof(graph::vertex<intT>) * n);
    intT* e = (intT*)malloc(sizeof(intT) * m);

    in.seekg(0, in.end);
    long long contents_size = (long long)in.tellg() - sizeof(header);
    in.seekg(sizeof(header), in.beg);
    long long vertex_type = contents_size / (n + 1 + m);

    if (vertex_type == 8) {
      initialize<long>(in, n, m, e, v);
    } else {
      initialize<int>(in, n, m, e, v);
    }
    return graph::graph<intT>(v, n, m, e);
  }
};

template <class Item>
Item read_from_pasl_file(std::ifstream& in) {
  return read_from_pasl_file_struct<Item>()(in);
}

template <class Item>
Item read_from_pasl_file(std::string file) {
  std::cerr << file << std::endl;
  std::ifstream in(file, std::ifstream::binary);
  return read_from_pasl_file<Item>(in);
}

void benchmark(std::string infile) {
  graph::graph<int> g = read_from_pasl_file<graph::graph<int>>(infile);
  char* flags = nullptr;
  deepsea::cmdline::dispatcher d;
  d.add("encore", [&] {
    encore::launch_interpreter<encorebench::maximalIndependentSet>(g, &flags);
  });
  d.add("pbbs", [&] {
    encore::run_and_report_elapsed_time([&] {
      flags = maximalIndependentSet(g);
    });
  });
  d.dispatch("algorithm"); 
  assert(flags != nullptr);
}

} // end namespace

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  std::string infile = deepsea::cmdline::parse_or_default_string("infile", "");
  if (infile == "") {
    std::cout << "bogus input file name" << std::endl;
    exit(0);
  }
  pbbs::benchmark(infile);
  return 0;
}