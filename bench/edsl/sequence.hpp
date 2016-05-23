
#ifndef _ENCORE_PBBS_SEQUENCE_H_
#define _ENCORE_PBBS_SEQUENCE_H_

#include "edsl.hpp"
#include "utils.hpp"

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

namespace sequence {
 
int threshold = 4;
int block_size = 8;
  
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
        F f = s.f;
        G g = s.g;
        intT ss = s.s;
        intT e = std::min(s.e, ss + threshold);
        OT r = *s.dest;
        for (; ss < e; ss++) {
          r = f(r, g(ss));
        }
        *s.dest = r;
        s.s = ss;
      }))
    });
  }
  
};
  
template <class OT, class intT, class F, class G>
typename reduceSerial<OT,intT,F,G>::cfg_type reduceSerial<OT,intT,F,G>::cfg = reduceSerial<OT,intT,F,G>::get_cfg();

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
          intT ss = s.s + p.s * block_size;
          intT ee = std::min(ss + block_size, s.e);
          return ecall<reduceSerial<OT,intT,F,G>>(st, ss, ee, s.f, s.g, &s.Sums[p.s]);
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
  
template <class OT, class intT, class F, class G>
typename reduce<OT,intT,F,G>::cfg_type reduce<OT,intT,F,G>::cfg = reduce<OT,intT,F,G>::get_cfg();
  
using stack_type = encore::cactus::stack_type;

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
  intT k;
  
  maxIndexSerial() { }
  
  maxIndexSerial(intT s, intT e, F f, G g, intT* dest)
  : s(s), e(e), f(f), g(g), dest(dest) { }
  
  encore_dc_declare(encore::edsl, maxIndexSerial, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par&) {
        s.k = 0;
        *s.dest = s.g(s.s);
        s.s++;
      }),
      dc::sequential_loop([] (sar& s, par&) { return s.s != s.e; }, dc::stmt([] (sar& s, par&) {
        intT j = s.s;
        intT k = s.k;
        intT r = *s.dest;
        auto g = s.g;
        auto f = s.f;
        intT e = std::min(s.e, j + threshold);
        for (; j < e; j++) {
          ET v = g(j);
          if (f(v,r)) {
            r = v;
            k = j;
          }
        }
        *s.dest = r;
        s.k = k;
        s.s = j;
      }))
    });
  }
  
};
  
template <class ET, class intT, class F, class G>
typename maxIndexSerial<ET,intT,F,G>::cfg_type maxIndexSerial<ET,intT,F,G>::cfg = maxIndexSerial<ET,intT,F,G>::get_cfg();

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
          intT ss = s.s + p.s * block_size;
          intT ee = std::min(ss + block_size, s.e);
          return ecall<maxIndexSerial<ET, intT, F, G>>(st, ss, ee, s.f, s.g, &s.Idx[p.s]);
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
  
template <class ET, class intT, class F, class G>
typename maxIndex<ET,intT,F,G>::cfg_type maxIndex<ET,intT,F,G>::cfg = maxIndex<ET,intT,F,G>::get_cfg();

template <class ET, class intT, class F>
stack_type maxIndex4(ET* A, intT n, F f, intT* dest) {
  auto g = getA<ET,intT>(A);
  return ecall<maxIndex<ET,intT,F,typeof(g)>>((intT)0, n, f, g);
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

template <class ET, class intT, class F, class G>
class scan : public encore::edsl::pcfg::shared_activation_record {
public:
  
  ET* Out; intT s; intT e; F f; G g; ET zero; bool inclusive; bool back; ET* dest;
  intT i; intT n; intT l; ET* Sums; ET tmp;
  
  scan() { }
  
  scan(ET* Out, intT s, intT e, F f, G g, ET zero, bool inclusive, bool back, ET* dest)
  : Out(Out), s(s), e(e), f(f), g(g), zero(zero), inclusive(inclusive), back(back), dest(dest) { }
  
  encore_private_activation_record_begin(encore::edsl, scan, 2)
    int s; int e;
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
          intT ss = s.s + p.s * block_size;
          intT ee = std::min(ss + block_size, s.e);
          return ecall<reduceSerial<ET,intT,F,G>>(st, ss, ee, s.f, s.g, &s.Sums[p.s]);
        }),
        dc::stmt([] (sar& s, par& p) {
          p.s++;
        })
      }))
      ,
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
          intT ss = s.s + p.s * block_size;
          intT ee = std::min(ss + block_size, s.e);
          return ecall<scanSerial<ET,intT,F,G>>(st, s.Out, ss, ee, s.f, s.g, s.Sums[p.s], s.inclusive, s.back, &s.tmp);
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
  
} // end namespace

#endif /*! _ENCORE_PBBS_SEQUENCE_H_ */