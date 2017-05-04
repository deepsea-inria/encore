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

#ifndef A_TRANSPOSE_INCLUDED
#define A_TRANSPOSE_INCLUDED

#define _TRANS_THRESHHOLD 64

template <class E, class intT>
class transpose : public encore::edsl::pcfg::shared_activation_record {
public:
  
  E *A, *B;
  intT rStart; intT rCount; intT rLength;
  intT cStart; intT cCount; intT cLength;
  intT l1; intT l2;
  
  transpose(E *AA, E *BB,
            intT rStart, intT rCount, intT rLength,
            intT cStart, intT cCount, intT cLength)
  : A(AA), B(BB),
  rStart(rStart), rCount(rCount), rLength(rLength),
  cStart(cStart), cCount(cCount), cLength(cLength) {}
  
  encore_dc_declare(encore::edsl, transpose, sar, par, dc, get_dc)

  static
  dc get_dc() {
    return dc::cond({
      std::make_pair([] (sar& s, par& p) {
        return s.cCount < _TRANS_THRESHHOLD && s.rCount < _TRANS_THRESHHOLD;
      }, dc::stmt([] (sar& s, par& p) { // later: consider encoding as encore loops?
        auto rStart = s.rStart; auto rCount = s.rCount;
        auto cStart = s.cStart; auto cCount = s.cCount;
        auto B = s.B; auto A = s.A;
        auto rLength = s.rLength; auto cLength = s.cLength;
        for (intT i=rStart; i < rStart + rCount; i++)
          for (intT j=cStart; j < cStart + cCount; j++)
            B[j*cLength + i] = A[i*rLength + j];
      })),
      std::make_pair([] (sar& s, par& p) {
        return s.cCount > s.rCount;
      }, dc::stmts({
        dc::stmt([] (sar& s, par& p) {
          s.l1 = s.rCount/2;
          s.l2 = s.rCount - s.rCount/2;
        }),
        dc::spawn2_join(
          [] (sar& s, par&, plt pt, stt st) {
            return encore_call<transpose<E,intT>>(st, pt, s.A, s.B, s.rStart, s.l1, s.rLength, s.cStart, s.cCount, s.cLength);
          },
          [] (sar& s, par&, plt pt, stt st) {
            return encore_call<transpose<E,intT>>(st, pt, s.A, s.B, s.rStart, s.rCount, s.rLength, s.cStart + s.l1, s.l2, s.cLength);
          })
      }))
    }, dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        s.l1 = s.rCount/2;
        s.l2 = s.rCount - s.rCount/2;
      }),
      dc::spawn2_join(
        [] (sar& s, par&, plt pt, stt st) {
          return encore_call<transpose<E,intT>>(st, pt, s.A, s.B, s.rStart, s.l1, s.rLength, s.cStart, s.cCount, s.cLength);
        },
        [] (sar& s, par&, plt pt, stt st) {
          return encore_call<transpose<E,intT>>(st, pt, s.A, s.B, s.rStart + s.l1, s.l2, s.rLength, s.cStart, s.cCount, s.cLength);
        })
    }));
  }
  
};

template <class E, class intT>
typename transpose<E,intT>::cfg_type transpose<E,intT>::cfg = transpose<E,intT>::get_cfg();

using stack_type = encore::edsl::pcfg::stack_type;

template <class E, class intT>
stack_type transpose2(stack_type st, plt_type pt, E *AA, E *BB, intT rCount, intT cCount) {
  return sequence::encore_call<transpose<E,intT>>(st, pt, AA, BB, 0,rCount,cCount,0,cCount,rCount);
}

template <class E, class intT>
class blockTrans : public encore::edsl::pcfg::shared_activation_record {
public:
  
  E *A, *B;
  intT *OA, *OB, *L;
  intT rStart; intT rCount; intT rLength;
  intT cStart; intT cCount; intT cLength;
  intT l1; intT l2;
    
  blockTrans(E *AA, E *BB,
             intT *OOA, intT *OOB, intT *LL,
             intT rStart, intT rCount, intT rLength,
             intT cStart, intT cCount, intT cLength)
  : A(AA), B(BB),
  OA(OOA), OB(OOB), L(LL),
  rStart(rStart), rCount(rCount), rLength(rLength),
  cStart(cStart), cCount(cCount), cLength(cLength) {}
  
  encore_dc_declare(encore::edsl, blockTrans, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::cond({
      std::make_pair([] (sar& s, par& p) {
        return s.cCount < _TRANS_THRESHHOLD && s.rCount < _TRANS_THRESHHOLD;
      }, dc::stmt([] (sar& s, par& p) { // later: consider encoding as encore loops?
        auto rStart = s.rStart; auto rCount = s.rCount;
        auto cStart = s.cStart; auto cCount = s.cCount;
        auto A = s.A; auto B = s.B;
        auto OA = s.OA; auto OB = s.OB; auto L = s.L;
        auto rLength = s.rLength; auto cLength = s.cLength;
        for (intT i=rStart; i < rStart + rCount; i++)
          for (intT j=cStart; j < cStart + cCount; j++) {
            E* pa = A+OA[i*rLength + j];
            E* pb = B+OB[j*cLength + i];
            intT l = L[i*rLength + j];
            //cout << "pa,pb,l: " << pa << "," << pb << "," << l << endl;
            for (intT k=0; k < l; k++) *(pb++) = *(pa++);
          }
      })),
      std::make_pair([] (sar& s, par& p) {
        return s.cCount > s.rCount;
      }, dc::stmts({
        dc::stmt([] (sar& s, par& p) {
          s.l1 = s.cCount/2;
          s.l2 = s.cCount - s.cCount/2;
        }),
        dc::spawn2_join(
          [] (sar& s, par&, plt_type pt, stt st) {
            return encore_call<blockTrans<E,intT>>(st, pt, s.A, s.B, s.OA, s.OB, s.L,
                                             s.rStart, s.rCount, s.rLength, s.cStart, s.l1, s.cLength);
          },
          [] (sar& s, par&, plt_type pt, stt st) {
            return encore_call<blockTrans<E,intT>>(st, pt, s.A, s.B, s.OA, s.OB, s.L,
                                             s.rStart, s.rCount, s.rLength, s.cStart + s.l1, s.l2, s.cLength);
          })
      }))
    }, dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        s.l1 = s.rCount/2;
        s.l2 = s.rCount - s.rCount/2;
      }),
      dc::spawn2_join(
        [] (sar& s, par&, plt_type pt, stt st) {
          return encore_call<blockTrans<E,intT>>(st, pt, s.A, s.B, s.OA, s.OB, s.L,
                                           s.rStart, s.l1, s.rLength, s.cStart, s.cCount, s.cLength);
        },
        [] (sar& s, par&, plt_type pt, stt st) {
          return encore_call<blockTrans<E,intT>>(st, pt, s.A, s.B, s.OA, s.OB, s.L,
                                           s.rStart + s.l1, s.l2, s.rLength, s.cStart, s.cCount, s.cLength);
        })
    }));
  }
  
};

template <class E, class intT>
typename blockTrans<E,intT>::cfg_type blockTrans<E,intT>::cfg = blockTrans<E,intT>::get_cfg();

template <class E, class intT>
stack_type blockTrans2(stack_type st, plt_type pt, E *AA, E *BB,
                       intT *OOA, intT *OOB, intT *LL,
                       intT rCount, intT cCount) {
  return sequence::encore_call<blockTrans<E,intT>>(st, pt, AA, BB, OOA, OOB, LL, 0,rCount,cCount,0,cCount,rCount);
}

#endif

