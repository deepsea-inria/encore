
#include <memory>

#include "forward.hpp"
#include "incounter.hpp"
#include "outset.hpp"

#ifndef _PASL_SCHED_VERTEX_H_
#define _PASL_SCHED_VERTEX_H_

namespace pasl {
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
    reset_incounter();
    release_handle = get_in()->increment(this);
    if (is_future) {
      out_future = new outset;
    } else {
      out_dflt.reset(new outset);
    }
  }
  
  virtual ~vertex() { }
  
  void reset_incounter() {
    in.reset(new incounter(this));
  }
  
  incounter* get_in() {
    return in.get();
  }
  
  outset* get_out() {
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

#endif /*! _PASL_SCHED_VERTEX_H_ */