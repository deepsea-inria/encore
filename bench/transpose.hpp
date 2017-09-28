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

#ifndef _ENCORE_A_TRANSPOSE_INCLUDED
#define _ENCORE_A_TRANSPOSE_INCLUDED

#define _TRANS_THRESHHOLD_encore 64

namespace encorebench {

template <class E, class intT>
class transpose : public encore::edsl::pcfg::shared_activation_record {
public:
  
  E *A, *B;
  intT rStart; intT rCount; intT rLength;
  intT cStart; intT cCount; intT cLength;
  intT l1; intT l2;

  using trampoline = enum { loop0, loop1, loop_exit };
  trampoline t; intT i, j;
  
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
        return s.cCount < _TRANS_THRESHHOLD_encore && s.rCount < _TRANS_THRESHHOLD_encore;
      }, dc::stmts({dc::stmt([] (sar& s, par& p) {
          s.i = s.rStart;
          s.t = loop0;
        }),
        dc::sequential_loop([] (sar& s, par& p) { return s.t != loop_exit; }, dc::stmt([] (sar& s, par& p) {
          using controller_type = encore::grain::controller<encore::grain::automatic, transpose>;
          auto lg_lt = controller_type::predict_lg_nb_iterations();
          auto lt = controller_type::predict_nb_iterations(lg_lt);
          int fuel0 = lt;
          int fuel = fuel0;
          auto rStart = s.rStart; auto rCount = s.rCount;
          auto cStart = s.cStart; auto cCount = s.cCount;
          auto B = s.B; auto A = s.A;
          auto rLength = s.rLength; auto cLength = s.cLength;
          auto t = s.t; intT i = s.i; intT j = s.j;
          while (i < rStart + rCount) {
            switch (t) {
              case loop0: {
                j = cStart;
                t = loop1;
              }
              case loop1: {
		auto lst = std::min(cStart + cCount, fuel);
                while (j < lst) {
                  B[j*cLength + i] = A[i*rLength + j];
                  j++;
                }
		fuel = std::max(1, fuel - j);
		if (--fuel == 0) {
		  goto exit;
		}
                i++;
                t = loop0;
                if (--fuel == 0) {
                  goto exit;
                }
              }
            }
          }
          t = loop_exit;
        exit:
          s.i = i; s.j = j; s.t = t;
          controller_type::register_callback(lg_lt, fuel0 - fuel);
          return;
        }))
      })),
      std::make_pair([] (sar& s, par& p) {
        return s.cCount > s.rCount;
      }, dc::stmts({
        dc::stmt([] (sar& s, par& p) {
          s.l1 = s.cCount/2;
          s.l2 = s.cCount - s.cCount/2;
        }),
        dc::spawn2_join(
          [] (sar& s, par&, plt pt, stt st) {
            return encore_call<transpose<E,intT>>(st, pt, s.A, s.B, s.rStart, s.rCount, s.rLength, s.cStart, s.l1, s.cLength);
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

  using trampoline = enum { loop0, loop1, loop2, loop_exit };
  trampoline t; intT i, j, k, l; E* pa; E* pb;
    
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
        return s.cCount < _TRANS_THRESHHOLD_encore && s.rCount < _TRANS_THRESHHOLD_encore;
      }, dc::stmts({dc::stmt([] (sar& s, par& p) {
          s.i = s.rStart;
          s.t = loop0;
        }),
        dc::sequential_loop([] (sar& s, par& p) { return s.t != loop_exit; }, dc::stmt([] (sar& s, par& p) {
          using controller_type = encore::grain::controller<encore::grain::automatic, blockTrans>;
          // later: set_ppt()
          auto lg_lt = controller_type::predict_lg_nb_iterations();
          auto lt = controller_type::predict_nb_iterations(lg_lt);
          int fuel0 = lt;
          int fuel = fuel0;
          auto rStart = s.rStart; auto rCount = s.rCount;
          auto cStart = s.cStart; auto cCount = s.cCount;
          auto A = s.A; auto B = s.B;
          auto OA = s.OA; auto OB = s.OB; auto L = s.L;
          auto rLength = s.rLength; auto cLength = s.cLength;
          auto i = s.i; auto j = s.j; auto k = s.k; auto l = s.l; auto t = s.t;
          auto pa = s.pa; auto pb = s.pb;
          while (i < rStart + rCount) {
            switch (t) {
              case loop0: {
                j = cStart;
                t = loop1;
              }
              case loop1: {
                if (j >= cStart + cCount) {
                  break;
                }
                pa = A+OA[i*rLength + j];
                pb = B+OB[j*cLength + i];
                l = L[i*rLength + j];
                k = 0;
                t = loop2;
              }
              case loop2: {
		auto lst = std::min(l, fuel);
                while (k < lst) {
                  *(pb++) = *(pa++);
                  k++;
                }
		fuel = std::max(1, fuel - k);
		if (--fuel == 0) {
		  goto exit;
		}
                j++;
                t = loop1;
                if (--fuel == 0) {
                  goto exit;
                }
                continue;
              }
            }
            i++;
            t = loop0;
            if (--fuel == 0) {
              goto exit;
            }
          }
          t = loop_exit;
        exit:
          s.i = i; s.j = j; s.t = t; s.k = k; s.l = l; s.pa = pa; s.pb = pb;
          controller_type::register_callback(lg_lt, fuel0 - fuel);
          return;
        }))
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

} // end namespace

#endif

