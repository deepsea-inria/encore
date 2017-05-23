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

#ifndef _ENCORE__PBBS_QUICKSORT_H_
#define _ENCORE__PBBS_QUICKSORT_H_

namespace encorebench {

template <class E, class BinPred, class intT>
void insertionSort2(E* A, intT n, BinPred f) {
  for (intT i=0; i < n; i++) {
    E v = A[i];
    E* B = A + i;
    while (--B >= A && f(v,*B)) *(B+1) = *B;
    *(B+1) = v;
  }
}

#define ISORT 25

template <class E, class BinPred>
E median2(E a, E b, E c, BinPred f) {
  return  f(a,b) ? (f(b,c) ? b : (f(a,c) ? c : a))
           : (f(a,c) ? a : (f(b,c) ? c : b));
}

// Quicksort based on median of three elements as pivot
//  and uses insertionSort for small inputs
template<class E, class BinPred, class intT>
class quicksort : public encore::edsl::pcfg::shared_activation_record {
public:

  E* A; intT n; BinPred f;
  E* M; E* L;

  quicksort(E* A, intT n, BinPred f)
    : A(A), n(n), f(f) { }

  encore_dc_declare(encore::edsl, quicksort, sar, par, dc, get_dc)

  static
  dc get_dc() {
    return dc::mk_if([] (sar& s, par&) { return s.n < ISORT; }, dc::stmt([] (sar& s, par& p) {
      insertionSort2(s.A, s.n, s.f);
    }), dc::stmts({dc::stmt([] (sar& s, par&) {
      auto f = s.f;
      auto n = s.n;
      auto A = s.A;
      E p = median2(A[n/4],A[n/2],A[(3*n)/4],f);
      E* L = A;   // below L are less than pivot
      E* M = A;   // between L and M are equal to pivot
      E* R = A+n-1; // above R are greater than pivot
      while (1) {
        while (!f(p,*M)) {
          if (f(*M,p)) std::swap(*M,*(L++));
          if (M >= R) break;
          M++;
        }
        while (f(p,*R)) R--;
        if (M >= R) break;
        std::swap(*M,*R--);
        if (f(*M,p)) std::swap(*M,*(L++));
        M++;
      }
      s.L = L;
      s.M = M;
    }),
    dc::spawn2_join(
       [] (sar& s, par&, plt pt, stt st) {
         return encore_call<quicksort<E,BinPred,intT>>(st, pt, s.A, s.L-s.A, s.f);
    }, [] (sar& s, par&, plt pt, stt st) {
         return encore_call<quicksort<E,BinPred,intT>>(st, pt, s.M, s.A+s.n-s.M, s.f);
    })}));
  }
  
};

template<class E, class BinPred, class intT>
typename quicksort<E,BinPred,intT>::cfg_type quicksort<E,BinPred,intT>::cfg = quicksort<E,BinPred,intT>::get_cfg();
  
} // end namespace

#endif
