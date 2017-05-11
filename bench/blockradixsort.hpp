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
#include <math.h>

#include "sequence.hpp"
#include "transpose.hpp"
#include "utils.h"

#ifndef A_RADIX_INCLUDED
#define A_RADIX_INCLUDED

namespace intSort {
  
  // Cannot be greater than 8 without changing definition of bIndexT
  //    from unsigned char to unsigned int (or unsigned short)
#define MAX_RADIX 8
#define BUCKETS 256    // 1 << MAX_RADIX
  
  //typedef int bucketsT[BUCKETS];
  
  
  // a type that must hold MAX_RADIX bits
  typedef unsigned char bIndexT;
  
  template <class E, class F, class intT>
  void radixBlock(E* A, E* B, bIndexT *Tmp, intT counts[BUCKETS], intT offsets[BUCKETS],
                  intT Boffset, intT n, intT m, F extract) {
    
    for (intT i = 0; i < m; i++)  counts[i] = 0;
    for (intT j = 0; j < n; j++) {
      intT k = Tmp[j] = extract(A[j]);
      counts[k]++;
    }
    intT s = Boffset;
    for (intT i = 0; i < m; i++) {
      s += counts[i];
      offsets[i] = s;
    }
    for (intT j = n-1; j >= 0; j--) {
      intT x =  --offsets[Tmp[j]];
      B[x] = A[j];
    }
  }
  
  template <class E, class F, class intT>
  void radixStepSerial(E* A, E* B, bIndexT *Tmp, intT buckets[BUCKETS],
                       intT n, intT m, F extract) {
    radixBlock(A, B, Tmp, buckets, buckets, (intT)0, n, m, extract);
    for (intT i=0; i < n; i++) A[i] = B[i];
    return;
  }
  
  // A is the input and sorted output (length = n)
  // B is temporary space for copying data (length = n)
  // Tmp is temporary space for extracting the bytes (length = n)
  // BK is an array of bucket sets, each set has BUCKETS integers
  //    it is used for temporary space for bucket counts and offsets
  // numBK is the length of BK (number of sets of buckets)
  // the first entry of BK is also used to return the offset of each bucket
  // m is the number of buckets per set (m <= BUCKETS)
  // extract is a function that extract the appropriate bits from A
  //  it must return a non-negative integer less than m
  template <class E, class F, class intT>
  class radixStep : public encore::edsl::pcfg::shared_activation_record {
  public:
    
    E* A; E* B; bIndexT *Tmp; intT (*BK)[BUCKETS];
    intT numBK; intT n; intT m; bool top; F extract;
    
    int expand; intT blocks; intT nn; intT* cnts; intT* oA; intT* oB; intT ss;
    
    radixStep(E* A, E* B, bIndexT *Tmp, intT (*BK)[BUCKETS],
              intT numBK, intT n, intT m, bool top, F extract)
    : A(A), B(B), Tmp(Tmp), BK(BK), numBK(numBK), n(n),
    m(m), top(top), extract(extract) { }
    
    encore_private_activation_record_begin(encore::edsl, radixStep, 1)
    int s; int e;
    intT od; intT nni;
    encore_private_activation_record_end(encore::edsl, radixStep, sar, par, dc, get_dc)
    
    static
    dc get_dc() {
      return dc::stmts({
        dc::stmt([] (sar& s, par& p) {
          // need 3 bucket sets per block
          s.expand = (sizeof(E)<=4) ? 64 : 32;
          s.blocks = std::min(s.numBK/3,(1+s.n/(BUCKETS*s.expand)));
        }),
        dc::mk_if([] (sar& s, par& p) {
          return s.blocks < 2;
        }, dc::stmts({
          dc::stmt([] (sar& s, par& p) {
            radixStepSerial(s.A, s.B, s.Tmp, s.BK[0], s.n, s.m, s.extract);
          }),
          dc::exit()
        })),
        dc::stmt([] (sar& s, par& p) {
          s.nn = (s.n+s.blocks-1)/s.blocks;
          s.cnts = (intT*) s.BK;
          s.oA = (intT*) (s.BK+s.blocks);
          s.oB = (intT*) (s.BK+2*s.blocks);
          p.s = 0;
          p.e = s.blocks;
        }),
        dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                              [] (par& p) { return std::make_pair(&p.s, &p.e); },
                              dc::stmts({
          dc::stmt([] (sar& s, par& p) {
            intT od = p.s*s.nn;
            intT nni = std::min(std::max<intT>(s.n-od,0),s.nn);
            radixBlock(s.A+od, s.B, s.Tmp+od, s.cnts + s.m*p.s, s.oB + s.m*p.s, od, nni, s.m, s.extract);
            p.s++;
          })
        })),
        dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          return transpose2(st, pt, s.cnts, s.oA, s.blocks, s.m);
        }),
        dc::mk_if([] (sar& s, par& p) {
          return s.top;
        }, dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          return sequence::scan6(st, pt, s.oA, s.oA, s.blocks*s.m, pbbs::utils::addF<intT>(), 0, &s.ss);
        }), dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          return sequence::scanSerial6(st, pt, s.oA, s.oA, s.blocks*s.m, pbbs::utils::addF<intT>(), 0, &s.ss);
        })),
        dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          return blockTrans2(st, pt, s.B, s.A, s.oB, s.oA, s.cnts, s.blocks, s.m);
        }),
        dc::stmt([] (sar& s, par& p) {
          // put the offsets for each bucket in the first bucket set of BK
          for (intT j = 0; j < s.m; j++) s.BK[0][j] = s.oA[j*s.blocks];
        })
      });
    }
    
  };
  
  template <class E, class F, class intT>
  typename radixStep<E,F,intT>::cfg_type radixStep<E,F,intT>::cfg = radixStep<E,F,intT>::get_cfg();
  
  // a function to extract "bits" bits starting at bit location "offset"
  template <class E, class F>
  struct eBits {
    F _f;  intT _mask;  intT _offset;
    eBits(int bits, intT offset, F f): _mask((1<<bits)-1),
    _offset(offset), _f(f) {}
    intT operator() (E p) {return _mask&(_f(p)>>_offset);}
  };

  // Radix sort with low order bits first
  template <class E, class F, class intT>
  class radixLoopBottomUp : public encore::edsl::pcfg::shared_activation_record {
  public:
    
    E *A; E *B; bIndexT *Tmp; intT (*BK)[BUCKETS];
    intT numBK; intT n; int bits; bool top; F f;
    
    int rounds; int rbits; int bitOffset;
    
    radixLoopBottomUp(E *A, E *B, bIndexT *Tmp, intT (*BK)[BUCKETS],
                      intT numBK, intT n, int bits, bool top, F f)
    : A(A), B(B), Tmp(Tmp), BK(BK), numBK(numBK), n(n), bits(bits), top(top), f(f) { }
    
    encore_dc_declare(encore::edsl, radixLoopBottomUp, sar, par, dc, get_dc)

    static
    dc get_dc() {
      return dc::stmts({
        dc::stmt([] (sar& s, par& p) {
          s.rounds = 1+(s.bits-1)/MAX_RADIX;
          s.rbits = 1+(s.bits-1)/s.rounds;
          s.bitOffset = 0;
        }),
        dc::sequential_loop([] (sar& s, par&) { return s.bitOffset < s.bits; }, dc::stmts({
          dc::stmt([] (sar& s, par& p) {
            if (s.bitOffset+s.rbits > s.bits) s.rbits = s.bits-s.bitOffset;
          }),
          dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
            auto f = eBits<E,F>(s.rbits,s.bitOffset,s.f);
            return encore_call<radixStep<E,typeof(f),intT>>(st, pt, s.A, s.B, s.Tmp, s.BK, s.numBK, s.n, (intT)1 << s.rbits, s.top, f);
          }),
          dc::stmt([] (sar& s, par& p) {
            s.bitOffset += s.rbits;
          })
        }))
      });
    }
    
  };
  
  template <class E, class F, class intT>
  typename radixLoopBottomUp<E,F,intT>::cfg_type radixLoopBottomUp<E,F,intT>::cfg = radixLoopBottomUp<E,F,intT>::get_cfg();
  
  // Radix sort with high order bits first
  template <class E, class F, class intT>
  class radixLoopTopDown : public encore::edsl::pcfg::shared_activation_record {
  public:
    
    E *A; E *B; bIndexT *Tmp; intT (*BK)[BUCKETS];
    intT numBK; intT n; int bits;  F f;
    intT* offsets; intT remain; float y;
    
    radixLoopTopDown(E *A, E *B, bIndexT *Tmp, intT (*BK)[BUCKETS],
                      intT numBK, intT n, int bits, F f)
    : A(A), B(B), Tmp(Tmp), BK(BK), numBK(numBK), n(n), bits(bits), f(f) { }
    
    encore_private_activation_record_begin(encore::edsl, radixLoopTopDown, 1)
      int s; int e;
      intT segOffset; intT segNextOffset; intT segLen; intT blocksOffset;
      intT blocksNextOffset; intT blockLen;
    encore_private_activation_record_end(encore::edsl, radixLoopTopDown, sar, par, dc, get_dc)
    
    static
    dc get_dc() {
      return dc::stmts({
        dc::cond({
          std::make_pair([] (sar& s, par& p) {
            return s.n == 0;
          }, dc::exit()),
          std::make_pair([] (sar& s, par& p) {
            return s.bits <= MAX_RADIX;
          }, dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
            auto f = eBits<E,F>(s.bits,0,s.f);
            return encore_call<radixStep<E,typeof(f),intT>>(st, pt, s.A, s.B, s.Tmp, s.BK, s.numBK, s.n, (intT)1 << s.bits, true, f);
          })),
          std::make_pair([] (sar& s, par& p) {
            return s.numBK >= BUCKETS+1;
          }, dc::stmts({
            dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
              auto f = eBits<E,F>(MAX_RADIX,s.bits-MAX_RADIX,s.f);
              return encore_call<radixStep<E,typeof(f),intT>>(st, pt, s.A, s.B, s.Tmp, s.BK, s.numBK, s.n, (intT)BUCKETS, true, f);
            }),
            dc::stmt([] (sar& s, par& p) {
              s.offsets = s.BK[0];
              s.remain = s.numBK - BUCKETS - 1;
              s.y = s.remain / (float) s.n;
              p.s = 0;
              p.e = BUCKETS;
            }),
            dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                                  [] (par& p) { return std::make_pair(&p.s, &p.e); },
                                  dc::stmts({
              dc::stmt([] (sar& s, par& p) {
                p.segOffset = s.offsets[p.s];
                p.segNextOffset = (p.s == BUCKETS-1) ? s.n : s.offsets[p.s+1];
                p.segLen = p.segNextOffset - p.segOffset;
                p.blocksOffset = ((intT) floor(p.segOffset * s.y)) + p.s + 1;
                p.blocksNextOffset = ((intT) floor(p.segNextOffset * s.y)) + p.s + 2;
                p.blockLen = p.blocksNextOffset - p.blocksOffset;
              }),
              dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
                return encore_call<radixLoopTopDown<E,F,intT>>(st, pt, s.A + p.segOffset, s.B + p.segOffset, s.Tmp + p.segOffset,
                                                         s.BK + p.blocksOffset, p.blockLen, p.segLen,
                                                         s.bits-MAX_RADIX, s.f);
              }),
              dc::stmt([] (sar& s, par& p) {
                p.s++;
              })
            }))
          }))
        }, dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          return encore_call<radixLoopBottomUp<E, F, intT>>(st, pt, s.A, s.B, s.Tmp, s.BK, s.numBK, s.n, s.bits, false, s.f);
        }))
      });
    }
    
  };
  
  template <class E, class F, class intT>
  typename radixLoopTopDown<E,F,intT>::cfg_type radixLoopTopDown<E,F,intT>::cfg = radixLoopTopDown<E,F,intT>::get_cfg();
  
  template <class E, class intT>
  long iSortSpace(intT n) {
    typedef intT bucketsT[BUCKETS];
    intT numBK = 1+n/(BUCKETS*8);
    return sizeof(E)*n + sizeof(bIndexT)*n + sizeof(bucketsT)*numBK;
  }

  template <class E, class F, class intT>
  class iSort : public encore::edsl::pcfg::shared_activation_record {
  public:
    
    E *A; intT* bucketOffsets; intT n; intT m; bool bottomUp;
    char* tmpSpace; F f;
    typedef intT bucketsT[BUCKETS];
    
    int bits;
    intT numBK;
    
    // the temporary space is broken into 3 parts: B, Tmp and BK
    E *B; intT Bsize; bIndexT *Tmp; intT tmpSize; bucketsT *BK;
    intT tmp;
   
    iSort(E *A, intT* bucketOffsets, intT n, intT m, bool bottomUp,
          char* tmpSpace, F f)
    : A(A), bucketOffsets(bucketOffsets), n(n), m(m), bottomUp(bottomUp),
    tmpSpace(tmpSpace), f(f) { }
    
    encore_private_activation_record_begin(encore::edsl, iSort, 3)
      int s; int e;
    encore_private_activation_record_end(encore::edsl, iSort, sar, par, dc, get_dc)
    
    static
    dc get_dc() {
      return dc::stmts({
        dc::stmt([] (sar& s, par& p) {
          s.bits = pbbs::utils::log2Up(s.m);
          s.numBK = 1+s.n/(BUCKETS*8);
          // the temporary space is broken into 3 parts: B, Tmp and BK
          s.B = (E*) s.tmpSpace;
          s.Bsize =sizeof(E)*s.n;
          s.Tmp = (bIndexT*) (s.tmpSpace+s.Bsize); // one byte per item
          s.tmpSize = sizeof(bIndexT)*s.n;
          s.BK = (bucketsT*) (s.tmpSpace+s.Bsize+s.tmpSize);
        }),
        dc::cond({
          std::make_pair([] (sar& s, par& p) {
            return s.bits <= MAX_RADIX;
          }, dc::stmts({
            dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
              auto f = eBits<E,F>(s.bits,0,s.f);
              return encore_call<radixStep<E,typeof(f),intT>>(st, pt, s.A, s.B, s.Tmp, s.BK, s.numBK, s.n, (intT) 1 << s.bits, true, f);
            }),
            dc::mk_if([] (sar& s, par& p) {
              return s.bucketOffsets != NULL;
            }, dc::stmts({
              dc::stmt([] (sar& s, par& p) {
                p.s = 0;
                p.e = s.m;
              }),
              dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                                    [] (par& p) { return std::make_pair(&p.s, &p.e); },
                                    dc::stmt([] (sar& s, par& p) { // later: replace by memcpy
                s.bucketOffsets[p.s] = s.BK[0][p.s];
                p.s++;
              }))
            })),
            dc::exit()
          })),
          std::make_pair([] (sar& s, par& p) {
            return s.bottomUp;
          }, dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
            return encore_call<radixLoopBottomUp<E, F, intT>>(st, pt, s.A, s.B, s.Tmp, s.BK, s.numBK, s.n, s.bits, true, s.f);
          }))
        }, dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          return encore_call<radixLoopTopDown<E, F, intT>>(st, pt, s.A, s.B, s.Tmp, s.BK, s.numBK, s.n, s.bits, s.f);
        })),
        dc::mk_if([] (sar& s, par& p) {
          return s.bucketOffsets != NULL;
        }, dc::stmts({
          dc::stmt([] (sar& s, par& p) {
            p.s = 0;
            p.e = s.m;
          }),
          dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                                [] (par& p) { return std::make_pair(&p.s, &p.e); },
                                dc::stmt([] (sar& s, par& p) { // later: replace by memcpy
            s.bucketOffsets[p.s] = s.n;
            p.s++;
          })),
          dc::stmt([] (sar& s, par& p) {
            p.s = 0;
            p.e = s.n-1;
          }),
          dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                                [] (par& p) { return std::make_pair(&p.s, &p.e); },
                                dc::stmt([] (sar& s, par& p) { // later: coarsen
            intT v = s.f(s.A[p.s]);
            intT vn = s.f(s.A[p.s+1]);
            if (v != vn) s.bucketOffsets[vn] = p.s+1;
            p.s++;
          })),
          dc::stmt([] (sar& s, par& p) {
            s.bucketOffsets[s.f(s.A[0])] = 0;
          }),
          dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
            return sequence::scanIBack(st, pt, s.bucketOffsets, s.bucketOffsets, s.m, pbbs::utils::minF<intT>(), s.n, &s.tmp);
          })
        }))
      });
    }
    
  };
  
  template <class E, class F, class intT>
  typename iSort<E,F,intT>::cfg_type iSort<E,F,intT>::cfg = iSort<E,F,intT>::get_cfg();


  template <class E, class F, class intT>
  class iSort6 : public encore::edsl::pcfg::shared_activation_record {
  public:
    
    E *A; intT* bucketOffsets; intT n; intT m; bool bottomUp;
    F f; typedef intT bucketsT[BUCKETS];

    char* ss;
    
    iSort6(E *A, intT* bucketOffsets, intT n, intT m, bool bottomUp, F f)
    : A(A), bucketOffsets(bucketOffsets), n(n), m(m), bottomUp(bottomUp),
    f(f) { }

    encore_dc_declare(encore::edsl, iSort6, sar, par, dc, get_dc)
    
    static
    dc get_dc() {
      return dc::stmts({
	dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
	  long x = iSortSpace<E,intT>(s.n);
	  s.ss = (char*) malloc(x);
	  return encore_call<iSort<E,F,intT>>(st, pt, s.A, s.bucketOffsets, s.n, s.m, s.bottomUp, s.ss, s.f);
	}),
	dc::stmt([] (sar& s, par& p) {
	  free(s.ss);
        })
      });
    }
    
  };
  
  template <class E, class F, class intT>
  typename iSort6<E,F,intT>::cfg_type iSort6<E,F,intT>::cfg = iSort6<E,F,intT>::get_cfg();

  template <class E, class F, class intT>
  stack_type iSort5(stack_type s, plt_type p, E *A, intT* bucketOffsets, intT n, intT m, F f) {
    return sequence::encore_call<iSort6<E,F,intT>>(s, p, A, bucketOffsets, n, m, false, f);
  }

} // end namespace

template <class intT>
class integerSort : public encore::edsl::pcfg::shared_activation_record {
public:

  intT* A; intT n;
  intT maxV;

  integerSort(intT* A, intT n) : A(A), n(n) { }

  encore_dc_declare(encore::edsl, integerSort, sar, par, dc, get_dc)

  static
  dc get_dc() {
    return dc::stmts({
      dc::spawn_join([] (sar& s, par&, plt p, stt st) {
        return sequence::reduce4(st, p, s.A, s.n, pbbs::utils::maxF<uintT>(), &(s.maxV));
      }),
      dc::spawn_join([] (sar& s, par&, plt p, stt st) {
	auto f = pbbs::utils::identityF<uintT>();
	return intSort::iSort5(st, p, s.A, (intT*)NULL, s.n, s.maxV+1, f);
      })
    });
  }

};

template <class intT>
typename integerSort<intT>::cfg_type integerSort<intT>::cfg = integerSort<intT>::get_cfg();

#endif
