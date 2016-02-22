#include <assert.h>

#ifndef _ENCORE_FORWARD_H_
#define _ENCORE_FORWARD_H_

namespace encore {
namespace sched {
    
class vertex;
class incounter;
class outset;
  
void schedule(vertex* v);
void parallel_notify(bool, outset*);
void parallel_deallocate(outset*);
void release(vertex* v);
  
} // end namespace
} // end namespace

#endif /*! _ENCORE_FORWARD_H_ */
