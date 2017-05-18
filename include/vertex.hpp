
#include <memory>

#include "forward.hpp"
#include "incounter.hpp"
#include "outset.hpp"

#ifndef _ENCORE_SCHED_VERTEX_H_
#define _ENCORE_SCHED_VERTEX_H_

namespace encore {
namespace sched {

class vertex {
public:
  
  incounter_handle* release_handle = nullptr;
  
  bool is_suspended = false;
  
private:
  
  std::unique_ptr<incounter> in;

  outset* out_future = nullptr; // to use if is_future
  std::unique_ptr<outset> out_dflt; // to use otherwise
  
public:
  
  vertex() {
    in.reset(new incounter(this));
    release_handle = get_incounter()->increment(this);
    out_dflt.reset(new outset);
  }
  
  virtual ~vertex() {
#ifndef NDEBUG
    if (in) {
      assert(! in->is_nonzero());
    }
#endif
  }
  
  incounter* get_incounter() {
    return in.get();
  }
  
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
  
  virtual int nb_strands() = 0;
  
  virtual int run(int fuel) = 0;
  
  virtual std::pair<vertex*, vertex*> split(int nb) = 0;
  
};
  
void delete_future(outset* f) {
  if (f != nullptr) {
    delete f;
  }
}
  
} // end namespace
} // end namespace

#endif /*! _ENCORE_SCHED_VERTEX_H_ */
