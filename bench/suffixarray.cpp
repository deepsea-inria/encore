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

// This is a parallel version of the algorithm described in
//  Juha Karkkainen and Peter Sanders.
//  Simple linear work suffix array construction.
//  Proc. ICALP 2003.  pp 943
// It includes code for finding the LCP
//   Written by Guy Blelloch and Julian Shun

#include <iostream>
#include "sequence.hpp"
#include "blockradixsort.hpp"

namespace encorebench {

using intT = int;

class myRMQ{
public:
  intT* a;
  intT n;
  intT m;
  intT** table;
  intT depth;
};

class precompute_queries : public encore::edsl::pcfg::shared_activation_record {
public:

  myRMQ* rmq;
  intT dist; intT j;
  
  precompute_queries() { }
  
  precompute_queries(myRMQ* rmq)
    : rmq(rmq) { }
  
  encore_private_activation_record_begin(encore::edsl, precompute_queries, 1)
    int lo; int hi;
  encore_private_activation_record_end(encore::edsl, precompute_queries, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        auto r = s.rmq;
        r->depth = std::log2(r->m) + 1;
        r->table = new intT*[r->depth];
      }),
      dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.r->depth; },
                            [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                            [] (sar& s, par& p, int lo, int hi) {
        auto r = s.rmq;
        auto table = r.table;
        auto n = r->n;
        for (auto k = lo; k != hi; k++) {
          table[k] = new intT[n];
        }
      }),
      dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.r->m; },
                            [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                            [] (sar& s, par& p, int lo, int hi) {
        auto r = s.rmq;
        auto table = r.table;
        auto n = r->n;
        auto a = r->a;
        for (auto i = lo; i != hi; i++) {
          intT start = i*BSIZE;
          intT end = min(start+BSIZE,n);
          intT k = i*BSIZE;
          for (intT j = start+1; j < end; j++) {
            if (a[j] < a[k]) {
              k = j;
            }
          }
          table[0][i] = k;
        }
      }),
      dc::stmt([] (sar& s, par& p) {
        s.dist = 1;
        s.j = 1;
      }),
      dc::sequential_loop([] (sar& s, par&) { return s.j < s.r->depth; }, dc::stmts({
        dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.r->m - s.dist; },
                              [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                              [] (sar& s, par& p, int lo, int hi) {
          auto r = s.rmq;
          auto table = r.table;
          auto n = r->n;
          auto a = r->a;
          auto j = s.j;
          auto dist = s.dist;
          for (auto i = lo; i != hi; i++) {
            if (a[table[j-1][i]] <= a[table[j-1][i+dist]]) {
              table[j][i] = table[j-1][i];
            } else {
              table[j][i] = table[j-1][i+dist];
            }
          }
        }),
        dc::parallel_for_loop([] (sar& s, par& p) { p.lo = s.r->m - s.dist; p.hi = s.r->m; },
                              [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                              [] (sar& s, par& p, int lo, int hi) {
          auto r = s.rmq;
          auto table = r.table;
          auto j = s.j;
          for (auto l = lo; l != hi; l++) {
            table[j][l] = table[j-1][l];
          }
        }),
        dc::stmt([] (sar& s, par& p) {
          s.j++;
          dist *= 2;
        })
      }))
    });
  }
  
};

encore_pcfg_allocate(precompute_queries, get_cfg)


  
} // end namespace
