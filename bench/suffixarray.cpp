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

  intT query(intT,intT);
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
      dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.rmq->depth; },
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

intT myRMQ::query(intT i, intT j){
  //same block
  if (j-i < BSIZE) {
    intT r = i;
    for (intT k = i+1; k <= j; k++) 
      if (a[k] < a[r]) r = k;
    return r;
  } 
  intT block_i = i/BSIZE;
  intT block_j = j/BSIZE;
  intT min = i;
  for(intT k=i+1;k<(block_i+1)*BSIZE;k++){
    if(a[k] < a[min]) min = k;
  }
  for(intT k=j; k>=(block_j)*BSIZE;k--){
    if(a[k] < a[min]) min = k;
  }
  if(block_j == block_i + 1) return min;
  intT outOfBlockMin;
  //not same or adjacent blocks
  if(block_j > block_i + 1){
    block_i++;
    block_j--;
    if(block_j == block_i) outOfBlockMin = table[0][block_i];
    else if(block_j == block_i + 1) outOfBlockMin = table[1][block_i];
    else {
      intT k = log2(block_j - block_i);
      intT p = 1<<k; //2^k
      outOfBlockMin = a[table[k][block_i]] <= a[table[k][block_j+1-p]]
        ? table[k][block_i] : table[k][block_j+1-p];
    }
  }

  return a[min] < a[outOfBlockMin] ? min : outOfBlockMin;

}

#define _MERGE_BSIZE (1 << 12)

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
  intT lr; intT m1, m2;

  merge(ET* S1, intT l1, ET* S2, intT l2, ET* R, F f)
    : S1(S1), l1(l1), S2(S2), l2(l2), R(R), f(f) { }
  
  encore_dc_declare(encore::edsl, merge, sar, par, dc, get_dc)

  static
  dc get_dc() {
    return dc::mk_if([] (sar& s, par& p) {
        return s.lr > _MERGE_BSIZE;
      }, dc::mk_if([] (sar& s, par& p) {
          return s.l2 > s.l1;
        }, dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          return encore_call<merge<ET,F,intT>>(st, pt, s.S2, s.l2, s.S1, s.l1, s.R, s.f);
        }), dc::stmts({dc::stmt([] (sar& s, par& p) {
              s.m1 = s.l1 / 2;
              s.m2 = binsearch(s.S2, s.l2, s.S1[s.m1], s.f);
             }),
             dc::spawn2_join(
               [] (sar& s, par&, plt_type pt, stt st) {
                 return encore_call<merge<ET,F,intT>>(st, pt, s.S1, s.m1, s.S2, s.m2, s.R, s.f);
               },
               [] (sar& s, par&, plt_type pt, stt st) {
                 return encore_call<merge<ET,F,intT>>(st, pt,
                              s.S1 + s.m1, s.s.l1 - m1, s.S2 + s.m2, s.l2 - s.m2,
                                                      s.R + s.m1 + s.m2, s.f);
               })
            })),
    dc::stmt([] (sar& s, par& p) {
      ET* pR = s.R; 
      ET* pS1 = s.S1; 
      ET* pS2 = s.S2;
      ET* eS1 = s.S1 + s.l1; 
      ET* eS2 = s.S2 + s.l2;
      auto f = s.f;
      while (true) {
        if (pS1==eS1) {std::copy(pS2,eS2,pR); break;}
        if (pS2==eS2) {std::copy(pS1,eS1,pR); break;}
        *pR++ = f(*pS2,*pS1) ? *pS2++ : *pS1++;
      }
    }));
  }
  
};

template <class ET, class F, class intT> 
typename merge<ET,F,intT>::cfg_type merge<ET,F,intT>::cfg = merge<ET,F,intT>::get_cfg();

inline bool leq(intT a1, intT a2,   intT b1, intT b2) {
  return(a1 < b1 || a1 == b1 && a2 <= b2); 
}                                                  

inline bool leq(intT a1, intT a2, intT a3, intT b1, intT b2, intT b3) {
  return(a1 < b1 || a1 == b1 && leq(a2,a3, b2,b3)); 
}

struct compS {
  intT* _s;
  intT* _s12;
  compS(intT* s, intT* s12) : _s(s), _s12(s12) {}
  int operator () (intT i, intT j) {
    if (i%3 == 1 || j%3 == 1) 
      return leq(_s[i],_s12[i+1], _s[j],_s12[j+1]);
    else
      return leq(_s[i],_s[i+1],_s12[i+2], _s[j],_s[j+1],_s12[j+2]);
  }
};

struct mod3is1 { bool operator() (intT i) {return i%3 == 1;}};

inline intT computeLCP(intT* LCP12, intT* rank, myRMQ & RMQ, 
		      intT j, intT k, intT* s, intT n){
 
  intT rank_j=rank[j]-2;
  intT rank_k=rank[k]-2;
  if(rank_j > rank_k) {std::swap(rank_j,rank_k);} //swap for RMQ query

  intT l = ((rank_j == rank_k-1) ? LCP12[rank_j] 
	   : LCP12[RMQ.query(rank_j,rank_k-1)]);

  intT lll = 3*l;
  if (s[j+lll] == s[k+lll]) {
    if (s[j+lll+1] == s[k+lll+1]) return lll + 2;
    else return lll + 1;
  } 
  return lll;
}

using stack_type = encore::edsl::pcfg::stack_type;
  
using plt_type = encore::edsl::pcfg::cactus::parent_link_type;

template <class Activation_record, class ...Args>
static stack_type encore_call(stack_type s, plt_type p, Args... args) {
  return encore::edsl::pcfg::push_call<Activation_record>(s, p, args...);
}

template <class intT>
stack_type radixSortPair(stack_type s, plt_type pt, pair<intT,intT> *A, intT n, intT m) {
  return intSort::iSort4(s, pt, A, n, m, utils::firstF<intT,intT>());
}

class suffix_array_rec : public encore::edsl::pcfg::shared_activation_record {
public:

  intT* s; intT n; intT K; bool findLCPs;
  intT n0, n1, n12; pair<intT,intT> *C; intT bits; intT* sorted12;
  intT* name12; intT tmp; intT names; pair<intT*,intT*> SA12_LCP;
  intT* SA12; intT* LCP12 = NULL; intT* s12; intT* rank;
  intT* s0; intT x; std::pair<intT,intT> *D; intT* SA0; intT o;
  intT* SA; intT* LCP; myRMQ RMQ;
  pair<intT*,intT*>* dest;
  
  suffix_array_rec() { }
  
  suffix_array_rec(intT* s, intT n, intT K, bool findLCPs, pair<intT*,intT*>* dest)
    : s(s), n(n), K(K), findLCPs(findLCPs), dest(dest) { }
  
  encore_private_activation_record_begin(encore::edsl, suffix_array_rec, 13)
    int lo; int hi;
  encore_private_activation_record_end(encore::edsl, suffix_array_rec, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        s.n++;
        s.n0 = (s.n + 2) / 3;
        s.n1 = (s.n + 1) / 3;
        s.n12 = s.n - s.n0;
        s.C = (pair<intT,intT> *) malloc(s.n12*sizeof(pair<intT,intT>));
        s.bits = utils::logUp(s.K);
      }),
      // if 3 chars fit into an int then just do one radix sort
      dc::mk_if([] (sar& s, par& p) {
        return s.bits < s.l1;
      }, dc::stmts({
        dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.n12; },
                              [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                              [] (sar& s, par& p, int lo, int hi) {
          auto bits = s.bits;
          auto C = s.C;
          auto _s = s.s;
          for (auto i = lo; i != hi; i++) {
            intT j = 1+(i+i+i)/2;
            C[i].first = (_s[j] << 2*bits) + (_s[j+1] << bits) + _s[j+2];
            C[i].second = j;
          }
        }),
        dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          return radixSortPair(st, pt, s.C, s.n12, s.K);
        })
      }), dc::stmts({ // else
        // otherwise do 3 radix sorts, one per char
        dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.n12; },
                              [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                              [] (sar& s, par& p, int lo, int hi) {
          auto bits = s.bits;
          auto C = s.C;
          auto _s = s.s;
          for (auto i = lo; i != hi; i++) {
            intT j = 1+(i+i+i)/2;
            C[i].first = _s[j+2]; 
            C[i].second = j;
          }
        }), 
        dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          return radixSortPair(st, pt, s.C, s.n12, s.K);
        }),
        dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.n12; },
                              [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                              [] (sar& s, par& p, int lo, int hi) {
          auto C = s.C;
          auto _s = s.s;
          for (auto i = lo; i != hi; i++) {
            C[i].first = _s[C[i].second+1];
          }
        }), 
        dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          return radixSortPair(st, pt, s.C, s.n12, s.K);
        }),
        dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.n12; },
                              [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                              [] (sar& s, par& p, int lo, int hi) {
          auto C = s.C;
          auto _s = s.s;
          for (auto i = lo; i != hi; i++) {
            C[i].first = _s[C[i].second];
          }
        }), 
        dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          return radixSortPair(st, pt, s.C, s.n12, s.K);
        })
      })), // end if
      // copy sorted results into sorted12
      dc::stmt([] (sar& s, par& p) {
        s.sorted12 = malloc_array<intT>(s.n12);
      }),
      dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.n12; },
                            [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                            [] (sar& s, par& p, int lo, int hi) {
        auto sorted12 = s.sorted12;
        auto C = s.C;
        for (auto i = lo; i != hi; i++) {
          sorted12[i] = C[i].second;
        }
      }), 
      dc::stmt([] (sar& s, par& p) {
        free(s.C);
      }),
      // generate names based on 3 chars
      dc::stmt([] (sar& s, par& p) {
        s.name12 = malloc_array<intT>(s.n12);        
      }),
      dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 1; p.hi = s.n12; },
                            [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                            [] (sar& s, par& p, int lo, int hi) {
        auto name12 = s.name12;
        auto sorted = s.sorted;
        auto _s = s.s;
        for (auto i = lo; i != hi; i++) {
          if (_s[sorted12[i]]!=_s[sorted12[i-1]] 
              || _s[sorted12[i]+1]!=_s[sorted12[i-1]+1] 
              || _s[sorted12[i]+2]!=_s[sorted12[i-1]+2]) 
            name12[i] = 1;
          else name12[i] = 0;
        }
      }), 
      dc::stmt([] (sar& s, par& p) {
        s.name12[0] = 1;
      }),
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        return sequence::scanI(st, pt, s.name12, s.name12, s.n12, utils::addF<intT>(), (intT)0, &s.tmp);
      }),
      dc::stmt([] (sar& s, par& p) {
        s.names = s.name12[s.n12-1];
      }),
      // recurse if names are not yet unique
      dc::mk_if([] (sar& s, par& p) {
        return s.names < s.n12;
      }, dc::stmts({
        dc::stmt([] (sar& s, par& p) {
          s.s12 = malloc_array<intT>(s.n12 + 3);
          s.s12[s.n12] = s.s12[s.n12+1] = s.s12[s.n12+2] = 0;
        }),
        // move mod 1 suffixes to bottom half and and mod 2 suffixes to top
        dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.n12; },
                              [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                              [] (sar& s, par& p, int lo, int hi) {
          auto sorted12 = s.sorted12;
          auto s12 = s.s12;
          auto name12 = s.name12;
          auto n1 = s.n1;
          for (auto i = lo; i != hi; i++) {
            if (sorted12[i]%3 == 1) s12[sorted12[i]/3] = name12[i];
            else s12[sorted12[i]/3+n1] = name12[i];
          }
        }),
        dc::stmt([] (sar& s, par& p) {
          free(name12);
          free(sorted12);
        }),
        dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          return encore_call<suffix_array_rec>(st, pt, s.s12, s.n12, s.names+1, s.findLCPs, &(s.SA12_LCP));
        }),
        dc::stmt([] (sar& s, par& p) {
          s.SA12 = s.SA12_LCP.first;
          s.LCP12 = s.SA12_LCP.second;
          free(s.s12);
        }),
        // restore proper indices into original array
        dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.n12; },
                              [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                              [] (sar& s, par& p, int lo, int hi) {
          auto SA12 = s.SA12;
          auto n1 = s.n1;
          for (auto i = lo; i != hi; i++) {
            intT l = SA12[i]; 
            SA12[i] = (l<n1) ? 3*l+1 : 3*(l-n1)+2;
          }
        })
      }), dc::stmts({ // else
        dc::stmt([] (sar& s, par& p) {
          free(s.name12); // names not needed if we don't recurse
          s.SA12 = s.sorted12; // suffix array is sorted array
        }),
        dc::mk_if([] (sar& s, par& p) {
          return s.findLCPs;
        }, dc::parallel_for_loop([] (sar& s, par& p) {
             s.LCP12 = malloc_array<intT>(s.n12 + 3);
             p.lo = 0; p.hi = s.n12 + 3;
           }, [] (par& p) {
             return std::make_pair(&p.lo, &p.hi);
           }, [] (sar& s, par& p, int lo, int hi) {
             std::fill(s.LCP12 + lo, s.LCP12 + hi, 0);  //LCP's are all 0 if not recursing
        }))
      })), // end if
      dc::stmt([] (sar& s, par& p) {
        s.rank = malloc_array<intT>(s.n + 2);
        s.rank[s.n]=1; s.rank[s.n+1] = 0;
      }),
      dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.n12; },
                            [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                            [] (sar& s, par& p, int lo, int hi) {
          auto rank = s.rank;
          auto SA12 = s.SA12;
          for (auto i = lo; i != hi; i++) {
            rank[SA12[i]] = i+2;
          }
      }),
      dc::stmt([] (sar& s, par& p) {
        s.s0 = malloc_array<intT>(s.n0);
      }),
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        return encore_call<filterDPS<intT,intT,mod3is1>>(st, pt, s.SA12, s.s0, s.n12, mod3is1(), &(s.x));
      }),
      dc::stmt([] (sar& s, par& p) {
        s.D = malloc_array<std::pair<intT,intT>>(s.n0);
        s.D[0].first = s.s[s.n-1]; s.D[0].second = s.n-1;
      }),
      dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.x; },
                            [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                            [] (sar& s, par& p, int lo, int hi) {
          auto D = s.D;
          auto n0 = s.n0;
          auto _s = s.s;
          auto x = s.x;
          auto s0 = s.s0;
          for (auto i = lo; i != hi; i++) {
            D[i+n0-x].first = _s[s0[i]-1]; 
            D[i+n0-x].second = s0[i]-1;
          }
      }),       
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        return radixSortPair(st, pt, s.D, s.n0, s.K);
      }),
      dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.n0; },
                            [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                            [] (sar& s, par& p, int lo, int hi) {
        auto SA0 = s.SA0;
        auto D = s.D;
        for (auto i = lo; i != hi; i++) {
          SA0[i] = D[i].second;
        }
      }),       
      dc::stmt([] (sar& s, par& p) {
        free(s.D);
        s.o = (s.n%3 == 1) ? 1 : 0;
        s.SA = malloc_array<intT>(s.n);
      }),
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        compS comp(s,rank);
        return encore_call<merge<intT,compS,intT>>(st, pt, s.SA0 + s.o, s.n0 - s.o, s.SA12 + 1 - .o, s.n12 + s.o - 1, s.SA, comp);
      }),
      dc::stmt([] (sar& s, par& p) {
        free(s.SA0); free(s.SA12);
      }),
      dc::mk_if([] (sar& s, par& p) {
        return s.findLCPs;
      }, dc::stmts({
        dc::stmt([] (sar& s, par& p) {
          s.LCP = malloc_array<intT>(s.n);
          s.LCP[s.n-1] = s.LCP[s.n-2] = 0;
          s.RMQ.a = s.LCP12;
          s.RMQ.n = s.n12 + 3;
          s.RMQ.m = 1 + (s.n-1)/BSIZE;
        }),
        dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          return encore_call<precompute_queries>(st, pt, &(s.RMQ));
        }),
        dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.n - 2; },
                              [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                              [] (sar& s, par& p, int lo, int hi) {
          auto SA = s.SA;
          auto _s = s.s;
          auto LCP = s.LCP;
          auto k = s.k;
          auto LCP12 = s.LCP12;
          auto rank = s.rank;
          auto& RMQ = s.RMQ;
          auto n = s.n;
          for (auto i = lo; i != hi; i++) {
            intT j=SA[i];
            intT k=SA[i+1];
            int CLEN = 16;
            intT ii;
            for (ii=0; ii < CLEN; ii++) 
              if (_s[j+ii] != _s[k+ii]) break;
            if (ii != CLEN) LCP[i] = ii;
            else {
              if (j%3 != 0 && k%3 != 0)  
                LCP[i] = computeLCP(LCP12, rank, RMQ, j, k, _s, n); 
              else if (j%3 != 2 && k%3 != 2)
                LCP[i] = 1 + computeLCP(LCP12, rank, RMQ, j+1, k+1, _s, n);
              else 
                LCP[i] = 2 + computeLCP(LCP12, rank, RMQ, j+2, k+2, _s, n);
            }
          }
        }),
        dc::stmt([] (sar& s, par& p) {
          free(s.LCP12);
        })
      })), // end if
      dc::stmt([] (sar& s, par& p) {
        free(s.rank);
        *dest = std::make_pair(s.SA, s.LCP);
      })
    });
  }
  
};

encore_pcfg_allocate(suffix_array_rec, get_cfg)

class suffix_array : public encore::edsl::pcfg::shared_activation_record {
public:

  char* s; intT n; intT K; bool findLCPs;
  intT *ss; intT k; pair<intT*,intT*> SA_LCP;
  pair<intT*,intT*>* dest;
  
  suffix_array() { }
  
  suffix_array(char* s, intT n, bool findLCPs, pair<intT*,intT*>* dest)
    : s(s), n(n), K(K), findLCPs(findLCPs), dest(dest) { }
  
  encore_private_activation_record_begin(encore::edsl, suffix_array, 1)
    int lo; int hi;
  encore_private_activation_record_end(encore::edsl, suffix_array, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        s.ss = malloc_array<intT>(s.n + 3);
        s.ss[s.n] = s.ss[s.n+1] = s.ss[s.n+2] = 0;          
      }),
      dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.n; },
                            [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                            [] (sar& s, par& p, int lo, int hi) {
        auto ss = s.ss;
        auto _s = s.s;
        for (auto i = lo; i != hi; i++) {
          ss[i] = _s[i]+1;
        }
      }),       
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        auto f = utils::maxF<intT>();
        return encore_call<reduce<intT,intT,decltype(f)>(st, pt, s.ss, s.n, f, &(s.k));
      }),
      dc::stmt([] (sar& s, par& p) {
        s.k++;
      }),
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        return encore_call<suffix_array_rec>(st, pt, s.ss, s.n, s.k, s.findLCPs, &(s.SA_LCP));
      }),
      dc::stmt([] (sar& s, par& p) {
        free(s.ss);
        *dest = s.SA_LCP;
      })        
    });
  }

};
  
} // end namespace

#include "pks.h"

namespace pasl {
namespace pctl {

void benchmark(std::string infile) {
  std::string x = pasl::pctl::io::load<std::string>(infile);
  std::string algorithm = cmdline::parse<std::string>("algorithm");
  deepsea::cmdline::dispatcher d;
  std::pair<intT*, intT*> res;
  d.add("encore", [&] {
    encore::launch_interpreter<encorebench::suffix_array>(x.begin(), x.length(), &res);
  });
  d.add("pbbs", [&] {
    encore::run_and_report_elapsed_time([&] {
      pbbs::suffixArray(x.begin(), (int)x.length());
    });
  });
  d.dispatch("algorithm"); 
}

} // end namespace
} // end namespace

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  std::string infile = deepsea::cmdline::parse_or_default_string("infile", "");
  if (infile == "") {
    std::cerr << "bogus input filename" << std::endl;
    exit(0);
  }
  pasl::pctl::benchmark(infile);
  return 0;
}
