
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

void scheduler_loop() {
  int fuel = D;
  while (! ready.empty()) {
    vertex* v = ready.back();
    ready.pop_back();
    if (v->nb_strands() == 0) {
      delete v;
      /* todo:
          deal with deallocation of outset; can't simply delete and replace
          because outset may be used as a future
       */
      assert(false);
    } else {
      incounter* in = v->in;
      v->in = new incounter;
      delete in;
    }
    fuel = v->run(fuel);
    if (fuel == 0) {
      fuel = D;
    }
  }
}
  
} // end namespace
  
void push_vertex(vertex* v) {
  trivial::ready.push_back(v);
}

void new_edge(vertex* source, vertex* destination) {
  assert(false); // todo
}
  
void release(vertex* v) {
  assert(false); // todo
}
  
} // end namespace
} // end namespace

#endif /*! _PASL_SCHED_SCHEDULER_H_ */