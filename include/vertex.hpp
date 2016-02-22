
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
  
  bool is_future = false;
  
  incounter_handle* release_handle = nullptr;
  
private:
  
  std::unique_ptr<incounter> in;
  
  /* later: consider using a proper union type, unless
   * it's clear that saving one word here would be unimportant
   */
  outset* out_future = nullptr; // to use if is_future
  std::unique_ptr<outset> out_dflt; // to use otherwise
  
public:
  
  vertex() {
    in.reset(new incounter(this));
    release_handle = get_incounter()->increment(this);
    if (is_future) {
      out_future = new outset;
    } else {
      out_dflt.reset(new outset);
    }
  }
  
  virtual ~vertex() { }
  
  incounter* get_incounter() {
    return in.get();
  }
  
  outset* get_outset() {
    if (is_future) {
      return out_future;
    } else {
      return out_dflt.get();
    }
  }
  
  virtual int nb_strands() = 0;
  
  virtual int run(int fuel) = 0;
  
  virtual vertex* split(int nb) = 0;
  
};
  
} // end namespace
} // end namespace

#endif /*! _ENCORE_SCHED_VERTEX_H_ */
