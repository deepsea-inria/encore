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

#include "utils.hpp"
#include "sequence.hpp"

#ifndef _SPECULATIVE_FOR_H_
#define _SPECULATIVE_FOR_H_

struct reservation {
  intT r;
  reservation() : r(INT_T_MAX) {}
  void reserve(intT i) { utils::writeMin(&r, i); }
  bool reserved() { return (r < INT_T_MAX);}
  void reset() {r = INT_T_MAX;}
  bool check(intT i) { return (r == i);}
  bool checkReset(intT i) {
    if (r==i) { r = INT_T_MAX; return 1;}
    else return 0;
  }
};

inline void reserveLoc(intT& x, intT i) {utils::writeMin(&x,i);}

template <class S>
class speculative_for : public encore::edsl::pcfg::shared_activation_record {
public:
  
  S step; intT s; intT e; int granularity;
  bool hasState; int maxTries; intT* dest;
  
  intT maxRoundSize; intT *I; intT *Ihold; bool *keep;
  S *state; intT size; _seq<intT> tmp;
  
  int round; intT numberDone; intT numberKeep; intT totalProcessed;
    
  speculative_for(S step, intT s, intT e, int granularity,
                  bool hasState, int maxTries, intT* dest)
  : step(step), s(s), e(e), granularity(granularity),
  hasState(hasState), maxTries(maxTries), dest(dest) { }
  
  encore_private_activation_record_begin(encore::edsl, speculative_for, 4)
  int s; int e;
  encore_private_activation_record_end(encore::edsl, speculative_for, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        if (s.maxTries < 0) s.maxTries = 100 + 200*s.granularity;
        s.maxRoundSize = (s.e-s.s)/s.granularity+1;
        s.I = malloc_array<intT>(s.maxRoundSize);
        s.Ihold = malloc_array<intT>(s.maxRoundSize);
        s.keep = malloc_array<bool>(s.maxRoundSize);
        if (s.hasState) {
          s.state = malloc_array<S>(s.maxRoundSize);
          for (intT i=0; i < s.maxRoundSize; i++) s.state[i] = s.step;
        }
        s.round = 0;
        s.numberDone = s.s; // number of iterations done
        s.numberKeep = 0; // number of iterations to carry to next round
        s.totalProcessed = 0;
      }),
      dc::sequential_loop([] (sar& s, par&) { return s.numberDone < s.e; }, dc::stmts({
        dc::stmt([] (sar& s, par& p) {
          if (s.round++ > s.maxTries) {
            std::cout << "speculativeLoop: too many iterations, increase maxTries parameter" << std::endl;
            abort();
          }
          s.size = std::min(s.maxRoundSize, s.e - s.numberDone);
          s.totalProcessed += s.size;
          p.s = 0;
          p.e = s.size;
        }),
        dc::mk_if([] (sar& s, par&) { return s.hasState; },
          dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                                [] (par& p) { return std::make_pair(&p.s, &p.e); },
            dc::stmt([] (sar& s, par& p) {
              if (p.s >= s.numberKeep) s.I[p.s] = s.numberDone + p.s;
              s.keep[p.s] = s.state[p.s].reserve(s.I[p.s]);
              p.s++;
            })),
          dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                                [] (par& p) { return std::make_pair(&p.s, &p.e); },
            dc::stmt([] (sar& s, par& p) {
              if (p.s >= s.numberKeep) s.I[p.s] = s.numberDone + p.s;
              s.keep[p.s] = s.step.reserve(s.I[p.s]);
              p.s++;
          }))),
        dc::mk_if([] (sar& s, par&) { return s.hasState; },
          dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                                [] (par& p) { return std::make_pair(&p.s, &p.e); },
                                dc::stmt([] (sar& s, par& p) {
            if (s.keep[p.s]) s.keep[p.s] = !s.state[p.s].commit(s.I[p.s]);
            p.s++;
          })),
          dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                                [] (par& p) { return std::make_pair(&p.s, &p.e); },
                                dc::stmt([] (sar& s, par& p) {
            if (s.keep[p.s]) s.keep[p.s] = !s.step.commit(s.I[p.s]);
            p.s++;
          }))),
        dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
          return sequence::pack5(st, pt, s.I, s.Ihold, s.keep, s.size, &s.tmp);
        }),
        dc::stmt([] (sar& s, par& p) {
          s.numberKeep = (intT)s.tmp.n;
          std::swap(s.I, s.Ihold);
          s.numberDone += s.size + s.numberKeep;
        })
      })),
      dc::stmt([] (sar& s, par& p) {
        free(s.I); free(s.Ihold); free(s.keep);
        if(s.hasState) free(s.state);
        *s.dest = s.totalProcessed;
      })
    });
  }
  
};

template <class S>
typename speculative_for<S>::cfg_type speculative_for<S>::cfg = speculative_for<S>::get_cfg();

using stack_type = encore::edsl::pcfg::stack_type;

template <class S>
stack_type speculative_for4(stack_type st, plt_type pt, S step, intT s, intT e, int granularity, intT* dest) {
  return sequence::encore_call<speculative_for<S>>(st, pt, step, s, e, granularity, 1, -1, dest);
}

#endif
