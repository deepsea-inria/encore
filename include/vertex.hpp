#include "forward.hpp"
#include "incounter.hpp"
#include "outset.hpp"

#ifndef _PASL_SCHED_VERTEX_H_
#define _PASL_SCHED_VERTEX_H_

namespace pasl {
namespace sched {

class vertex {
public:
  
  incounter* in;
  outset* out;
  incounter_handle* release_handle;
  
  virtual int nb_strands() = 0;
  virtual int run(int fuel) = 0;
  virtual vertex* split(int nb) = 0;
  
};
  
} // end namespace
} // end namespace

#endif /*! _PASL_SCHED_VERTEX_H_ */