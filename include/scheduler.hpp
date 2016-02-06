
#include <vector>

#include "vertex.hpp"
#include "perworker.hpp"

#ifndef _PASL_SCHED_SCHEDULER_H_
#define _PASL_SCHED_SCHEDULER_H_

namespace pasl {
namespace sched {
  
static constexpr int D = 4;
static constexpr int K = 8;
  
/*---------------------------------------------------------------------*/
/* Trivial uniprocessor scheduler, for testing purposes */
  
namespace trivial {

std::vector<vertex*> ready;
  
void parallel_notify(outset*) {
  assert(false); // todo
}
  
void scheduler_loop() {
  int fuel = D;
  while (! ready.empty()) {
    vertex* v = ready.back();
    ready.pop_back();
    if (v->nb_strands() == 0) {
      parallel_notify(v->get_out());
      delete v;
    } else {
      v->reset_incounter();
    }
    fuel = v->run(fuel);
    if (fuel == 0) {
      fuel = D;
    }
  }
}
  
} // end namespace
  
void schedule(vertex* v) {
  trivial::ready.push_back(v);
}

void new_edge(vertex* source, vertex* destination) {
  incounter_handle* h = destination->get_in()->increment(destination);
  bool success = source->get_out()->insert(h);
  if (! success) {
    destination->get_in()->decrement(h);
  }
}
  
void release(vertex* v) {
  v->get_in()->decrement(v->release_handle);
}
  
} // end namespace
} // end namespace

#endif /*! _PASL_SCHED_SCHEDULER_H_ */