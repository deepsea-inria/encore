#include <assert.h>

#ifndef _PASL_FORWARD_H_
#define _PASL_FORWARD_H_

namespace pasl {
namespace sched {
    
class vertex;
class incounter;
class outset;
  
void schedule(vertex* v);
void parallel_notify(bool, outset*);
void parallel_deallocate(outset*);
void reset_incounter(vertex*);
  
} // end namespace
} // end namespace

#endif /*! _PASL_FORWARD_H_ */