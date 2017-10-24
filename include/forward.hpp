#include <assert.h>

#ifndef _ENCORE_FORWARD_H_
#define _ENCORE_FORWARD_H_

namespace encore {
namespace sched {
    
class vertex;
class incounter;
class outset;
struct vertex_split_struct;

using vertex_split_type = struct vertex_split_struct;
  
void schedule(vertex* v);
void parallel_notify(outset*);
void parallel_deallocate(outset*);
void release(vertex* v);
void suspend(vertex* v);
  
} // end namespace
} // end namespace

#endif /*! _ENCORE_FORWARD_H_ */
