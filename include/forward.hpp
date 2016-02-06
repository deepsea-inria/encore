#include <assert.h>

#ifndef _PASL_FORWARD_H_
#define _PASL_FORWARD_H_

namespace pasl {
namespace sched {
    
class vertex;
class incounter;
  
void schedule(vertex* v);
  
} // end namespace
} // end namespace

#endif /*! _PASL_FORWARD_H_ */