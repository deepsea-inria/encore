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
#include <limits>

#include "sequence.hpp"
#include "blockradixsort.hpp"
#include "utils.h"
#include "encore.hpp"
#include "octtree.hpp"

namespace encorebench {

// A k-nearest neighbor structure
// requires vertexT to have pointT and vectT typedefs
template <class vertexT, int maxK>
struct kNearestNeighbor {
  typedef vertexT vertex;
  typedef typename vertexT::pointT point;
  typedef typename point::vectT fvect;

  typedef gTreeNode<point,fvect,vertex> qoTree;
  typedef gTreeNodeConstructor2<point,fvect,vertex, typename qoTree::ndty> qoTreeCons;
  qoTree *tree;

  void del() {tree->del();}

  struct kNN {
    vertex *ps;  // the element for which we are trying to find a NN
    vertex *pn[maxK];  // the current k nearest neighbors (nearest last)
    double rn[maxK]; // radius of current k nearest neighbors
    int quads;
    int k;
    kNN() {}

    // returns the ith smallest element (0 is smallest) up to k-1
    vertex* operator[] (const int i) { return pn[k-i-1]; }

    kNN(vertex *p, int kk) {
      if (kk > maxK) {cout << "k too large in kNN" << endl; abort();}
      k = kk;
      quads = (1 << (p->pt).dimension());
      ps = p;
      for (int i=0; i<k; i++) {
        pn[i] = (vertex*) NULL; 
        rn[i] = numeric_limits<double>::max();
      }
    }

    // if p is closer than pn then swap it in
    void update(vertex *p) { 
      //inter++;
      point opt = (p->pt);
      fvect v = (ps->pt) - opt;
      double r = v.length();
      if (r < rn[0]) {
        pn[0]=p; rn[0] = r;
        for (int i=1; i < k && rn[i-1]<rn[i]; i++) {
          std::swap(rn[i-1],rn[i]); std::swap(pn[i-1],pn[i]); }
      }
    }

    // looks for nearest neighbors in boxes for which ps is not in
    void nearestNghTrim(qoTree *T) {
      if (!(T->center).out_of_box(ps->pt, (T->size/2)+rn[0])) {
        if (T->IsLeaf())
          for (int i = 0; i < T->count; i++) update(T->vertices[i]);
        else 
          for (int j=0; j < quads; j++) nearestNghTrim(T->children[j]);
      }
    }

    // looks for nearest neighbors in box for which ps is in
    void nearestNgh(qoTree *T) {
      if (T->IsLeaf())
        for (int i = 0; i < T->count; i++) {
          vertex *pb = T->vertices[i];
          if (pb != ps) update(pb);
        }
      else {
        int i = T->findQuadrant(ps);
        nearestNgh(T->children[i]);
        for (int j=0; j < quads; j++) 
          if (j != i) nearestNghTrim(T->children[j]);
      }
    }
  };

  vertex* nearest(vertex *p) {
    kNN nn(p,1);
    nn.nearestNgh(tree); 
    return nn[0];
  }

  // version that writes into result
  void kNearest(vertex *p, vertex** result, int k) {
    kNN nn(p,k);
    nn.nearestNgh(tree); 
    for (int i=0; i < k; i++) result[i] = 0;
    for (int i=0; i < k; i++) result[i] = nn[i];
  }

  // version that allocates result
  vertex** kNearest(vertex *p, int k) {
    vertex** result = newA(vertex*,k);
    kNearest(p,result,k);
    return result;
  }

};

// find the k nearest neighbors for all points in tree
// places pointers to them in the .ngh field of each vertex
template <int maxK, class vertexT>
class ANN : public encore::edsl::pcfg::shared_activation_record {
public:

  typedef kNearestNeighbor<vertexT,maxK> kNNT;

  vertexT** v; int n; int k;
  kNNT T; vertexT** vr;

  ANN(vertexT** v, int n, int k)
    : v(v), n(n), k(k) { } 
  
  encore_private_activation_record_begin(encore::edsl, ANN, 1)
    int s; int e;
  encore_private_activation_record_end(encore::edsl, ANN, sar, par, dc, get_dc)

  static
  dc get_dc() {
    return dc::stmts({
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        using gtc = typename kNNT::qoTreeCons;
        return encore_call<gtc>(st, pt, s.v, s.n, &(s.T.tree));
      }),
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        // this reorders the vertices for locality          
        return flatten(st, pt, s.T.tree, &(s.vr));
      }),
      // find nearest k neighbors for each point
      dc::parallel_for_loop([] (sar& s, par& p) { p.s = 0; p.e = s.n;},
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
                            [] (sar& s, par& p, int lo, int hi) {
        auto& T = s.T;
        auto vr = s.vr;
        auto k = s.k;
        for (auto i = lo; i != hi; i++) {
          T.kNearest(vr[i], vr[i]->ngh, k);
        }
      }),
      dc::stmt([] (sar& s, par& p) {
        free(s.vr);
        s.T.del();
      })
    });
  }
  
};

template <int maxK, class vertexT>
typename ANN<maxK,vertexT>::cfg_type ANN<maxK,vertexT>::cfg = ANN<maxK,vertexT>::get_cfg();

template <class PT, int KK>
struct vertexNN {
  typedef PT pointT;
  int identifier;
  pointT pt;         // the point itself
  vertexNN* ngh[KK];    // the list of neighbors
  vertexNN(pointT p, int id) : pt(p), identifier(id) {}
};
  
template <int maxK, class pointT>
class findNearestNeighbors : public encore::edsl::pcfg::shared_activation_record {
public:

  typedef vertexNN<pointT, maxK> vertex;
  
  pointT* p; int n; int k;
  vertex** v; vertex* vv; intT* result;

  findNearestNeighbors(pointT* p, int n, int k)
    : p(p), n(n), k(k) { } 
  
  encore_private_activation_record_begin(encore::edsl, findNearestNeighbors, 2)
    int s; int e;
  encore_private_activation_record_end(encore::edsl, findNearestNeighbors, sar, par, dc, get_dc)

  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        s.v = malloc_array<vertex*>(s.n);
        s.vv = malloc_array<vertex>(s.n);
      }),
      dc::parallel_for_loop([] (sar& s, par& p) { p.s = 0; p.e = s.n; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
                            [] (sar& s, par& p, int lo, int hi) {
        auto v = s.v;
        auto vv = s.vv;
        auto _p = s.p;
        for (auto i = lo; i != hi; i++) {
          v[i] = new (&vv[i]) vertex(_p[i],i);
        }
      }),
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        return encore_call<ANN<maxK, vertex>>(st, pt, s.v, s.n, s.k);
      }),
      dc::stmt([] (sar& s, par& p) {
        s.result = malloc_array<intT>(s.n * s.k);
      }),
      dc::parallel_for_loop([] (sar& s, par& p) { p.s = 0; p.e = s.n; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
                            [] (sar& s, par& p, int lo, int hi) {
        auto result = s.result;
        auto v = s.v;
        auto k = s.k;
        auto n = s.n;
        for (auto i = lo; i != hi; i++) {
          for (int j = 0; j < std::min(k, n - 1); j++) {
            result[i * k + j] = v[i]->ngh[j]->identifier;
          }
        }
      }),
      dc::stmt([] (sar& s, par& p) {
        free(s.result);
      })
    });
  }
  
};

template <int maxK, class pointT>
typename findNearestNeighbors<maxK,pointT>::cfg_type findNearestNeighbors<maxK,pointT>::cfg = findNearestNeighbors<maxK,pointT>::get_cfg();

} // end namespace

#include "nearestNeighbors.h"
#undef blocked_for
#undef parallel_for
#include "loaders.hpp"

namespace pasl {
namespace pctl {

parray<pbbs::_point2d<double>> to_pbbs(parray<_point2d<double>>& points) {
  parray<pbbs::_point2d<double>> result(points.size());
  for (int i = 0; i < points.size(); i++) {
    result[i] = pbbs::_point2d<double>(points[i].x, points[i].y);
  }
  return result;
}

parray<pbbs::_point3d<double>> to_pbbs(parray<_point3d<double>>& points) {
  parray<pbbs::_point3d<double>> result(points.size());
  for (int i = 0; i < points.size(); i++) {
    result[i] = pbbs::_point3d<double>(points[i].x, points[i].y, points[i].z);
  }
  return result;
}
  
template <class Item1, class Item2, int K>
void benchmark(parray<Item1>& x, int k) {
  std::string algorithm = deepsea::cmdline::parse<std::string>("algorithm");
  deepsea::cmdline::dispatcher d;
  std::pair<intT*, intT*> res;
  d.add("encore", [&] {
    encore::launch_interpreter<encorebench::findNearestNeighbors<K, Item1>>(x.begin(), (int)x.size(), k);
  });
  d.add("pbbs", [&] {
    parray<Item2> y = to_pbbs(x);
    encore::run_and_report_elapsed_time([&] {
      pbbs::findNearestNeighbors<K, Item2>(&y[0], (int)y.size(), k);
    });
  });
  d.dispatch("algorithm"); 
}

void benchmark(std::string infile) {
  int k = deepsea::cmdline::parse_or_default_int("k", 1);
  deepsea::cmdline::dispatcher d;
  d.add("array_point2d", [&] {
    parray<_point2d<double>> x = io::load<parray<_point2d<double>>>(infile);
    benchmark<_point2d<double>, pbbs::_point2d<double>, 10>(x, k);
  });
  d.add("array_point3d", [&] {
    parray<_point3d<double>> x = io::load<parray<_point3d<double>>>(infile);
    benchmark<_point3d<double>, pbbs::_point3d<double>, 10>(x, k);
  });                                                                           
  d.dispatch("type");
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
