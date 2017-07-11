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

#include "encore.hpp"
#include "logging.hpp"
#include "sequence.hpp"

#ifndef _ENCORE__PBBS_MERGE_H_
#define _ENCORE__PBBS_MERGE_H_

namespace encorebench {

using stack_type = encore::edsl::pcfg::stack_type;

#define _MERGE_BSIZEE (1 << 12)

template <class ET, class F, class intT> 
int binSearch(ET* S, intT n, ET v, F f) {
  ET* T = S;
  while (n > 0) {
    intT mid = n/2;
    if (f(v,T[mid])) n = mid;
    else {
      n = (n-mid)-1;
      T = T + mid + 1;
    }
  }
  return T-S;
}

template <class ET, class F, class intT> 
class merge : public encore::edsl::pcfg::shared_activation_record {
public:

  ET* S1; intT l1; ET* S2; intT l2; ET* R; F f;
  intT m1, m2; bool not_done = true;
  ET* pR; ET* pS1; ET* pS2;

  using trampoline = enum { copy1, copy2, loop };
  trampoline t = copy1;

  merge(ET* S1, intT l1, ET* S2, intT l2, ET* R, F f)
    : S1(S1), l1(l1), S2(S2), l2(l2), R(R), f(f) { }
  
  encore_dc_declare(encore::edsl, merge, sar, par, dc, get_dc)

  static
  dc get_dc() {
    return dc::mk_if([] (sar& s, par& p) {
        return (s.l1 + s.l2) > _MERGE_BSIZEE;
      }, dc::mk_if([] (sar& s, par& p) {
          return s.l2 > s.l1;
        }, dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          return encore_call<merge>(st, pt, s.S2, s.l2, s.S1, s.l1, s.R, s.f);
        }), dc::stmts({
             dc::stmt([] (sar& s, par& p) {
              s.m1 = s.l1 / 2;
              s.m2 = binSearch(s.S2, s.l2, s.S1[s.m1], s.f);
             }),
             dc::spawn2_join(
               [] (sar& s, par&, plt pt, stt st) {
                 return encore_call<merge>(st, pt, s.S1, s.m1, s.S2, s.m2, s.R, s.f);
               },
               [] (sar& s, par&, plt pt, stt st) {
                 return encore_call<merge>(st, pt,
                              s.S1 + s.m1, s.l1 - s.m1, s.S2 + s.m2, s.l2 - s.m2,
                                                      s.R + s.m1 + s.m2, s.f);
             })
      })),
      dc::stmts({
        dc::stmt([] (sar& s, par& p) {
          s.pR = s.R; 
          s.pS1 = s.S1; 
          s.pS2 = s.S2;
        }),
        dc::sequential_loop([] (sar& s, par& p) { return s.not_done; }, dc::stmt([] (sar& s, par& p) {
          ET* pR = s.pR; 
          ET* pS1 = s.pS1; 
          ET* pS2 = s.pS2;
          ET* eS1 = s.S1 + s.l1; 
          ET* eS2 = s.S2 + s.l2;
          auto f = s.f;
          int fuel = 64;
          trampoline t = s.t;
          while (true) {
            switch (t) {
              case copy1: {
                if (pS1==eS1) {
                  auto m = eS2-pS2;
                  auto n = std::min((int)m, fuel);
                  std::copy(pS2,pS2+n,pR);
                  pS2 += n; pR += n; fuel = n;
                  if (m == n) {
                    s.not_done = false;
                    return;
                  } else {
                    goto exit;
                  }
                }
                t = copy2;
              }
              case copy2: {
                if (pS2==eS2) {
                  auto m = eS1-pS1;
                  auto n = std::min((int)m, fuel);
                  std::copy(pS1,pS1+n,pR);
                  pS1 += n; pR += n; fuel = n;
                  if (m == n) {
                    s.not_done = false;
                    return;
                  } else {
                    goto exit;
                  }
                }
                t = loop;
              }
              case loop: {
                *pR++ = f(*pS2,*pS1) ? *pS2++ : *pS1++;
                t = copy1;
                if (--fuel <= 0) {
                  goto exit;
                }
              }
            }
          }
        exit:
          s.pR = pR;
          s.pS1 = pS1;
          s.pS2 = pS2;
          s.t = t;
        }))
      }));
  }
  
};

template <class ET, class F, class intT> 
typename merge<ET,F,intT>::cfg_type merge<ET,F,intT>::cfg = merge<ET,F,intT>::get_cfg();

} // end namespace

#endif
