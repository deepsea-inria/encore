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

#include <algorithm>

#include "edsl.hpp"
#include "utils.hpp"

#include <limits.h>

#if defined(LONG)
typedef long intT;
typedef unsigned long uintT;
#define INT_T_MAX LONG_MAX
#else
typedef int intT;
typedef int uintT;
#define INT_T_MAX INT_MAX
#endif

#ifndef _ENCORE_PBBS_SEQUENCE_H_
#define _ENCORE_PBBS_SEQUENCE_H_

template <class T>
T* malloc_array(size_t n) {
  return (T*)malloc(n * sizeof(T));
}

template <class T>
struct _seq {
  T* A;
  long n;
  _seq() {A = NULL; n=0;}
  _seq(T* _A, long _n) : A(_A), n(_n) {}
  void del() {free(A);}
};

namespace orig {
  namespace sequence {
    
    template <class intT>
    struct boolGetA {
      bool* A;
      boolGetA(bool* AA) : A(AA) {}
      intT operator() (intT i) {return (intT) A[i];}
    };
    
    template <class ET, class intT>
    struct getA {
      ET* A;
      getA(ET* AA) : A(AA) {}
      ET operator() (intT i) {return A[i];}
    };
    
    template <class IT, class OT, class intT, class F>
    struct getAF {
      IT* A;
      F f;
      getAF(IT* AA, F ff) : A(AA), f(ff) {}
      OT operator () (intT i) {return f(A[i]);}
    };
    
    template <class OT, class intT, class F, class G>
    OT reduceSerial(intT s, intT e, F f, G g) {
      OT r = g(s);
      for (intT j=s+1; j < e; j++) r = f(r,g(j));
      return r;
    }
    
    template <class ET, class intT, class F, class G>
    intT maxIndexSerial(intT s, intT e, F f, G g) {
      ET r = g(s);
      intT k = 0;
      for (intT j=s+1; j < e; j++) {
        ET v = g(j);
        if (f(v,r)) { r = v; k = j;}
      }
      return k;
    }
    
    template <class ET, class intT, class F, class G>
    ET scanSerial(ET* Out, intT s, intT e, F f, G g, ET zero, bool inclusive, bool back) {
      ET r = zero;
      
      if (inclusive) {
        if (back) for (intT i = e-1; i >= s; i--) Out[i] = r = f(r,g(i));
        else for (intT i = s; i < e; i++) Out[i] = r = f(r,g(i));
      } else {
        if (back)
          for (intT i = e-1; i >= s; i--) {
            ET t = g(i);
            Out[i] = r;
            r = f(r,t);
          }
        else
          for (intT i = s; i < e; i++) {
            ET t = g(i);
            Out[i] = r;
            r = f(r,t);
          }
      }
      return r;
    }
    
    template <class ET, class intT, class F>
    ET scanSerial(ET *In, ET* Out, intT n, F f, ET zero) {
      return scanSerial(Out, (intT) 0, n, f, getA<ET,intT>(In), zero, false, false);
    }
    
  }
}

namespace sequence {
 
int threshold = 1024;
int block_size = 2048;
  
template <class intT>
struct boolGetA {
  bool* A;
  boolGetA(bool* AA) : A(AA) {}
  intT operator() (intT i) {return (intT) A[i];}
};

template <class ET, class intT>
struct getA {
  ET* A;
  getA(ET* AA) : A(AA) {}
  ET operator() (intT i) {return A[i];}
};

template <class IT, class OT, class intT, class F>
struct getAF {
  IT* A;
  F f;
  getAF(IT* AA, F ff) : A(AA), f(ff) {}
  OT operator () (intT i) {return f(A[i]);}
};
  
template <class intT>
static inline
int nb_blocks(intT n, intT bsize) {
  return 1 + ((n - 1) / bsize);
}
  
template <class intT>
class range_type {
public:
  intT lo;
  intT hi;
  range_type(intT lo, intT hi)
  : lo(lo), hi(hi) { }
};

template <class intT>
range_type<intT> get_rng(intT block_size, intT item_lo, intT item_hi, intT block_lo, intT block_hi) {
  assert(block_lo <= block_hi);
  assert(item_lo <= item_hi);
  intT block_lo2 = block_lo + (item_lo * block_size);
  intT block_hi2 = std::min(block_lo2 + block_size, std::min(item_hi * block_size, block_hi));
  return range_type<intT>(block_lo2, block_hi2);
}
  
template <class OT, class intT, class F, class G>
class reduceSerial : public encore::edsl::pcfg::shared_activation_record {
public:
  
  intT s; intT e; F f; G g; OT* dest;
  
  reduceSerial() { }
  
  reduceSerial(intT s, intT e, F f, G g, OT* dest)
  : s(s), e(e), f(f), g(g), dest(dest) { }
  
  encore_dc_declare(encore::edsl, reduceSerial, sar, par, dc, get_dc)

  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par&) {
        *s.dest = s.g(s.s);
        s.s++;
      }),
      dc::sequential_loop([] (sar& s, par&) { return s.s < s.e; }, dc::stmt([] (sar& s, par&) {
        intT ss = s.s;
        intT ee = std::min(s.e, ss + threshold);
        *s.dest = s.f(*s.dest, orig::sequence::reduceSerial<OT>(ss, ee, s.f, s.g));
        s.s = ee;
      }))
    });
  }
  
};
  
template <class OT, class intT, class F, class G>
typename reduceSerial<OT,intT,F,G>::cfg_type reduceSerial<OT,intT,F,G>::cfg = reduceSerial<OT,intT,F,G>::get_cfg();
  
#if 1
  
template <class OT, class intT, class F, class G>
class reduce : public encore::edsl::pcfg::shared_activation_record {
public:
  
  intT s; intT e; F f; G g; OT* dest;
  
  reduce() { }
  
  reduce(intT s, intT e, F f, G g, OT* dest)
  : s(s), e(e), f(f), g(g), dest(dest) { }
  
  class private_activation_record
  : public encore::edsl::pcfg::parallel_loop_private_activation_record<reduce,
  private_activation_record> {
  public:
    
    private_activation_record() {
      private_activation_record::initialize_descriptors();
    }
    
    encore::edsl::pcfg::parallel_combine_activation_record _ar;
    encore::edsl::pcfg::parallel_loop_activation_record* _encore_loop_activation_record_of(encore::edsl::pcfg::parallel_loop_id_type id) {
      return &_ar;
    }
    
    int s; int e; OT acc;
    
  };
  
  encore_dc_loop_declare(encore::edsl, reduce, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        p.s = s.s;
        p.e = s.e;
      }),
      dc::parallel_combine_loop([] (sar&, par& p) { return p.s < p.e; },
                                [] (par& p) { return std::make_pair(&p.s, &p.e); },
                                [] (sar&, par& p) { p.acc = OT(); },
                                [] (sar& s, par& p, par& dest) { dest.acc = s.f(p.acc, dest.acc); },
                                dc::stmt([] (sar& s, par& p) {
        intT ss = p.s;
        intT ee = std::min(p.e, ss + threshold);
        p.acc = s.f(p.acc, orig::sequence::reduceSerial<OT>(ss, ee, s.f, s.g));
        p.s = ee;
      })),
      dc::stmt([] (sar& s, par& p) {
        *s.dest = p.acc;
      })
    });
  }
  
};
  
#else

template <class OT, class intT, class F, class G>
class reduce : public encore::edsl::pcfg::shared_activation_record {
public:
  
  intT s; intT e; F f; G g; OT* dest;
  intT l; OT* Sums;

  reduce() { }
  
  reduce(intT s, intT e, F f, G g, OT* dest)
  : s(s), e(e), f(f), g(g), dest(dest) { }
  
  encore_private_activation_record_begin(encore::edsl, reduce, 1)
    int s; int e;
  encore_private_activation_record_end(encore::edsl, reduce, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par&) {
        s.l = nb_blocks(s.e - s.s, block_size);
      }),
      dc::mk_if([] (sar& s, par&) { return s.l <= 1; }, dc::stmts({
        dc::spawn_join([] (sar& s, par&, stt st) {
          return ecall<reduceSerial<OT,intT,F,G>>(st, s.s, s.e, s.f, s.g, s.dest);
        }),
        dc::exit()
      })),
      dc::stmt([] (sar& s, par& p) {
        s.Sums = malloc_array<OT>(s.l);
        p.s = 0;
        p.e = s.l;
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
                            dc::stmts({
        dc::spawn_join([] (sar& s, par& p, stt st) {
          auto rng = get_rng(block_size, p.s, p.e, s.s, s.e);
          return ecall<reduceSerial<OT,intT,F,G>>(st, rng.lo, rng.hi, s.f, s.g, &s.Sums[p.s]);
        }),
        dc::stmt([] (sar& s, par& p) {
          p.s++;
        })
      })),
      dc::spawn_join([] (sar& s, par&, stt st) {
        auto g = getA<OT,intT>(s.Sums);
        return ecall<reduce<OT,intT,F,typeof(g)>>(st, 0, s.l, s.f, g, s.dest);
      }),
      dc::stmt([] (sar& s, par&) {
        free(s.Sums);
      })
    });
  }
  
};
  
#endif
  
template <class OT, class intT, class F, class G>
typename reduce<OT,intT,F,G>::cfg_type reduce<OT,intT,F,G>::cfg = reduce<OT,intT,F,G>::get_cfg();
  
using stack_type = encore::edsl::pcfg::stack_type;

template <class Activation_record, class ...Args>
static stack_type ecall(stack_type s, Args... args) {
  return encore::edsl::pcfg::push_call<Activation_record>(s, args...);
}

template <class OT, class intT, class F>
stack_type reduce4(OT* A, intT n, F f, OT* dest) {
  auto g = getA<OT,intT>(A);
  return ecall<reduce<OT,intT,F,typeof(g)>>((intT)0,n,f,g,dest);
}
  
template <class OT, class intT>
stack_type plusReduce(OT* A, intT n, OT* dest) {
  auto f = utils::addF<OT>();
  auto g = getA<OT,intT>(A);
  return ecall<reduce<OT,intT,typeof(f),typeof(g)>>((intT)0,n,f,g,dest);
}
  
template <class ET, class intT, class F, class G>
class maxIndexSerial : public encore::edsl::pcfg::shared_activation_record {
public:

  intT s; intT e; F f; G g; intT* dest;
  
  maxIndexSerial() { }
  
  maxIndexSerial(intT s, intT e, F f, G g, intT* dest)
  : s(s), e(e), f(f), g(g), dest(dest) { }
  
  encore_dc_declare(encore::edsl, maxIndexSerial, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par&) {
        *s.dest = 0;
      }),
      dc::sequential_loop([] (sar& s, par&) { return s.s < s.e; }, dc::stmt([] (sar& s, par&) {
        intT ss = s.s;
        intT ee = std::min(s.e, ss + threshold);
        intT m = orig::sequence::maxIndexSerial<ET>(ss, ee, s.f, s.g);
        if (s.f(s.g(m), s.g(*s.dest))) {
          *s.dest = m;
        }
        s.s = ee;
      }))
    });
  }
  
};
  
template <class ET, class intT, class F, class G>
typename maxIndexSerial<ET,intT,F,G>::cfg_type maxIndexSerial<ET,intT,F,G>::cfg = maxIndexSerial<ET,intT,F,G>::get_cfg();

#if 1
  
template <class ET, class intT, class F, class G>
class maxIndex : public encore::edsl::pcfg::shared_activation_record {
public:
  
  intT s; intT e; F f; G g; intT* dest;
  
  maxIndex() { }
  
  maxIndex(intT s, intT e, F f, G g, intT* dest)
  : s(s), e(e), f(f), g(g), dest(dest) { }
  
  class private_activation_record
  : public encore::edsl::pcfg::parallel_loop_private_activation_record<maxIndex,
  private_activation_record> {
  public:
    
    private_activation_record() {
      private_activation_record::initialize_descriptors();
    }
    
    encore::edsl::pcfg::parallel_combine_activation_record _ar;
    encore::edsl::pcfg::parallel_loop_activation_record* _encore_loop_activation_record_of(encore::edsl::pcfg::parallel_loop_id_type id) {
      return &_ar;
    }
    
    int s; int e; intT k;
    
  };
  
  encore_dc_loop_declare(encore::edsl, maxIndex, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        p.s = s.s;
        p.e = s.e;
        p.k = 0;
      }),
      dc::parallel_combine_loop([] (sar&, par& p) { return p.s < p.e; },
                                [] (par& p) { return std::make_pair(&p.s, &p.e); },
                                [] (sar&, par& p) { p.k = 0; },
                                [] (sar& s, par& p, par& dest) {
                                  if (s.f(s.g(p.k), s.g(dest.k))) {
                                    dest.k = p.k;
                                  }
                                },
                                dc::stmt([] (sar& s, par& p) {
        intT ss = p.s;
        intT ee = std::min(p.e, ss + threshold);
        int k = orig::sequence::maxIndexSerial<ET>(ss, ee, s.f, s.g);
        if (s.f(s.g(k), s.g(p.k))) {
          p.k = k;
        }
        p.s = ee;
      })),
      dc::stmt([] (sar& s, par& p) {
        *s.dest = p.k;
      })
    });
  }
  
};
  
#else
  
template <class ET, class intT, class F, class G>
class maxIndex : public encore::edsl::pcfg::shared_activation_record {
public:
  
  intT s; intT e; F f; G g; intT* dest;
  intT l; intT* Idx; intT j;
  
  maxIndex() { }
  
  maxIndex(intT s, intT e, F f, G g, intT* dest)
  : s(s), e(e), f(f), g(g), dest(dest) { }
  
  encore_private_activation_record_begin(encore::edsl, maxIndex, 1)
  int s; int e;
  encore_private_activation_record_end(encore::edsl, maxIndex, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par&) {
        s.l = nb_blocks(s.e - s.s, block_size);
      }),
      dc::mk_if([] (sar& s, par&) { return s.l <= 1; }, dc::stmts({
        dc::spawn_join([] (sar& s, par&, stt st) {
          return ecall<maxIndexSerial<ET, intT, F, G>>(st, s.s, s.e, s.f, s.g, s.dest);
        }),
        dc::exit()
      })),
      dc::stmt([] (sar& s, par& p) {
        s.Idx = malloc_array<intT>(s.l);
        p.s = 0;
        p.e = s.l;
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.s != p.e; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
                            dc::stmts({
        dc::spawn_join([] (sar& s, par& p, stt st) {
          auto rng = get_rng(block_size, p.s, p.e, s.s, s.e);
          return ecall<maxIndexSerial<ET, intT, F, G>>(st, rng.lo, rng.hi, s.f, s.g, &s.Idx[p.s]);
        }),
        dc::stmt([] (sar& s, par& p) {
          p.s++;
        })
      })),
      dc::stmt([] (sar& s, par&) {
        *s.dest = s.Idx[0];
        s.j = 1;
      }),
      dc::sequential_loop([] (sar& s, par&) { return s.j < s.l; }, dc::stmt([] (sar& s, par&) {
        if (s.f(s.g(s.Idx[s.j]), s.g(*s.dest))) {
          *s.dest = s.Idx[s.j];
        }
        s.j++;
      })),
      dc::stmt([] (sar& s, par&) {
        free(s.Idx);
      })
    });
  }
  
};
  
#endif
  
template <class ET, class intT, class F, class G>
typename maxIndex<ET,intT,F,G>::cfg_type maxIndex<ET,intT,F,G>::cfg = maxIndex<ET,intT,F,G>::get_cfg();

template <class ET, class intT, class F>
stack_type maxIndex4(stack_type st, ET* A, intT n, F f, intT* dest) {
  auto g = getA<ET,intT>(A);
  return ecall<maxIndex<ET,intT,F,typeof(g)>>(st, (intT)0, n, f, g);
}
  
template <class ET, class intT, class F, class G>
class scanSerial : public encore::edsl::pcfg::shared_activation_record {
public:
  
  ET* Out; intT s; intT e; F f; G g; ET zero; bool inclusive; bool back; ET* dest;
  intT i;
  
  scanSerial() { }
  
  scanSerial(ET* Out, intT s, intT e, F f, G g, ET zero, bool inclusive, bool back, ET* dest)
  : Out(Out), s(s), e(e), f(f), g(g), zero(zero), inclusive(inclusive), back(back), dest(dest) { }
  
  encore_dc_declare(encore::edsl, scanSerial, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par&) {
        *s.dest = s.zero;
      }),
      dc::mk_if([] (sar& s, par&) { return s.inclusive; },
        dc::mk_if([] (sar& s, par&) { return s.back; }, dc::stmts({
            dc::stmt([] (sar& s, par&) {
              s.i = s.e - 1;
            }),
            dc::sequential_loop([] (sar& s, par&) { return s.i >= s.s; }, dc::stmt([] (sar& s, par&) {
              auto f = s.f;
              auto g = s.g;
              intT i = s.i;
              intT ss = std::max(s.s, i - threshold);
              ET r = *s.dest;
              ET* Out = s.Out;
              for (; i >= ss; i--) {
                Out[i] = r = f(r, g(i));
              }
              *s.dest = r;
              s.i = i;
            }))
          }),
          dc::stmts({
            dc::stmt([] (sar& s, par&) {
              s.i = s.s;
            }),
            dc::sequential_loop([] (sar& s, par&) { return s.i < s.e; }, dc::stmt([] (sar& s, par&) {
              auto f = s.f;
              auto g = s.g;
              intT i = s.i;
              intT ee = std::min(s.e, i + threshold);
              ET r = *s.dest;
              ET* Out = s.Out;
              for (; i < ee; i++) {
                Out[i] = r = f(r, g(i));
              }
              *s.dest = r;
              s.i = i;
            }))
          })),
        dc::mk_if([] (sar& s, par&) { return s.back; }, dc::stmts({
          dc::stmt([] (sar& s, par&) {
            s.i = s.e - 1;
          }),
          dc::sequential_loop([] (sar& s, par&) { return s.i >= s.s; }, dc::stmt([] (sar& s, par&) {
            auto f = s.f;
            auto g = s.g;
            intT i = s.i;
            intT ss = std::max(s.s, i - threshold);
            ET r = *s.dest;
            ET* Out = s.Out;
            for (; i >= ss; i--) {
              ET t = g(i);
              Out[i] = r;
              r = f(r, t);
            }
            *s.dest = r;
            s.i = i;
          }))
        }),
        dc::stmts({
          dc::stmt([] (sar& s, par&) {
            s.i = s.s;
          }),
          dc::sequential_loop([] (sar& s, par&) { return s.i < s.e; }, dc::stmt([] (sar& s, par&) {
            auto f = s.f;
            auto g = s.g;
            intT i = s.i;
            intT ee = std::min(s.e, i + threshold);
            ET r = *s.dest;
            ET* Out = s.Out;
            for (; i < ee; i++) {
              ET t = g(i);
              Out[i] = r;
              r = f(r, t);
            }
            *s.dest = r;
            s.i = i;
          }))
        })))
    });
  }
  
};
  
template <class ET, class intT, class F, class G>
typename scanSerial<ET,intT,F,G>::cfg_type scanSerial<ET,intT,F,G>::cfg = scanSerial<ET,intT,F,G>::get_cfg();
  
template <class ET, class intT, class F>
stack_type scanSerial6(stack_type st, ET *In, ET* Out, intT n, F f, ET zero, ET* dest) {
  auto g = getA<ET,intT>(In);
  return ecall<scanSerial<ET,intT,F,typeof(g)>>(st, Out, (intT) 0, n, f, g, zero, false, false, dest);
}

template <class ET, class intT, class F, class G>
class scan : public encore::edsl::pcfg::shared_activation_record {
public:
  
  ET* Out; intT s; intT e; F f; G g; ET zero; bool inclusive; bool back; ET* dest;
  intT i; intT l; ET* Sums;
  
  scan() { }
  
  scan(ET* Out, intT s, intT e, F f, G g, ET zero, bool inclusive, bool back, ET* dest)
  : Out(Out), s(s), e(e), f(f), g(g), zero(zero), inclusive(inclusive), back(back), dest(dest) { }
  
  encore_private_activation_record_begin(encore::edsl, scan, 2)
    int s; int e;
    ET tmp;
  encore_private_activation_record_end(encore::edsl, scan, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par&) {
        s.l = nb_blocks(s.e - s.s, block_size);
      }),
      dc::mk_if([] (sar& s, par&) { return s.l <= 2; }, dc::stmts({
        dc::spawn_join([] (sar& s, par&, stt st) {
          return ecall<scanSerial<ET,intT,F,G>>(st, s.Out, s.s, s.e, s.f, s.g, s.zero, s.inclusive, s.back, s.dest);
        }),
        dc::exit()
      })),
      dc::stmt([] (sar& s, par& p) {
        s.Sums = malloc_array<ET>(s.l);
        p.s = 0;
        p.e = s.l;
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
                            dc::stmts({
        dc::spawn_join([] (sar& s, par& p, stt st) {
          auto rng = get_rng(block_size, p.s, p.e, s.s, s.e);
          return ecall<reduceSerial<ET,intT,F,G>>(st, rng.lo, rng.hi, s.f, s.g, &s.Sums[p.s]);
        }),
        dc::stmt([] (sar& s, par& p) {
          p.s++;
        })
      })),
      dc::spawn_join([] (sar& s, par&, stt st) {
        return ecall<scan>(st, s.Sums, (intT)0, s.l, s.f, getA<ET,intT>(s.Sums), s.zero, false, s.back, s.dest);
      }),
      dc::stmt([] (sar& s, par& p) {
        p.s = 0;
        p.e = s.l;
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
                            dc::stmts({
        dc::spawn_join([] (sar& s, par& p, stt st) {
          auto rng = get_rng(block_size, p.s, p.e, s.s, s.e);
          return ecall<scanSerial<ET,intT,F,G>>(st, s.Out, rng.lo, rng.hi, s.f, s.g, s.Sums[p.s], s.inclusive, s.back, &p.tmp);
        }),
        dc::stmt([] (sar& s, par& p) {
          p.s++;
        })
      })),
      dc::stmt([] (sar& s, par&) {
        free(s.Sums);
      })
    });
  }
  
};

template <class ET, class intT, class F, class G>
typename scan<ET,intT,F,G>::cfg_type scan<ET,intT,F,G>::cfg = scan<ET,intT,F,G>::get_cfg();
  
template <class ET, class intT, class F>
stack_type scan6(stack_type st, ET *In, ET* Out, intT n, F f, ET zero, ET* dest) {
  auto g = getA<ET,intT>(In);
  return ecall<scan<ET,intT,F,typeof(g)>>(st, Out, (intT) 0, n, f, g, zero, false, false, dest);
}

template <class ET, class intT, class F>
stack_type scanBack(stack_type st, ET *In, ET* Out, intT n, F f, ET zero, ET* dest) {
  auto g = getA<ET,intT>(In);
  return ecall<scan<ET,intT,F,typeof(g)>>(st, Out, (intT) 0, n, f, g, zero, false, true, dest);
}

template <class ET, class intT, class F>
stack_type scanI(stack_type st, ET *In, ET* Out, intT n, F f, ET zero, ET* dest) {
  auto g = getA<ET,intT>(In);
  return ecall<scan<ET,intT,F,typeof(g)>>(st, Out, (intT) 0, n, f, g, zero, true, false, dest);
}

template <class ET, class intT, class F>
stack_type scanIBack(stack_type st, ET *In, ET* Out, intT n, F f, ET zero, ET* dest) {
  auto g = getA<ET,intT>(In);
  return ecall<scan<ET,intT,F,typeof(g)>>(st, Out, (intT) 0, n, f, g, zero, true, true, dest);
}
  
template <class ET, class intT>
stack_type plusScan(stack_type st, ET *In, ET* Out, intT n, ET* dest) {
  auto f = utils::addF<ET>();
  auto g = getA<ET,intT>(In);
  return ecall<scan<ET, intT, typeof(f), typeof(g)>>(st, Out, (intT)0, n, f, g, (ET)0, false, false, dest);
}
  
template <class intT>
class sumFlagsSerial : public encore::edsl::pcfg::shared_activation_record {
public:
  
  bool *Fl; intT n; intT* dest;
  int* IFl; int k; intT r; intT j;
  
  sumFlagsSerial() { }
  
  sumFlagsSerial(bool *Fl, intT n, intT* dest)
  : Fl(Fl), n(n), dest(dest) { }
  
  encore_dc_declare(encore::edsl, sumFlagsSerial, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par&) {
        s.r = 0;
        s.k = 0;
        s.j = 0;
      }),
      dc::mk_if([] (sar& s, par&) { return (s.n >= 128 && (s.n & 511) == 0 && ((long) s.Fl & 3) == 0); }, dc::stmts({
        dc::stmt([] (sar& s, par&) {
          s.IFl = (int*)s.Fl;
        }),
        dc::sequential_loop([] (sar& s, par&) { return s.k < (s.n >> 9); }, dc::stmt([] (sar& s, par&) {
          int rr = 0;
          auto IFl = s.IFl;
          for (int j=0; j < 128; j++) rr +=IFl[j];
          s.r += (rr&255) + ((rr>>8)&255) + ((rr>>16)&255) + ((rr>>24)&255);
          s.IFl += 128;
          s.k++;
        }))
      }),
      dc::sequential_loop([] (sar& s, par&) { return s.j < s.n; }, dc::stmt([] (sar& s, par&) {
        intT r = 0;
        auto Fl = s.Fl;
        for (intT j=0; j < s.n; j++) r += Fl[j];
        s.r = r;
        s.j++;
      }))),
      dc::stmt([] (sar& s, par&) {
        *s.dest = s.r;
      })
    });
  }
  
};

template <class intT>
typename sumFlagsSerial<intT>::cfg_type sumFlagsSerial<intT>::cfg = sumFlagsSerial<intT>::get_cfg();
  
template <class ET, class intT, class F>
class packSerial : public encore::edsl::pcfg::shared_activation_record {
public:
  
  ET* Out; bool* Fl; intT s; intT e; F f; _seq<ET>* dest;
  intT m; intT k;
  
  packSerial() { }
  
  packSerial(ET* Out, bool* Fl, intT s, intT e, F f, _seq<ET>* dest)
  : Out(Out), Fl(Fl), s(s), e(e), f(f), dest(dest) { }
  
  encore_dc_declare(encore::edsl, packSerial, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::mk_if([] (sar& s, par&) { return s.Out == nullptr; }, dc::stmts({
        dc::spawn_join([] (sar& s, par& p, stt st) {
          return ecall<sumFlagsSerial<intT>>(st, s.Fl + s.s, s.e - s.s, &s.m);
        }),
        dc::stmt([] (sar& s, par&) {
          s.Out = malloc_array<ET>(s.m);
        })
      })),
      dc::stmt([] (sar& s, par&) {
        s.k = 0;
      }),
      dc::sequential_loop([] (sar& s, par&) { return s.s < s.e; }, dc::stmt([] (sar& s, par&) {
        auto Fl = s.Fl;
        auto Out = s.Out;
        auto kk = s.k;
        auto f = s.f;
        auto ss = s.s;
        auto ee = std::min(s.e, ss + threshold);
        for (; ss < ee; ss++) {
          if (Fl[ss]) Out[kk++] = f(ss);
        }
        s.s = ss;
        s.k = kk;
      })),
      dc::stmt([] (sar& s, par&) {
        *s.dest = _seq<ET>(s.Out,s.k);
      })
    });
  }
  
};
  
template <class ET, class intT, class F>
typename packSerial<ET,intT,F>::cfg_type packSerial<ET,intT,F>::cfg = packSerial<ET,intT,F>::get_cfg();
  
template <class ET, class intT, class F>
class pack : public encore::edsl::pcfg::shared_activation_record {
public:
  
  ET* Out; bool* Fl; intT s; intT e; F f; _seq<ET>* dest;
  intT l; intT m; intT* Sums;
  
  pack() { }

  pack(ET* Out, bool* Fl, intT s, intT e, F f, _seq<ET>* dest)
  : Out(Out), Fl(Fl), s(s), e(e), f(f), dest(dest) { }
  
  encore_private_activation_record_begin(encore::edsl, pack, 2)
    int s; int e;
    _seq<ET> tmp;
  encore_private_activation_record_end(encore::edsl, pack, sar, par, dc, get_dc)
  
  static inline
  int get_pack_block_size() {
    return 2 * block_size;
  }
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par&) {
        s.l = nb_blocks(s.e - s.s, get_pack_block_size());
      }),
      dc::mk_if([] (sar& s, par&) { return s.l <= 1; }, dc::stmts({
        dc::spawn_join([] (sar& s, par&, stt st) {
          return ecall<packSerial<ET, intT, F>>(st, s.Out, s.Fl, s.s, s.e, s.f, s.dest);
        }),
        dc::exit()
      })),
      dc::stmt([] (sar& s, par& p) {
        s.Sums = malloc_array<intT>(s.l);
        p.s = 0;
        p.e = s.l;
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
                            dc::stmts({
        dc::spawn_join([] (sar& s, par& p, stt st) {
          auto rng = get_rng(get_pack_block_size(), p.s, p.e, s.s, s.e);
          return ecall<sumFlagsSerial<intT>>(st, s.Fl + rng.lo, rng.hi - rng.lo, &s.Sums[p.s]);
        }),
        dc::stmt([] (sar& s, par& p) {
          p.s++;
        })
      })),
      dc::spawn_join([] (sar& s, par& p, stt st) {
        return plusScan(st, s.Sums, s.Sums, s.l, &s.m);
      }),
      dc::stmt([] (sar& s, par& p) {
        if (s.Out == nullptr) s.Out = malloc_array<ET>(s.m);
        p.s = 0;
        p.e = s.l;
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
                            dc::stmts({
        dc::spawn_join([] (sar& s, par& p, stt st) {
          auto rng = get_rng(get_pack_block_size(), p.s, p.e, s.s, s.e);
          return ecall<packSerial<ET, intT, F>>(st, s.Out + s.Sums[p.s], s.Fl, rng.lo, rng.hi, s.f, &p.tmp);
        }),
        dc::stmt([] (sar& s, par& p) {
          p.s++;
        })
      })),
      dc::stmt([] (sar& s, par& p) {
        free(s.Sums);
        *s.dest = _seq<ET>(s.Out,s.m);
      }) 
    });
  }
  
};
  
template <class ET, class intT, class F>
typename pack<ET,intT,F>::cfg_type pack<ET,intT,F>::cfg = pack<ET,intT,F>::get_cfg();
  
template <class ET, class intT>
stack_type pack5(stack_type st, ET* In, ET* Out, bool* Fl, intT n, _seq<ET>* dest) {
  auto f = getA<ET,intT>(In);
  return ecall<pack<ET,intT,typeof(f)>>(st, Out, Fl, (intT) 0, n, f, dest);
}
  
template <class ET, class intT>
stack_type pack4(stack_type st, ET* In, bool* Fl, intT n, _seq<ET>* dest) {
  auto f = getA<ET,intT>(In);
  return ecall<pack<ET,intT,typeof(f)>>(st, (ET*) nullptr, Fl, (intT) 0, n, f, dest);
}

template <class ET, class intT, class PRED>
class filterDPS : public encore::edsl::pcfg::shared_activation_record {
public:
  
  ET* In; ET* Out; intT n; PRED p; intT* dest;
  _seq<ET> tmp; intT m; bool* Fl;
  
  filterDPS() { }
  
  filterDPS(ET* In, ET* Out, intT n, PRED p, intT* dest)
  : In(In), Out(Out), n(n), p(p), dest(dest) { }
  
  encore_private_activation_record_begin(encore::edsl, filterDPS, 1)
    int s; int e;
  encore_private_activation_record_end(encore::edsl, filterDPS, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        s.Fl = malloc_array<bool>(s.n);
        p.s = 0;
        p.s = s.n;
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
                            dc::stmts({
        dc::stmt([] (sar& s, par& p) {
          auto Fl = s.Fl;
          auto pr = s.p;
          auto In = s.In;
          intT ss = p.s;
          intT ee = std::min(p.e, p.s + threshold);
          for (; ss < ee; ss++) {
            Fl[ss] = (bool)pr(In[ss]);
          }
          p.s = ss;
        })
      })),
      dc::spawn_join([] (sar& s, par& p, stt st) {
        return pack5(st, s.In, s.Out, s.Fl, s.n, &s.tmp);
      }),
      dc::stmt([] (sar& s, par& p) {
        s.m = (intT)s.tmp.n;
        free(s.Fl);
        *s.dest = s.m;
      })
    });
  }
  
};

template <class ET, class intT, class PRED>
typename filterDPS<ET,intT,PRED>::cfg_type filterDPS<ET,intT,PRED>::cfg = filterDPS<ET,intT,PRED>::get_cfg();
  
template <class ET, class intT, class PRED>
class filter : public encore::edsl::pcfg::shared_activation_record {
public:
  
  ET* In; intT n; PRED p; _seq<ET>* dest;
  bool* Fl;
  
  filter() { }
  
  filter(ET* In, intT n, PRED p, _seq<ET>* dest)
  : In(In), n(n), p(p), dest(dest) { }
  
  encore_private_activation_record_begin(encore::edsl, filter, 1)
  int s; int e;
  encore_private_activation_record_end(encore::edsl, filter, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        s.Fl = malloc_array<bool>(s.n);
        p.s = 0;
        p.e = s.n;
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
                            dc::stmts({
        dc::stmt([] (sar& s, par& p) {
          auto Fl = s.Fl;
          auto pr = s.p;
          auto In = s.In;
          intT ss = p.s;
          intT ee = std::min(p.e, p.s + threshold);
          for (; ss < ee; ss++) {
            Fl[ss] = (bool)pr(In[ss]);
          }
          p.s = ss;
        })
      })),
      dc::spawn_join([] (sar& s, par& p, stt st) {
        return pack4<ET,intT>(st, s.In, s.Fl, s.n, s.dest);
      }),
      dc::stmt([] (sar& s, par& p) {
        free(s.Fl);
      })
    });
  }
  
};

template <class ET, class intT, class PRED>
typename filter<ET,intT,PRED>::cfg_type filter<ET,intT,PRED>::cfg = filter<ET,intT,PRED>::get_cfg();
  
template <class Iter, class Output_iterator>
class copy : public encore::edsl::pcfg::shared_activation_record {
public:
  
  Iter lo; Iter hi; Output_iterator dst;
  
  copy() { }
  
  copy(Iter lo, Iter hi, Output_iterator dst)
  : lo(lo), hi(hi), dst(dst) { }
  
  encore_private_activation_record_begin(encore::edsl, copy, 1)
    int s; int e;
  encore_private_activation_record_end(encore::edsl, copy, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        p.s = 0;
        p.e = s.hi - s.lo;
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
                            dc::stmts({
        dc::stmt([] (sar& s, par& p) {
          intT ss = p.s;
          intT ee = std::min(p.e, p.s + threshold);
          auto lo = s.lo + ss;
          auto hi = s.lo + ee;
          std::copy(lo, hi, s.dst + (lo - s.lo));
          p.s = ee;
        })
      }))
    });
  }
  
};
  
template <class Iter, class Output_iterator>
typename copy<Iter,Output_iterator>::cfg_type copy<Iter,Output_iterator>::cfg = copy<Iter,Output_iterator>::get_cfg();
  
void initialize() {
  threshold = cmdline::parse_or_default("threshold", sequence::threshold);
  block_size = cmdline::parse_or_default("block_size", sequence::block_size);
}
  
} // end namespace

#endif /*! _ENCORE_PBBS_SEQUENCE_H_ */
