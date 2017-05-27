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

// Mostly written by Jeremy Fineman

#include <iostream>
#include <cstdlib>
#include "sequence.hpp"
#include "geometry.hpp"
#include "blockradixsort.hpp"
#include "quicksort.hpp"

namespace encorebench {

// *************************************************************
//    QUAD/OCT TREE NODES
// *************************************************************

#define gMaxLeafSize 16  // number of points stored in each leaf
#define GTREE_BASE_CASE 65536
#define GTREE_SEQSORT_BASE 1000
#define GTREE_SUBPROB_POW .92
#define GTREE_ALLOC_FACTOR 2/gMaxLeafSize

// minpt: topleft of the square
// blocksize: length of each edge of a quadrant
// numblocks: number of quadrants in each direction
// p: the point to assign to a block
// return the index of the quadrant for the point p
int ptFindBlock(point2d minpt, double blocksize, int log_numblocks, point2d p) {
  int numblocks = (1 << log_numblocks);
  int xindex = min((int)((p.x - minpt.x)/blocksize),numblocks-1);  
  int yindex = min((int)((p.y - minpt.y)/blocksize),numblocks-1);  
  int result = 0;
  for (int i = 0; i < log_numblocks; i++) {
    int mask = (1 << i);
    result += (xindex & mask) << i;
    result += (yindex & mask) << (i+1);
  }
  return result;
}

// minpt: topleft of the cube
// blocksize: length of each edge of a quadrant
// numblocks: number of quadrants in each direction
// p: the point to assign to a block
// return the index of the quadrant for the point p
int ptFindBlock(point3d minpt, double blocksize, int log_numblocks, point3d p) {
  int numblocks = (1 << log_numblocks);
  int xindex = min((int)((p.x - minpt.x)/blocksize),numblocks-1);  
  int yindex = min((int)((p.y - minpt.y)/blocksize),numblocks-1);  
  int zindex = min((int)((p.z - minpt.z)/blocksize),numblocks-1);  
  int result = 0;
  for (int i = 0; i < log_numblocks; i++) {
    int mask = (1 << i);
    result += (xindex & mask) << (2*i);
    result += (yindex & mask) << (2*i+1);
    result += (zindex & mask) << (2*i+2);
  }
  return result;
}

// minpt: topleft of the square
// blocksize: length of each edge of a quadrant
// log_numblocks: log_2(number of quadrants in each direction)
// index: the index of this quadrant
// return the center point of this quadrant
point2d ptMakeBlock(point2d minpt, double blocksize, int log_numblocks, int index) {
  int xindex = 0;
  int yindex = 0;
  for (int i = 0; i < log_numblocks; i++) {
    xindex |= ((index & 1) << i);
    yindex |= ((index & 2) << i);
    index = (index >> 2);
  }
  yindex = yindex >> 1;
  double xoff = (double)xindex * blocksize;
  double yoff = (double)yindex * blocksize;
  return point2d(minpt.x + xoff,minpt.y +yoff).offsetPoint(7,blocksize/2);
}

// minpt: topleft of the cube
// blocksize: length of each edge of a quadrant
// log_numblocks: log_2(number of quadrants in each direction)
// index: the index of this quadrant
// return the center point of this quadrant
point3d ptMakeBlock(point3d minpt, double blocksize, int log_numblocks, int index) {
  int xindex = 0;
  int yindex = 0;
  int zindex = 0;
  for (int i=0; i<log_numblocks; i++) {
    xindex |= ((index & 1) << i);
    yindex |= ((index & 2) << i);
    zindex |= ((index & 4) << i);
    index = (index >> 3);
  }
  yindex = (yindex >> 1);
  zindex = (zindex >> 2);
  double xoff = (double)xindex * blocksize;
  double yoff = (double)yindex * blocksize;
  double zoff = (double)zindex * blocksize;
  return point3d(minpt.x+xoff,minpt.y+yoff,minpt.z+zoff).offsetPoint(7,blocksize/2);
}

template <class vertex>
struct nData {
  int cnt;
  nData(int x) : cnt(x) {}
  nData() : cnt(0) {}
  nData operator+(nData op2) {return nData(cnt+op2.cnt);}
  nData operator+(vertex* op2) {return nData(cnt+1);}
};

template <class pointT, class vectT, class vertexT, class nodeData = nData<vertexT> >
class gTreeNode {
public :
  typedef pointT point;
  typedef vectT fvect;
  typedef vertexT vertex;
  typedef pair<point,point> pPair;

  // used for reduce to find bounding box
  struct minMax {
    pPair operator() (pPair a, pPair b) {
      return pPair((a.first).minCoords(b.first),
		   (a.second).maxCoords(b.second));}};

  struct toPair {
    pPair operator() (vertex* v) {
      return pPair(v->pt,v->pt);}};

  point center; // center of the box
  double size;   // width of each dimension, which have to be the same
  nodeData data; // total mass of vertices in the box
  int count;  // number of vertices in the box
  gTreeNode *children[8];
  vertex **vertices;
  gTreeNode *nodeMemory;

  static vertex** wv;

  int IsLeaf() { return (vertices != NULL); }

  void del() {
    free(wv);
    wv = NULL;
    del_recursive();
  }

  void del_recursive() {
    if (IsLeaf()) ; //delete [] vertices;
    else {
      for (int i=0 ; i < (1 << center.dimension()); i++) {
        children[i]->del();
        //	delete children[i];
      }
    }
    if (nodeMemory != NULL) free(nodeMemory);
  }

  // Returns the depth of the tree rooted at this node
  int Depth() {
    int depth;
    if (IsLeaf()) depth = 0;
    else {
      depth = 0;
      for (int i=0 ; i < (1 << center.dimension()); i++)
        depth = max(depth,children[i]->Depth());
    }
    return depth+1;
  }

  // Returns the size of the tree rooted at this node
  int Size() {
    int sz;
    if (IsLeaf()) {
      sz = count;
      for (int i=0; i < count; i++) 
        if (vertices[i] < ((vertex*) NULL)+1000) 
          cout << "oops: " << vertices[i] << "," << count << "," << i << endl;
    }
    else {
      sz = 0;
      for (int i=0 ; i < (1 << center.dimension()); i++)
        sz += children[i]->Size();
    }
    
    //cout << "node size = " << sz << "T->size" << " size=" << size << " center=";
    //center.print();
    //cout << endl;
    return sz;
  }

  struct flatten_FA {
    vertex** _A;
    flatten_FA(vertex** A) : _A(A) {}
    void operator() (vertex* p, int i) {
      //p->pt.print();
      _A[i] = p;}
  };

  vertex** flatten() {
    vertex** A = new vertex*[count];
    applyIndex(0,flatten_FA(A));
    return A;
  }

  // Returns the child the vertex p appears in
  int findQuadrant (vertex* p) {
    return (p->pt).quadrant(center); }

// A hack to get around Cilk shortcomings
  static gTreeNode *newTree(_seq<vertex*> S, point cnt, double sz, gTreeNode* newNodes, int numNewNodes) {
    utils::myAssert(numNewNodes > 0, "numNewNodes == 0\n");
    return new(newNodes) gTreeNode(S,cnt,sz,newNodes+1,numNewNodes-1);
  }

  struct compare {
    gTreeNode *tr;
    compare(gTreeNode *t) : tr(t) {}
    int operator() (vertex* p, vertex *q) {
      int x = tr->findQuadrant(p);
      int y = tr->findQuadrant(q);
      return (x < y);
    }
  };

  gTreeNode() { }

  gTreeNode(point cnt, double sz) 
  {
    count = 0;
    size = sz;
    center = cnt;
    vertices = NULL;
    nodeMemory = NULL;
  }

typedef pair<int,vertex*> pintv;

struct compFirst {
  bool operator()(pintv A, pintv B) {return A.first<B.first; }
};


};

template <class pointT, class vectT, class vertexT, class nodeData>
class gTree : public encore::edsl::pcfg::shared_activation_record {
public:
  
  using gtn = gTreeNode<pointT,vectT,vertexT,nodeData>;
  typedef pointT point;
  typedef vectT fvect;
  typedef vertexT vertex;
  typedef pair<point,point> pPair;

  vertex** vv; int n; gTreeNode* result;
  pPair minMaxPair; fvect box; point center;

  gTree(vertex** vv, int n, gTreeNode* result)
    : vv(vv), n(n), result(result) { }
  
  encore_dc_declare(encore::edsl, gTree, sar, par, dc, get_dc)

  static
  dc get_dc() {
    return dc::stmts({
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        return sequence::mapReduce(st, p, s.vv, s.n, minMax(), toPair(), &(s.minMaxPair));
      }),
      dc::stmt([] (sar& s, par& p) {
        point minPt = s.minMaxPair.first;
        point maxPt = s.minMaxPair.second;
        fvect box = s.box = maxPt-minPt;
        point center = s.center = minPt+(box/2.0);

        // copy before calling recursive routine since recursive routine is destructive
        gtn::wv = malloc_array<vertex*>(s.n);
      }),
      dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
        return encore_call<sequence::copy<vertex*, vertex*>>(st, pt, gtn::wv, gtn::wv + s.n, s.vv);
      }),
      dc::stmt([] (sar& s, par& p) {
        *result = new gTreeNode(_seq<vertex*>(gtn::wv,s.n),s.center,s.box.maxDim(),NULL,0);
      })
    });
  }
  
};

template <class pointT, class vectT, class vertexT, class nodeData>
typename gTree<pointT,vectT,vertexT,nodeData>::cfg_type gTree<pointT,vectT,vertexT,nodeData>::cfg = gTree<pointT,vectT,vertexT,nodeData>::get_cfg();

template <class F, class pointT, class vectT, class vertexT, class nodeData>
class applyIndex : public encore::edsl::pcfg::shared_activation_record {
public:

  using gtn = gTreeNode<pointT,vectT,vertexT,nodeData>;
  typedef pointT point;
  typedef vectT fvect;
  typedef vertexT vertex;
  typedef pair<point,point> pPair;

  int s; F f; gtn* g;
  int ss; int i;
  sched::incounter* join = nullptr;

  applyIndex(int s, F f, gtn* g)
    : s(s), f(f), g(g) { }
  
  encore_dc_declare(encore::edsl, applyIndex, sar, par, dc, get_dc)

  static
  dc get_dc() {
    return dc::mk_if([] (sar& s, par& p) {
      return s.g->IsLeaf();
    }, dc::stmt([] (sar& s, par& p) {
      auto count = s.g->count;
      auto vertices = s.g->vertices;
      auto _s = s.s;
      auto f = s.f;
      for (int i=0; i < count; i++) f(vertices[i],_s+i);
    }), dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        s.ss = s;
      }),
      dc::join_plus([] (sar& s, par&, plt pt, stt st) {
        dc::sequential_loop([] (sar& s, par&) { return (1 << s.g->center.dimension()); }, dc::stmts({
          dc::spawn_minus([] (sar& s, par&, plt pt, stt st) {
            return encore_call<applyIndex>(st, pt, s.ss, s.f, s.g->children[s.i]); },
            [] (sar& s, par&) { return &(s.join); }),
          dc::stmt([] (sar& s, par& p) {
            s.ss += s.g->children[s.i]->count;
            s.i++;
          })
        }))
        [] (sar& s, par&) { return &(s.join); }))
    }))
  }
  
};

template <class F, class pointT, class vectT, class vertexT, class nodeData>
typename applyIndex<F,pointT,vectT,vertexT,nodeData>::cfg_type applyIndex<F,pointT,vectT,vertexT,nodeData>::cfg = applyIndex<F,pointT,vectT,vertexT,nodeData>::get_cfg();

template <class pointT, class vectT, class vertexT, class nodeData>
class buildRecursiveTree : public encore::edsl::pcfg::shared_activation_record {
public:

  using gtn = gTreeNode<pointT,vectT,vertexT,nodeData>;
  typedef pointT point;
  typedef vectT fvect;
  typedef vertexT vertex;
  typedef pair<point,point> pPair;

  _seq<vertex*> S; int* offsets; int quadrants; gTreeNode *newNodes;
  gTreeNode* parent; int nodesToLeft; int height; int depth; gtn* g;
  int s; int e;

  buildRecursiveTree(_seq<vertex*> S, int* offsets, int quadrants, gTreeNode *newNodes,
                     gTreeNode* parent, int nodesToLeft, int height, int depth, gtn* g)
    : S(S), offsets(offsets), quadrants(quadrants), newNodes(newNodes),
      parent(parent), nodesToLeft(nodesToLeft), height(height), depth(depth), g(g) { }
  
  encore_private_activation_record_begin(encore::edsl, buildRecursiveTree, 2)
    int s; int e; int q;
  encore_private_activation_record_end(encore::edsl, buildRecursiveTree, sar, par, dc, get_dc)

  static
  dc get_dc() {
    return dc::stmts({
        dc::stmt([] (sar& s, par& p) {
          s.g->parent->count = 0;
        }),
        dc::mk_if([] (sar& s, par& p) {
          return s.height == 1;
        }, dc::stmts({
          dc::stmt([] (sar& s, par& p) {
             p.s = 0; p.e = (1<<s.center.dimension());
          }),
          dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                                [] (par& p) { return std::make_pair(&p.s, &p.e); },
                                dc::stmts({
            dc::stmt([] (sar& s, par& p) {
              int i = p.s;
              point newcenter = (s.parent->center).offsetPoint(s.i,s.parent->size/4.0);
              int q = p.q = (s.nodesToLeft<<s.g->center.dimension()) + i;
              int l = ((q==s.quadrants-1) ? s.S.n : s.offsets[q+1]) - s.offsets[q];
              _seq<vertex*> A = _seq<vertex*>(s.S.A + s.offsets[q],l);
              s.parent->children[i] = s.newNodes+q;
            }),
            dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
              int i = p.s;
              return encore_call<newTree>(st, pt, s.A, s.newcenter, s.parent->size/2.0, s.newNodes+p.q,1);
            }),
            dc::stmt([] (sar& s, par& p) {
              p.s++;
            })
          }))
        }), dc::stmts({ // else
          dc::stmt([] (sar& s, par& p) {
             p.s = 0; p.e = (1<<s.center.dimension());
          }),
          dc::parallel_for_loop([] (sar&, par& p) { return p.s < p.e; },
                                [] (par& p) { return std::make_pair(&p.s, &p.e); },
                                dc::stmts({
          dc::stmt([] (sar& s, par& p) {
            int i = p.s;
            point newcenter = (s.parent->center).offsetPoint(i, s.parent->size/4.0);
            s.parent->children[i] = new(s.newNodes + i + 
               s.nodesToLeft*(1<<s.g->center.dimension())) gTreeNode(newcenter,s.parent->size/2.0);
          })),
          dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
            int i = p.s;
            return encore_call<buildRecursiveTree>(st, pt, s.S, s.offsets, s.quadrants, s.newNodes + (1<<(s.depth*s.g->center.dimension())), s.parent->children[i], (s.nodesToLeft << s.g->center.dimension())+i, s.height-1,s.depth+1);
          }),
          dc::stmt([] (sar& s, par& p) {
            p.s++;
            })))
        })),
        dc::sequential_loop([] (sar& s, par&) {
          s.s = 0; s.e = (1<<s.g->center.dimension());
        }, [] (sar& s, par&) {
         return std::make_pair(&s.s, &s.e);
        }, [] (sar& s, par&, int lo, int hi) {
          int i = s.s;
          s.parent->data = s.parent->data + (s.parent->children[i])->data;
          s.parent->count += (s.parent->children[i])->count;
        }),
        dc::stmt([] (sar& s, par& p) {
          if (s.parent->count == 0) {
            // make it look like a leaf
            s.parent->vertices = s.S.A;
          }
        })
      });
  }
  
};

template <class pointT, class vectT, class vertexT, class nodeData>
typename buildRecursiveTree<pointT,vectT,vertexT,nodeData>::cfg_type buildRecursiveTree<pointT,vectT,vertexT,nodeData>::cfg = buildRecursiveTree<pointT,vectT,vertexT,nodeData>::get_cfg();

template <class pointT, class vectT, class vertexT, class nodeData>
class sortBlocksBig : public encore::edsl::pcfg::shared_activation_record {
public:

  typedef pointT point;
  typedef vectT fvect;
  typedef vertexT vertex;
  typedef pair<point,point> pPair;

  vertex** S; int count; int quadrants; 
  int logdivs; 
  double size; point center; int* offsets;
  pintv* blk; double blocksize; point minpt;

  sortBlocksBig(vertex** S, int count, int quadrants, 
                int logdivs, 
                double size, point center, int* offsets)
    : S(S), count(count), quadrants(quadrants), logdivs(logdivs),
      size(size), center(center), offsets(offsets) { }

  encore_private_activation_record_begin(encore::edsl, sortBlocksBig, 2)
    int s; int e;
  encore_private_activation_record_end(encore::edsl, sortBlocksBig, sar, par, dc, get_dc)

  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        s.blk = newA(pintv,count);
        s.blocksize = size/(double)(1 << logdivs);
        s.minpt = center.offsetPoint(0,size/2);
      }),
      dc::parallel_for_loop([] (sar& s, par& p) { p.s = 0; p.e = s.count; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
                            [] (sar& s, par& p, int lo, int hi) {
        auto blk = s.blk;
        auto minpt = s.mintpt;
        auto blocksize = s.blocksize;
        auto logdivs = s.logdivs;
        auto S = s.S;
        for (auto i = lo; i != hi; i++) {
          blk[i] = pintv(ptFindBlock(minpt, blocksize, logdivs, S[i]->pt), S[i]);
        }
      }),
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        return intSort::iSort5(st, pt, s.blk, s.offsets, s.count, s.quadrants, utils::firstF<int,vertex*>());
      }),
      dc::parallel_for_loop([] (sar& s, par& p) { p.s = 0; p.e = s.count; },
                            [] (par& p) { return std::make_pair(&p.s, &p.e); },
                            [] (sar& s, par& p, int lo, int hi) {
        auto blk = s.blk;
        auto S = s.S;
        for (auto i = lo; i != hi; i++) {
          S[i] = blk[i].second;
        }
      }),
      dc::stmt([] (sar& s, par& p) {
        free(s.blk);
      })
    });
  }
  
};

template <class pointT, class vectT, class vertexT, class nodeData>
typename sortBlocksBig<pointT,vectT,vertexT,nodeData>::cfg_type sortBlocksBig<pointT,vectT,vertexT,nodeData>::cfg = sortBlocksBig<pointT,vectT,vertexT,nodeData>::get_cfg();

template <class pointT, class vectT, class vertexT, class nodeData>
class sortBlocksSmall : public encore::edsl::pcfg::shared_activation_record {
public:
  
  typedef pointT point;
  typedef vectT fvect;
  typedef vertexT vertex;
  typedef pair<point,point> pPair;

  vertex** S; int count; point center; int* offsets;
  int quadrants;

  sortBlocksSmall(vertex** S, int count, point center, int* offsets)
    : S(S), count(count), center(center), offsets(offsets) { }

  encore_dc_declare(encore::edsl, sortBlocksSmall, sar, par, dc, get_dc)

  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        vertex* start = S[0];
        s.quadrants = 1 << s.center.dimension();
        pintv* blk = malloc_array<pintv>(count);
        auto count = s.count;
        auto blk = s.blk;
        auto S = s.S;
        auto center = s.center;
        for (int i=0;i< count; i++) {
          //cout << i << " : " << S[i]-start << endl;
          blk[i] = pintv((S[i]->pt).quadrant(center),S[i]);
        }
      }),
      dc::spawn_join([] (sar& s, par&, plt pt, stt st) {
        return encore_call<sampleSort<pintv,compFirst>>(st, pt, s.blk, s.count, compFirst());
      }),
      dc::stmt([] (sar& s, par& p) {
        auto count = s.count;
        auto blk = s.blk;
        auto S = s.S;
        auto offsets = s.offsets;
        int j = -1;
        for (int i=0; i< count; i++) {
          S[i] = blk[i].second;
          while (blk[i].first != j) {
            offsets[++j] = i;
          }
        }
        while (++j < quadrants) offsets[j] = count;
        free(blk);
      }),
    });
  }
  
};

template <class pointT, class vectT, class vertexT, class nodeData>
typename sortBlocksSmall<pointT,vectT,vertexT,nodeData>::cfg_type sortBlocksSmall<pointT,vectT,vertexT,nodeData>::cfg = sortBlocksSmall<pointT,vectT,vertexT,nodeData>::get_cfg();

/* newNodes is the memory to use for allocated gTreeNodes.  this is
   currently ignored for the upper levels of recursion (where we build
   more than 4 or 8 "quadrants" at a time).  It is used at the lower
   levels where we build just a single level of the tree.  If space is
   exhausted, more memory is allocated as needed.  The hope is that
   the number of reallocations are reduced  */
template <class pointT, class vectT, class vertexT, class nodeData>
class gTreeNodeConstructor : public encore::edsl::pcfg::shared_activation_record {
public:

  using gtn = gTreeNode<pointT,vectT,vertexT,nodeData>;
  typedef pointT point;
  typedef vectT fvect;
  typedef vertexT vertex;
  typedef pair<point,point> pPair;

  _seq<vertex*> S, point cnt, double sz, gTreeNode* newNodes, int numNewNodes;
  gtn* g; int logdivs; int usedNodes; int i; int* offsets; int quadrants; int l;
  _seq<vertex*> A; point newcenter;

  gTreeNodeConstructor(_seq<vertex*> S, point cnt, double sz, gTreeNode* newNodes, int numNewNodes, gtn* g)
    : S(S), cnt(cnt), sz(sz), newNodes(newNodes), numNewNodes(numNewNodes), g(g) { }

  encore_dc_declare(encore::edsl, gTreeNodeConstructor, sar, par, dc, get_dc)

  static
  dc get_dc() {
    return dc::stmts({
      dc::stmt([] (sar& s, par& p) {
        s.g->count = S.n;
        s.g->size = sz;
        s.g->center = cnt;
        s.g->vertices = NULL;
        s.g->nodeMemory = NULL;
        s.logdivs = (int)(std::log2(s.g->count)*GTREE_SUBPROB_POW/(double)s.g->center.dimension());
      }),
      dc::cond({
        std::make_pair([] (sar& s, par& p) {
          return s.logdivs > 1 && s.g->count > GTREE_BASE_CASE;
        }, dc::stmts({
          dc::stmt([] (sar& s, par& p) {
            s.quadrants = (1<<(s.g->center.dimension() * s.logdivs)); // total number
            s.offsets = malloc_array<int>(s.quadrants);
          }),
          dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
            using fct = sortBlocksBig<pointT,vectT,vertexT,nodeData>;
            return encore_call<fct>(st, pt, s.S.A, s.g->count, s.quadrants, s.logdivs, s.g->size, s.g->center, s.offsets);
          }),
          dc::stmt([] (sar& s, par& p) {
            s.numNewNodes = (1<<s.g->center.dimension());
            for (int i=0; i< s.logdivs;i++) {
              s.numNewNodes = ((s.numNewNodes << s.g->center.dimension())
                               + (1<< s.g->center.dimension()));
            }
            s.g->nodeMemory = malloc_array<gTreeNode>(s.numNewNodes);
          }),
          dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
            using fct = buildRecursiveTree<pointT,vectT,vertexT,nodeData>;
            return encore_call<fct>(st, pt, s.S, s.offsets, s.quadrants, s.g->nodeMemory, g, 0, s.logdivs, 1);
          }),
          dc::stmt([] (sar& s, par& p) {
            free(s.offsets);
          })
        })),
        std::make_pair([] (sar& s, par& p) {
          return s.g->count > gMaxLeafSize;
        }, dc::stmts({
          dc::stmt([] (sar& s, par& p) {
            if (s.numNewNodes < (1<<s.g->center.dimension())) { 
              // allocate ~ count/gMaxLeafSize gTreeNodes here
              s.numNewNodes = std::max(GTREE_ALLOC_FACTOR*
                                       std::max(s.g->count/gMaxLeafSize, 1<< s.g->center.dimension()),
                                       1<< s.g->center.dimension());
              s.g->nodeMemory = malloc_array<gTreeNode>(s.numNewNodes);
              s.newNodes = s.g->nodeMemory;
              s.quadrants = ( 1<< s.g->center.dimension());
            }
          }),
          dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
            using fct = sortBlockSmall<pointT,vectT,vertexT,nodeData>;
            return encore_call<fct>(st, pt, s.S.A, s.S.n, s.g->center, s.offsets);
          })
        })),
        dc::stmts({
          dc::stmt([] (sar& s, par& p) {
            s.usedNodes = 0;
            s.i = 0;
          }),
          dc::sequential_loop([] (sar& s, par&) { return s.j < s.l; }, dc::stmt([] (sar& s, par&) {
            dc::stmt([] (sar& s, par& p) {
              s.l = ((s.i == s.quadrants-1) ? s.S.n : s.offsets[i+1]) - s.offsets[s.i];
              s.A = _seq<vertex*>(s.S.A + s.offsets[s.i], s.l);
              s.newcenter = s.g->center.offsetPoint(s.i, s.g->size/4.0);
              s.g->children[s.i] = new (s.newNodes+s.usedNodes) gTreeNode;
            }),
            dc::spawn_join([] (sar& s, par& p, plt pt, stt st) {
              auto x = (s.newNodes+s.usedNodes) + 1;
              auto y = ((s.numNewNodes - (1<<s.g->center.dimension()))*s.l/s.g->count + 1) - 1;
              return encore_call<gTreeNodeConstructor>(st, pt, s.A, s.newcenter, s.g->size/2.0, x, y);
            }),
            dc::stmt([] (sar& s, par& p) {
              s.usedNodes += (s.numNewNodes - (1<< s.g->center.dimension()))*s.l/s.g->count + 1;
              s.i++;
            })
          })),
          dc::stmt([] (sar& s, par& p) {
            for (int i=0 ; i < s.quadrants; i++) {
              s.g->data = s.g->data + s.g->children[s.i]->data;
            }
          })
        }),
        dc::stmt([] (sar& s, par& p) {
          s.g->vertices = s.S.A;
          for (int i=0; i < s.g->count; i++) {
            s.g->data = s.g->data + s.S.A[s.i];
          }
        })
      })
    });
  }
  
};

template <class pointT, class vectT, class vertexT, class nodeData>
typename gTreeNodeConstructor<pointT,vectT,vertexT,nodeData>::cfg_type gTreeNodeConstructor<pointT,vectT,vertexT,nodeData>::cfg = gTreeNodeConstructor<pointT,vectT,vertexT,nodeData>::get_cfg();
  
} // end namespace
