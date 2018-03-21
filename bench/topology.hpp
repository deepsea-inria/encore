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
#include "geometry.hpp"
#include "deterministichash.hpp"

#ifndef ENCORE__TOPOLOGY_INCLUDED
#define ENCORE__TOPOLOGY_INCLUDED

namespace encorebench {
using namespace std;

using point2d = sptl::point2d;

// *************************************************************
//    TOPOLOGY
// *************************************************************

struct vertex;

// an unoriented triangle with its three neighbors and 3 vertices
//          vtx[1]
//           o 
//           | \ -> ngh[1]
// ngh[2] <- |   o vtx[0]
//           | / -> ngh[0]
//           o
//         vtx[2]
struct tri {
  tri *ngh [3];
  vertex *vtx [3];
  intT id;
  bool initialized;
  char bad;  // used to mark badly shaped triangles
  void setT(tri *t1, tri *t2, tri* t3) {
    ngh[0] = t1; ngh[1] = t2; ngh[2] = t3; }
  void setV(vertex *v1, vertex *v2, vertex *v3) {
    vtx[0] = v1; vtx[1] = v2; vtx[2] = v3; }
  int locate(tri *t) {
    for (int i=0; i < 3; i++)
      if (ngh[i] == t) return i;
    cout<<"did not locate back pointer in triangulation\n";
    abort(); // did not find
  }
  void update(tri *t, tri *tn) {
    for (int i=0; i < 3; i++)
      if (ngh[i] == t) {ngh[i] = tn; return;}
    cout<<"did not update\n";
    abort(); // did not find
  }
};

// a vertex pointing to an arbitrary triangle to which it belongs (if any)
struct vertex {
  typedef point2d pointT;
  point2d pt;
  tri *t;
  tri *badT;
  intT id;
  intT reserve;
  void print() {
    cout << id << " (" << pt.x << "," << pt.y << ") " << endl;
  }
  vertex(point2d p, intT i) : pt(p), id(i), reserve(-1)
			   , badT(NULL)
  {}
};

inline int mod3(int i) {return (i>2) ? i-3 : i;}

// a simplex is just an oriented triangle.  An integer (o)
// is used to indicate which of 3 orientations it is in (0,1,2)
// If boundary is set then it represents the edge through t.ngh[o],
// which is a NULL pointer.
struct simplex {
  tri *t;
  int o;
  bool boundary;
  simplex() { }
  simplex(tri *tt, int oo) : t(tt), o(oo), boundary(0) {}
  simplex(tri *tt, int oo, bool _b) : t(tt), o(oo), boundary(_b) {}
  simplex(vertex *v1, vertex *v2, vertex *v3, tri *tt) {
    t = tt;
    t->ngh[0] = t->ngh[1] = t->ngh[2] = NULL;
    t->vtx[0] = v1; v1->t = t;
    t->vtx[1] = v2; v2->t = t;
    t->vtx[2] = v3; v3->t = t;
    o = 0;
    boundary = 0;
  }

  void print() {
    if (t == NULL) cout << "NULL simp" << endl;
    else {
      cout << "vtxs=";
      for (int i=0; i < 3; i++) 
        if (t->vtx[mod3(i+o)] != NULL)
          cout << t->vtx[mod3(i+o)]->id << " (" <<
            t->vtx[mod3(i+o)]->pt.x << "," <<
            t->vtx[mod3(i+o)]->pt.y << ") ";
        else cout << "NULL ";
      cout << endl;
    }
  }

  simplex across() {
    tri *to = t->ngh[o];
    if (to != NULL) return simplex(to,to->locate(t));
    else return simplex(t,o,1);
  }

  // depending on initial triangle this could be counterclockwise
  simplex rotClockwise() { return simplex(t,mod3(o+1));}

  bool valid() {return (!boundary);}
  bool isTriangle() {return (!boundary);}
  bool isBoundary() {return boundary;}
  
  vertex *firstVertex() {return t->vtx[o];}

  bool inCirc(vertex *v) {
    if (boundary || t == NULL) return 0;
    return inCircle(t->vtx[0]->pt, t->vtx[1]->pt, 
		    t->vtx[2]->pt, v->pt);
  }

  // the angle facing the across edge
  double farAngle() {
    return angle(t->vtx[mod3(o+1)]->pt,
		 t->vtx[o]->pt,
		 t->vtx[mod3(o+2)]->pt);
  }

  bool outside(vertex *v) {
    if (boundary || t == NULL) return 0;
    return counter_clockwise(t->vtx[mod3(o+2)]->pt, v->pt, t->vtx[o]->pt);
  }

  // flips two triangles and adjusts neighboring triangles
  void flip() { 
    simplex s = across();
    int o1 = mod3(o+1);
    int os1 = mod3(s.o+1);

    tri *t1 = t->ngh[o1];
    tri *t2 = s.t->ngh[os1];
    vertex *v1 = t->vtx[o1];
    vertex *v2 = s.t->vtx[os1];

    t->vtx[o]->t = s.t;
    t->vtx[o] = v2;
    t->ngh[o] = t2;
    if (t2 != NULL) t2->update(s.t,t);
    t->ngh[o1] = s.t;

    s.t->vtx[s.o]->t = t;
    s.t->vtx[s.o] = v1;
    s.t->ngh[s.o] = t1;
    if (t1 != NULL) t1->update(t,s.t);
    s.t->ngh[os1] = t;
  }

  // splits the triangle into three triangles with new vertex v in the middle
  // updates all neighboring simplices
  // ta0 and ta0 are pointers to the memory to use for the two new triangles
  void split(vertex* v, tri* ta0, tri* ta1) {
    v->t = t;
    tri *t1 = t->ngh[0]; tri *t2 = t->ngh[1]; tri *t3 = t->ngh[2];
    vertex *v1 = t->vtx[0]; vertex *v2 = t->vtx[1]; vertex *v3 = t->vtx[2];
    t->ngh[1] = ta0;        t->ngh[2] = ta1;
    t->vtx[1] = v;
    ta0->setT(t2,ta1,t);  ta0->setV(v2,v,v1);
    ta1->setT(t3,t,ta0);  ta1->setV(v3,v,v2);
    if (t2 != NULL) t2->update(t,ta0);      
    if (t3 != NULL) t3->update(t,ta1);
    v2->t = ta0;
  }

  // splits one of the boundaries of a triangle to form two triangles
  // the orientation dictates which edge to split (i.e., t.ngh[o])
  // ta is a pointer to memory to use for the new triangle
  void splitBoundary(vertex* v, tri* ta) {
    int o1 = mod3(o+1);
    int o2 = mod3(o+2);
    if (t->ngh[o] != NULL) {
      cout << "simplex::splitBoundary: not boundary" << endl; abort();}
    v->t = t;
    tri *t2 = t->ngh[o2];
    vertex *v1 = t->vtx[o1]; vertex *v2 = t->vtx[o2];
    t->ngh[o2] = ta;   t->vtx[o2] = v;
    ta->setT(t2,NULL,t);  ta->setV(v2,v,v1);
    if (t2 != NULL) t2->update(t,ta);      
    v2->t = t;
  }

  // given a vertex v, extends a boundary edge (t.ngh[o]) with an extra 
  // triangle on that edge with apex v.  
  // ta is used as the memory for the triangle
  simplex extend(vertex* v, tri* ta) {
    if (t->ngh[o] != NULL) {
      cout << "simplex::extend: not boundary" << endl; abort();}
    t->ngh[o] = ta;
    ta->setV(t->vtx[o], t->vtx[mod3(o+2)], v);
    ta->setT(NULL,t,NULL);
    v->t = ta;
    return simplex(ta,0);
  }

};

typedef pair<intT, intT> pairInt;
typedef pair<pairInt, tri*> edge;

// Hash table to store skinny triangles
struct hashEdges {
  typedef pairInt kType;
  typedef edge* eType;
  eType empty() {return NULL;}
  kType getKey(eType v) { return v->first;}
  uintT hash(kType s) { return pbbs::utils::hash(s.first)+3*(pbbs::utils::hash(s.second)); }
  int cmp(kType s1, kType s2) {
    return ((s1.first > s2.first) ? 1 : 
	    (s1.first < s2.first) ? -1 : 
	    (s1.second > s2.second) ? 1 :
	    (s1.second < s2.second) ? -1 : 0);
  }
  bool replaceQ(eType s, eType s2) {return 0;}
};

typedef Table<hashEdges,intT> EdgeTable;
EdgeTable makeEdgeTable(intT m) {return EdgeTable(m,hashEdges());}

template <class point>
using triangles = sptl::triangles<point>;

using triangle = sptl::triangle;
  
class topologyFromTriangles : public encore::edsl::pcfg::shared_activation_record {
public:

  triangles<point2d> Tri; vertex** vr; tri** tr;
  intT n; point2d* P; intT m; triangle* T; vertex* v;
  tri* Triangs; edge* E; EdgeTable ET; 
  typedef typename hashEdges::eType eType;
  eType empty;

  topologyFromTriangles(triangles<point2d> Tri, vertex** vr, tri** tr)
    : Tri(Tri), vr(vr), tr(tr) { }
  
  encore_private_activation_record_begin(encore::edsl, topologyFromTriangles, 3)
    int lo; int hi;
  encore_private_activation_record_end(encore::edsl, topologyFromTriangles, sar, par, dc, get_dc)

  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        s.n = s.Tri.num_points;
        s.P = s.Tri.p;

        s.m = s.Tri.num_triangles;
        s.T = s.Tri.t;

        if (*s.vr == NULL) {
          *s.vr = malloc_array<vertex>(s.n);
        }
        s.v = *s.vr;
      }),
      dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.n;},
                            [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                            [] (sar& s, par& p, int lo, int hi) {
        auto v = s.v;
        auto P = s.P;
        for (auto i = lo; i != hi; i++) {
          v[i] = vertex(P[i],i);
        }
      }),
      dc::stmt([] (sar& s, par& p) {
        if (*s.tr == NULL) {
          *s.tr = malloc_array<tri>(s.m);
        }
        s.Triangs = *s.tr;
        s.E = malloc_array<edge>(s.m*3);
        new (&s.ET) EdgeTable(s.m * 6, hashEdges());
      }),
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        return sequence::fill3(st, pt, s.ET.TA, s.ET.TA + s.ET.m, &(s.ET.empty));
      }),
      dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.m;},
                            [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                            [] (sar& s, par& p, int lo, int hi) {
        auto Triangs = s.Triangs;
        auto E = s.E;
        auto T = s.T;
        auto v = s.v;
        auto& ET = s.ET;
        for (auto i = lo; i != hi; i++) {
          Triangs[i] = tri();
          for (int j=0; j<3; j++) {
            E[i*3 + j] = edge(pairInt(T[i].vertices[j], T[i].vertices[(j+1)%3]), &Triangs[i]);
            ET.insert(&E[i*3+j]);
            Triangs[i].vtx[(j+2)%3] = &v[T[i].vertices[j]];
          }
        }
      }),
      dc::parallel_for_loop([] (sar& s, par& p) { p.lo = 0; p.hi = s.m;},
                            [] (par& p) { return std::make_pair(&p.lo, &p.hi); },
                            [] (sar& s, par& p, int lo, int hi) {
        auto Triangs = s.Triangs;
        auto E = s.E;
        auto T = s.T;
        auto v = s.v;
        auto& ET = s.ET;
        for (auto i = lo; i != hi; i++) {
          Triangs[i].id = i;
          Triangs[i].initialized = 1;
          Triangs[i].bad = 0;
          for (int j=0; j<3; j++) {
            pairInt key = pairInt(T[i].vertices[(j+1)%3], T[i].vertices[j]);
            edge *Ed = ET.find(key);
            if (Ed != NULL) Triangs[i].ngh[j] = Ed->second;
            else { Triangs[i].ngh[j] = NULL;
            }
          }
        }
      }),
      dc::stmt([] (sar& s, par& p) {
        s.ET.del();
        free(s.E);
      })
    });
  }
  
};

encore_pcfg_allocate(topologyFromTriangles, get_cfg)
    
} // end namespace
#endif // _TOPOLOGY_INCLUDED
