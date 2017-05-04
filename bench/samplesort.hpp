// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2010 Guy Blelloch and the PBBS team
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

#include "transpose.hpp"

#ifndef _PBBS_SAMPLESORT_H_
#define _PBBS_SAMPLESORT_H_

template<class E, class BinPred, class intT>
void mergeSeq (E* sA, E* sB, intT* sC, intT lA, intT lB, BinPred f) {
  if (lA==0 || lB==0) return;
  E *eA = sA+lA;
  E *eB = sB+lB;
  for (intT i=0; i <= lB; i++) sC[i] = 0;
  while(1) {
    while (f(*sA, *sB)) {(*sC)++; if (++sA == eA) return;}
    sB++; sC++;
    if (sB == eB) break;
    if (!(f(*(sB-1),*sB))) {
      while (!f(*sB, *sA)) {(*sC)++; if (++sA == eA) return;}
      sB++; sC++;
      if (sB == eB) break;
    }
  }
  *sC = eA-sA;
}

#define SSORT_THR 100000
#define AVG_SEG_SIZE 2
#define PIVOT_QUOT 2

template<class E, class BinPred, class intT>
class sampleSort : public encore::edsl::pcfg::shared_activation_record {
public:
  
  E* A; intT n; BinPred f;
  intT sq; intT rowSize; intT numR; intT numSegs; int overSample = 4;
  intT sampleSetSize; E* sampleSet;
  E* pivots;
  E *B; intT *segSizes; intT *offsetA; intT *offsetB;
    
  sampleSort(E* A, intT n, BinPred f)
  : A(A), n(n), f(f) { }
  
  encore_private_activation_record_begin(encore::edsl, sampleSort, 5)
    intT s; intT e;
    intT offset; intT size; intT tmp;
  encore_private_activation_record_end(encore::edsl, sampleSort, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::mk_if([] (sar& s, par&) { return s.n < SSORT_THR; }, dc::stmts({
        dc::stmt([] (sar& s, par&) {
          std::sort(s.A, s.A + s.n, s.f);
        }),
        dc::exit()
      })),
      dc::stmt([] (sar& s, par& p) {
        s.sq = (intT)(pow(s.n,0.5));
        s.rowSize = s.sq*AVG_SEG_SIZE;
        s.numR = (intT)ceil(((double)s.n)/((double)s.rowSize));
        s.numSegs = (s.sq-1)/PIVOT_QUOT;
        s.sampleSetSize = s.numSegs*s.overSample;
        s.sampleSet = malloc_array<E>(s.sampleSetSize);
        p.s = 0;
        p.e = s.sampleSetSize;
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
        dc::stmt([] (sar& s, par& p) { // todo: coarsen
          intT o = utils::hash(p.s)% s.n;
          s.sampleSet[p.s] = s.A[o];
        p.s++;
        })),
      dc::stmt([] (sar& s, par& p) {
        std::sort(s.sampleSet, s.sampleSet + s.sampleSetSize, s.f);
        s.pivots = malloc_array<E>(s.numSegs - 1);
        p.s = 0;
        p.e = s.numSegs - 1;
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
        dc::stmt([] (sar& s, par& p) { // todo: coarsen
          intT o = s.overSample * p.s;
          s.pivots[p.s] = s.sampleSet[o];
          p.s++;
        })),
      dc::stmt([] (sar& s, par& p) {
        free(s.sampleSet);
        s.B = malloc_array<E>(s.numR * s.rowSize);
        s.segSizes = malloc_array<intT>(s.numR * s.numSegs);
        s.offsetA = malloc_array<intT>(s.numR * s.numSegs);
        s.offsetB = malloc_array<intT>(s.numR * s.numSegs);
        p.s = 0;
        p.e = s.numR;
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
                            dc::stmts({
        dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          p.offset = p.s * s.rowSize;
          p.size =  (p.s < s.numR - 1) ? s.rowSize : s.n - p.offset;
          return encore_call<sampleSort>(st, pt, s.A + p.offset, p.size, s.f);
        }),
        dc::stmt([] (sar& s, par& p) {
          mergeSeq(s.A + p.offset, s.pivots, s.segSizes + p.s*s.numSegs, p.size, s.numSegs-1, s.f);
          p.s++;
        }),
      })),
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        auto plus = [] (intT x, intT y) {
          return x + y;
        };
        return sequence::scan6(st, pt, s.segSizes, s.offsetA, s.numR*s.numSegs, plus, 0, &p.tmp);
      }),
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        return transpose2(st, pt, s.segSizes, s.offsetB, s.numR, s.numSegs);
      }),
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        auto plus = [] (intT x, intT y) {
          return x + y;
        };
        return sequence::scan6(st, pt, s.offsetB, s.offsetB, s.numR*s.numSegs, plus, 0, &p.tmp);
      }),
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        return blockTrans2(st, pt, s.A, s.B, s.offsetA, s.offsetB, s.segSizes, s.numR, s.numSegs);
      }),
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        return encore_call<sequence::copy<E*, E*>>(st, pt, s.B, s.B + s.n, s.A);
      }),
      dc::stmt([] (sar& s, par& p) {
        free(s.B);
        free(s.offsetA);
        free(s.segSizes);
        p.s = 0;
        p.e = s.numSegs;
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
                            dc::stmts({
        dc::stmt([] (sar& s, par& p) {
          p.offset = s.offsetB[p.s * s.numR];
        }),
        dc::cond({
          std::make_pair([] (sar& s, par& p) {
            return (p.s == 0);
          }, dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
            return encore_call<sampleSort<E,BinPred,intT>>(st, pt, s.A, s.offsetB[s.numR], s.f);
          })),
          std::make_pair([] (sar& s, par& p) {
            return (p.s < s.numSegs - 1);
          }, dc::mk_if([] (sar& s, par& p) {
                return (s.f(s.pivots[p.s-1],s.pivots[p.s]));
              }, dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
                return encore_call<sampleSort<E,BinPred,intT>>(st, pt, s.A + p.offset, s.offsetB[(p.s+1)*s.numR] - p.offset, s.f);
              })))
        }, dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          return encore_call<sampleSort<E,BinPred,intT>>(st, pt, s.A + p.offset, s.n - p.offset, s.f);
        })),
        dc::stmt([] (sar& s, par& p) {
          p.s++;
        })
      })),
      dc::stmt([] (sar& s, par& p) {
        free(s.pivots);
        free(s.offsetB);
      })
    });
  }
  
};

template<class E, class BinPred, class intT>
typename sampleSort<E,BinPred,intT>::cfg_type sampleSort<E,BinPred,intT>::cfg = sampleSort<E,BinPred,intT>::get_cfg();

#endif
