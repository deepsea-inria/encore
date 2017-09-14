
#include "cycles.hpp"
#include "perworker.hpp"

#ifndef _ENCORE_FUEL_H_
#define _ENCORE_FUEL_H_

namespace encore {
namespace fuel {

using check_type = enum {
  check_yes_promote,
  check_no_promote,
  check_suspend
};

data::perworker::array<uint64_t> time_last_promote;

uint64_t promotion_threshold = 0;

check_type check(uint64_t current) {
  uint64_t previous = time_last_promote.mine();
  if (current < previous) {
    return check_no_promote;
  }
  time_last_promote.mine() = current + promotion_threshold;
  return check_yes_promote;
}

void initialize(double cpu_freq_ghz, double kappa_nsec) {
  double cycles_per_nsec = cpu_freq_ghz;
  promotion_threshold = (uint64_t) (cycles_per_nsec * kappa_nsec);
}

void initialize_worker() {
  time_last_promote.mine() = cycles::now();
}

} // end namespace
} // end namespace

#endif /*! _ENCORE_FUEL_H_ */
