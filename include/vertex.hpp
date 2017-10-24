
#include <memory>

#include "forward.hpp"
#include "incounter.hpp"
#include "outset.hpp"
#include "fuel.hpp"

#ifndef _ENCORE_SCHED_VERTEX_H_
#define _ENCORE_SCHED_VERTEX_H_

namespace encore {
namespace sched {

class vertex;

// invariant 1: each vertex is ready (incounter is zero)
// invariant 2: vertices ordered by priority
// invariant 3: vertex v0 may be the null pointer, but v1 and v2 must not
using vertex_split_type = struct {
  vertex* v0;
  vertex* v1;
  vertex* v2;
};

vertex_split_type make_vertex_split(vertex* v0, vertex* v1, vertex* v2) {
  return {.v0 = v0, .v1 = v1, .v2 = v2};
}

vertex_split_type make_vertex_split(vertex* v1, vertex* v2) {
  return make_vertex_split(nullptr, v1, v2);
}
  
class vertex {
public:
  
  incounter_handle release_handle;
  
  bool is_suspended = false;
  
private:
  
  incounter in;

  outset* out_future = nullptr; // to use if is_future
  std::unique_ptr<outset> out_dflt; // to use otherwise
  
public:
  
  vertex() : in(this) {
    release_handle = in.increment(this);
    out_dflt.reset(new outset);
  }
  
  virtual
  ~vertex() {
    assert(is_ready());
  }
  
  incounter* get_incounter() {
    return &in;
  }

#ifndef NDEBUG
  bool is_ready() {
    return ! in.is_nonzero();
  }
#endif
  
  outset* get_outset() {
    outset* r = nullptr;
    if (is_future()) {
      r = out_future;
    } else {
      r = out_dflt.get();
    }
    assert(r != nullptr);
    return r;
  }
  
  bool is_future() const {
    if (out_future == nullptr) {
      assert(out_dflt.get() != nullptr);
      return false;
    } else {
      assert(out_dflt.get() == nullptr);
      return true;
    }
  }
  
  void enable_future() {
    assert(! is_future());
    out_future = out_dflt.release();
    assert(is_future());
  }

  void make_ready() {
    in.make_ready(this);
  }
  
  virtual
  int nb_strands() = 0;
  
  virtual
  fuel::check_type run() = 0;
  
  virtual
  vertex_split_type split(int nb) = 0;
  
};
  
void delete_future(outset* f) {
  if (f != nullptr) {
    delete f;
  }
}
  
} // end namespace
} // end namespace

#endif /*! _ENCORE_SCHED_VERTEX_H_ */
