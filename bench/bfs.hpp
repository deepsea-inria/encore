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

#include "utils.h"
#include "graph.h"
#include "sequence.hpp"

#ifndef _ENCORE_BFS_H_
#define _ENCORE_BFS_H_

namespace encorebench {

// **************************************************************
//    Non-DETERMINISTIC BREADTH FIRST SEARCH
// **************************************************************

// **************************************************************
//    THE NON-DETERMINISTIC BSF
//    Updates the graph so that it is the BFS tree (i.e. the neighbors
//      in the new graph are the children in the bfs tree)
// **************************************************************

struct nonNegF{bool operator() (intT a) {return (a>=0);}};

class bfs : public encore::edsl::pcfg::shared_activation_record {
public:

  intT start; pbbs::graph::graph<intT> GA; pbbs::graph::vertex<intT>* G;
  std::pair<intT,intT>* result;

  intT numVertices; intT numEdges; intT* Frontier; intT* Visited;
  intT* FrontierNext; intT* Counts; intT totalVisited; intT round;
  intT frontierSize; intT nr;

  bfs(intT start, pbbs::graph::graph<intT> GA, std::pair<intT,intT>* result) :
    start(start), GA(GA), result(result) { }

  encore_private_activation_record_begin(encore::edsl, bfs, 3)
    int s1; int e1;
    int s2; int e2;
    int s3; int e3;
  encore_private_activation_record_end(encore::edsl, bfs, sar, par, dc, get_dc)

  static
  dc get_dc() {
    return dc::stmts({
       dc::stmt([] (sar& s, par& p) {
         s.numVertices = s.GA.n;
         s.numEdges = s.GA.m;
         s.G = s.GA.V;
         s.Frontier = malloc_array<intT>(s.numEdges);
         s.Visited = malloc_array<intT>(s.numVertices);
         s.FrontierNext = malloc_array<intT>(s.numEdges);
         s.Counts = malloc_array<intT>(s.numVertices);
         p.s1 = 0;
         p.e1 = s.numVertices;
       }),
       dc::parallel_for_loop([] (sar&, par& p) { return p.s1 < p.e1; },
                             [] (par& p) { return std::make_pair(&p.s1, &p.e1); },
                             dc::stmt([] (sar& s, par& p) {
         s.Visited[p.s1] = 0;
         p.s1++;
       })),
       dc::stmt([] (sar& s, par& p) {
         s.totalVisited = 0;
         s.round = 0;
       }),
       dc::sequential_loop([] (sar& s, par&) { return s.frontierSize > 0; }, dc::stmts({
         dc::stmt([] (sar& s, par& p) {
           s.round++;
           s.totalVisited += s.frontierSize;
           p.s2 = 0;
           p.e2 = s.frontierSize;
         }),
         dc::parallel_for_loop([] (sar&, par& p) { return p.s2 < p.e2; },
                               [] (par& p) { return std::make_pair(&p.s2, &p.e2); },
                               dc::stmt([] (sar& s, par& p) {
           s.Counts[p.s2] = s.G[s.Frontier[p.s2]].degree;         
           p.s2++;
         })),  
         dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
             return sequence::scan6(st, pt, s.Counts, s.Counts, s.frontierSize, pbbs::utils::addF<intT>(),(intT)0, &(s.nr));
         }),
         dc::stmt([] (sar& s, par& p) {
           p.s3 = 0;
           p.e3 = s.frontierSize;
         }),
         dc::parallel_for_loop([] (sar&, par& p) { return p.s3 < p.e3; },
                               [] (par& p) { return std::make_pair(&p.s3, &p.e3); },
                               dc::stmt([] (sar& s, par& p) {
           intT k = 0;
           intT v = s.Frontier[p.s3];
           intT o = s.Counts[p.s3];
           for (intT j=0; j < s.G[v].degree; j++) {
             intT ngh = s.G[v].Neighbors[j];
             if (s.Visited[ngh] == 0 && !__sync_val_compare_and_swap(&(s.Visited[ngh]), 0, 1)) {
               s.FrontierNext[o+j] = s.G[v].Neighbors[k++] = ngh;
             }
             else s.FrontierNext[o+j] = -1;
           }
           s.G[v].degree = k;
         })),
         dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
           return sequence::filterDPS5(st, pt, s.FrontierNext, s.Frontier, s.nr, nonNegF(), &(s.frontierSize));
         }),
       })),
       dc::stmt([] (sar& s, par& p) {
         *s.result = std::make_pair(s.totalVisited, s.round);
         free(s.FrontierNext); free(s.Frontier); free(s.Counts); free(s.Visited);
       })
     });
   }
  
};

encore_pcfg_allocate(bfs, get_cfg)
  
} // end namespace

#endif
