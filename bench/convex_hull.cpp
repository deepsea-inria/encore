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

#include <iostream>
#include <chrono>
#include <array>
#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>
#include <algorithm>

#include "encore.hpp"
#include "sequence.hpp"
#include "geometry.hpp"

#include "test.hpp"
#include "prandgen.hpp"
#include "geometryio.hpp"
#include "loaders.hpp"

#include "hull.h"

namespace sched = encore::sched;
namespace cmdline = deepsea::cmdline;
namespace dsl = encore::edsl;

using namespace std;
using namespace sequence;

using point2d = pasl::pctl::point2d;

template <class ET, class F>
pair<intT,intT> split(ET* A, intT n, F lf, F rf) {
  intT ll = 0, lm = 0;
  intT rm = n-1, rr = n-1;
  while (1) {
    while ((lm <= rm) && !(rf(A[lm]) > 0)) {
      if (lf(A[lm]) > 0) A[ll++] = A[lm];
      lm++;
    }
    while ((rm >= lm) && !(lf(A[rm]) > 0)) {
      if (rf(A[rm]) > 0) A[rr--] = A[rm];
      rm--;
    }
    if (lm >= rm) break;
    ET tmp = A[lm++];
    A[ll++] = A[rm--];
    A[rr--] = tmp;
  }
  intT n1 = ll;
  intT n2 = n-rr-1;
  return pair<intT,intT>(n1,n2);
}

struct aboveLine {
  intT l, r;
  point2d* P;
  aboveLine(point2d* _P, intT _l, intT _r) : P(_P), l(_l), r(_r) {}
  bool operator() (intT i) {return pasl::pctl::triangle_area(P[l], P[r], P[i]) > 0.0;}
};

intT serialQuickHull(intT* I, point2d* P, intT n, intT l, intT r) {
  if (n < 2) return n;
  intT maxP = I[0];
  double maxArea = pasl::pctl::triangle_area(P[l],P[r],P[maxP]);
  for (intT i=1; i < n; i++) {
    intT j = I[i];
    double a = pasl::pctl::triangle_area(P[l],P[r],P[j]);
    if (a > maxArea) {
      maxArea = a;
      maxP = j;
    }
  }
  
  pair<intT,intT> nn = split(I, n, aboveLine(P,l,maxP), aboveLine(P,maxP,r));
  intT n1 = nn.first;
  intT n2 = nn.second;
  
  intT m1, m2;
  m1 = serialQuickHull(I,      P, n1, l,   maxP);
  m2 = serialQuickHull(I+n-n2, P, n2, maxP,r);
  for (intT i=0; i < m2; i++) I[i+m1+1] = I[i+n-n2];
  I[m1] = maxP;
  return m1+1+m2;
}

struct triangArea {
  intT l, r;
  point2d* P;
  intT* I;
  triangArea(intT* _I, point2d* _P, intT _l, intT _r) : I(_I), P(_P), l(_l), r(_r) {}
  double operator() (intT i) {return pasl::pctl::triangle_area(P[l], P[r], P[I[i]]);}
};

int quickHull_threshold = 10000;

class quickHull : public encore::edsl::pcfg::shared_activation_record {
public:
  
  intT* I; intT* Itmp; point2d* P; intT n; intT l; intT r; intT depth;
  intT* dest;
  intT idx; intT maxP; intT n1; intT n2; intT m1; intT m2;
    
  quickHull(intT* I, intT* Itmp, point2d* P, intT n, intT l, intT r, intT depth, intT* dest)
  : I(I), Itmp(Itmp), P(P), n(n), l(l), r(r), depth(depth), dest(dest) { }
  
  encore_dc_declare(encore::edsl, quickHull, sar, par, dc, get_dc)
  
  sattic
  dc get_dc() {
    return dc::stmts({
      dc::mk_if([&] (sar& s, par&) { return s.n < quickHull_threshold; }, dc::stmts({
        dc::stmt([] (sar& s, par&) {
          *s.dest = serialQuickHull(s.I, s.P, s.n, s.l, s.r);
        }),
        dc::exit()
      })),
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        auto f = greater<double>();
        auto g = triangArea(s.I,s.P,s.l,s.r);
        return encore_call<maxIndex<double,intT,typeof(f),typeof(g)>>(st, pt, 0, s.n, f, g, &s.idx);
      }),
      dc::stmt([] (sar& s, par&) {
        s.maxP = s.I[s.idx];
      }),
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        auto a = aboveLine(s.P, s.l, s.maxP);
        return encore_call<filterDPS<intT, intT, typeof(a)>>(st, pt, s.I, s.Itmp, s.n, a, &s.n1);
      }),
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        auto a = aboveLine(s.P, s.maxP, s.r);
        return encore_call<filterDPS<intT, intT, typeof(a)>>(st, pt, s.I, s.Itmp+s.n1, s.n, a, &s.n2);
      }),
      dc::spawn2_join(
        [] (sar& s, par&, plt pt, stt st) {
          return encore_call<quickHull>(st, pt, s.Itmp, s.I, s.P, s.n1, s.l, s.maxP, s.depth-1, &s.m1);
        },
        [] (sar& s, par&, plt pt, stt st) {
          return encore_call<quickHull>(st, pt, s.Itmp+s.n1, s.I+s.n1, s.P, s.n2, s.maxP, s.r, s.depth-1, &s.m2);
        }),
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        return encore_call<sequence::copy<intT*, intT*>>(st, pt, s.Itmp, s.Itmp + s.m1, s.I);
      }),
      dc::stmt([] (sar& s, par& p) {
        s.I[s.m1] = s.maxP;
      }),
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        auto Itmp = s.Itmp + s.n1;
        return encore_call<sequence::copy<intT*, intT*>>(st, pt, Itmp, Itmp + s.m2, s.I + s.m1 + 1);
      }),
      dc::stmt([] (sar& s, par& p) {
        *s.dest = s.m1 + 1 + s.m2;
      })
    });
  }
  
};

encore_pcfg_allocate(quickHull, get_cfg)

struct makePair {
  pair<intT,intT> operator () (intT i) { return pair<intT,intT>(i,i);}
};

struct minMaxIndex {
  point2d* P;
  minMaxIndex (point2d* _P) : P(_P) {}
  pair<intT,intT> operator () (pair<intT,intT> l, pair<intT,intT> r) {
    intT minIndex =
    (P[l.first].x < P[r.first].x) ? l.first :
    (P[l.first].x > P[r.first].x) ? r.first :
    (P[l.first].y < P[r.first].y) ? l.first : r.first;
    intT maxIndex = (P[l.second].x > P[r.second].x) ? l.second : r.second;
    return pair<intT,intT>(minIndex, maxIndex);
  }
};

class hull : public encore::edsl::pcfg::shared_activation_record {
public:
  
  point2d* P; intT n;
  _seq<intT>* dest;
  intT l; intT r; bool* fTop; bool* fBot; intT* I; intT* Itmp; intT n1; intT n2;
  intT m1; intT m2;
  _seq<intT> tmp;
  pair<intT,intT> minMax;
  
  hull(point2d* P, intT n, _seq<intT>* dest)
  : P(P), n(n), dest(dest) { }
  
  encore_private_activation_record_begin(encore::edsl, hull, 1)
    intT s; intT e;
  encore_private_activation_record_end(encore::edsl, hull, sar, par, dc, get_dc)
  
  static
  dc get_dc() {
    return dc::stmts({
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        auto f = minMaxIndex(s.P);
        auto g = makePair();
        return encore_call<reduce<pair<intT,intT>, intT, typeof(f), typeof(g)>>(st, pt, 0, s.n, f, g, &s.minMax);
      }),
      dc::stmt([] (sar& s, par& p) {
        s.l = s.minMax.first;
        s.r = s.minMax.second;
        s.fTop = malloc_array<bool>(s.n);
        s.fBot = malloc_array<bool>(s.n);
        s.I = malloc_array<intT>(s.n);
        s.Itmp = malloc_array<intT>(s.n);
        p.s = 0;
        p.e = s.n;
      }),
      dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
        dc::stmt([] (sar& s, par& p) {
          s.Itmp[p.s] = p.s;
          double a = triangle_area(s.P[s.l], s.P[s.r], s.P[p.s]);
          s.fTop[p.s] = a > 0;
          s.fBot[p.s] = a < 0;
          p.s++;
        })),
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        return pack5(st, pt, s.Itmp, s.I, s.fTop, s.n, &s.tmp);
      }),
      dc::stmt([] (sar& s, par& p) {
        s.n1 = (intT)s.tmp.n;
      }),
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        return pack5(st, pt, s.Itmp, s.I + s.n1, s.fBot, s.n, &s.tmp);
      }),
      dc::stmt([] (sar& s, par& p) {
        s.n2 = (intT)s.tmp.n;
        free(s.fTop);
        free(s.fBot);
      }),
      dc::spawn2_join(
        [] (sar& s, par&, plt pt, stt st) {
          return encore_call<quickHull>(st, pt, s.I, s.Itmp, s.P, s.n1, s.l, s.r, 5, &s.m1);
        },
        [] (sar& s, par&, plt pt, stt st) {
          return encore_call<quickHull>(st, pt, s.I + s.n1, s.Itmp + s.n1, s.P, s.n2, s.r, s.l, 5, &s.m2);
        }),
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        return encore_call<sequence::copy<intT*, intT*>>(st, pt, s.I, s.I + s.m1, s.Itmp + 1);
      }),
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        auto I = s.I + s.n1;
        return encore_call<sequence::copy<intT*, intT*>>(st, pt, I, I + s.m2, s.Itmp + s.m1 + 2);
      }),
      dc::stmt([] (sar& s, par& p) {
        free(s.I);
        s.Itmp[0] = s.l;
        s.Itmp[s.m1 + 1] = s.r;
        *s.dest = _seq<intT>(s.Itmp, s.m1 + 2 + s.m2);
      })
    });
  }
  
};

encore_pcfg_allocate(hull, get_cfg)

namespace pasl {
namespace pctl {

/*---------------------------------------------------------------------*/
/* Quickcheck IO */

template <class Container>
std::ostream& operator<<(std::ostream& out, const pasl::pctl::container_wrapper<Container>& c) {
  out << c.c;
  return out;
}

/*---------------------------------------------------------------------*/
/* Quickcheck generators */

void generate(size_t _nb, parray<point2d>& dst) {
  intT nb = (intT)_nb;
  if (quickcheck::generateInRange(0, 1) == 0) {
    dst = plummer2d(nb);
  } else {
    bool inSphere = quickcheck::generateInRange(0, 1) == 0;
    bool onSphere = quickcheck::generateInRange(0, 1) == 0;
    dst = uniform2d(inSphere, onSphere, nb);
  }
}

void generate(size_t nb, container_wrapper<parray<point2d>>& c) {
  generate(nb, c.c);
}

/*---------------------------------------------------------------------*/
/* Quickcheck properties */

struct getX {
  point2d* P;
  getX(point2d* _P) : P(_P) {}
  double operator() (intT i) {return P[i].x;}
};

struct lessX {bool operator() (point2d a, point2d b) {
  return (a.x < b.x) ? 1 : (a.x > b.x) ? 0 : (a.y < b.y);} };

bool equals(point2d a, point2d b) {
  return (a.x == b.x) && (a.y == b.y);
}

//    if (f(v,r)) { r = v; k = j;}
bool check_hull(parray<point2d>& points, parray<intT> indices) {
  point2d* p = points.begin();
  intT n = (intT)points.size();
  intT hull_size = (intT)indices.size();
  point2d* convex_hull = newA(point2d, hull_size);
  for (intT i = 0; i < hull_size; i++) {
    convex_hull[i] = p[indices[i]];
  }
  intT idx = 0;
  for (int i = 0; i < hull_size; i++) {
    if (convex_hull[i].x > convex_hull[idx].x ||
        (convex_hull[i].x == convex_hull[idx].x && convex_hull[i].y > convex_hull[idx].y)) {
      idx = i;
    }
  }
  std::sort(p, p + n, lessX());
  if (!equals(p[0], convex_hull[0])) {
    cout << "checkHull: bad leftmost point" << endl;
    p[0].print();  convex_hull[0].print(); cout << endl;
    return 1;
  }
  if (!equals(p[n - 1], convex_hull[idx])) {
    cout << "checkHull: bad rightmost point" << endl;
    return 1;
  }
  intT k = 1;
  for (intT i = 0; i < idx; i++) {
    if (i > 0 && counter_clockwise(convex_hull[i - 1], convex_hull[i], convex_hull[i + 1])) {
      cout << "checkHull: not convex sides" << endl;
      return 1;
    }
    if (convex_hull[i].x > convex_hull[i + 1].x) {
      cout << "checkHull: upper hull not sorted by x" << endl;
      cout << indices << endl;
      for (int i = 0; i < hull_size; i++) {
        cout << convex_hull[i] << " ";
      }
      cout << std::endl;
      return 1;
    }
    while (k < n && !equals(p[k], convex_hull[i + 1]))
      if (counter_clockwise(convex_hull[i], convex_hull[i + 1], p[k++])) {
        cout << "checkHull: not convex" << endl;
        return 1;
      }
    if (k == n) {
      cout << "checkHull: unexpected points in hull" << endl;
      return 1;
    }
    k++;
  }
  k = n - 2;
  for (intT i = idx; i < hull_size - 1; i++) {
    if (i > idx && counter_clockwise(convex_hull[i - 1], convex_hull[i], convex_hull[i + 1])) {
      cout << "checkHull: not convex sides" << endl;
      return 1;
    }
    if (convex_hull[i].x < convex_hull[i + 1].x) {
      cout << "checkHull: lower hull not sorted by x" << endl;
      return 1;
    }
    while (k >= 0 && !equals(p[k], convex_hull[i + 1])) {
      if (counter_clockwise(convex_hull[i], convex_hull[i + 1], p[k--])) {
        cout << "checkHull: not convex" << endl;
      }
    }
    k--;
  }
  free(convex_hull);
  return 0;
}


using parray_wrapper = container_wrapper<parray<point2d>>;

class consistent_hulls_property : public quickcheck::Property<parray_wrapper> {
public:
  
  bool holdsFor(const parray_wrapper& _in) {
    parray_wrapper in(_in);
    _seq<intT> idxs;
    encore::launch_interpreter<hull>(in.c.begin(), in.c.size(), &idxs);
    parray<intT> idxs2(idxs.A, idxs.A + idxs.n);
    idxs.del();
    return ! check_hull(in.c, idxs2);
  }
  
};

/*---------------------------------------------------------------------*/
/* Benchmarking */

parray<pbbs::_point2d<double>> to_pbbs(parray<pasl::pctl::_point2d<double>>& points) {
  parray<pbbs::_point2d<double>> result(points.size());
  for (int i = 0; i < points.size(); i++) {
    result[i] = pbbs::_point2d<double>(points[i].x, points[i].y);
  }
  return result;
}


void benchmark(std::string infile) {
  parray<point2d> x = io::load<parray<point2d>>(infile);
  std::string algorithm = cmdline::parse<std::string>("algorithm");
  deepsea::cmdline::dispatcher d;
  d.add("encore", [&] {
    _seq<intT> idxs;
    encore::launch_interpreter<hull>(x.begin(), x.size(), &idxs);
    idxs.del();
  });
  d.add("pbbs", [&] {
    encore::run_and_report_elapsed_time([&] {
      parray<pbbs::_point2d<double>> y = to_pbbs(x);
      pbbs::hull(y.begin(), (int)y.size());
    });
  });
  d.dispatch("algorithm"); 
}

} // end namespace
} // end namespace

int main(int argc, char** argv) {
  encore::initialize(argc, argv);
  quickHull_threshold = deepsea::cmdline::parse_or_default("threshold", quickHull_threshold);
  std::string infile = deepsea::cmdline::parse_or_default_string("infile", "");
  if (infile != "") {
    pasl::pctl::benchmark(infile);
    return 0;
  }
  int nb_tests = cmdline::parse_or_default_int("nb_tests", 1000);
  checkit<pasl::pctl::consistent_hulls_property>(nb_tests, "quickhull is correct");
  return 0;
}
