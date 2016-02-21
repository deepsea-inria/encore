
#include "scheduler.hpp"
#include "edsl.hpp"
#include "cmdline.hpp"

#ifndef _ENCORE_H_
#define _ENCORE_H_

namespace cmdline = pasl::util::cmdline;

namespace encore {
  
void initialize(int argc, char** argv) {
  cmdline::set(argc, argv);
  sched::D = cmdline::parse_or_default("dag_freq", sched::D);
  sched::K = cmdline::parse_or_default("sharing_freq", sched::K);
}

} // end namespace

#endif /*! _ENCORE_H_ */
