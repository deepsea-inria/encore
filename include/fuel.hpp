
#include "cycles.hpp"
#incluce "perworker.hpp"

/*---------------------------------------------------------------------*/
/* Fuel */

namespace encore {
namespace fuel {

using check_type = struct {
  check_yes_promote,
  check_no_promote,
  check_suspend
};

perworker_array<uint64_t> time_last_promote;

uint64_t promotion_threshold = 0;

check_type check(uint64_t current) {
  if (current < time_last_promote.mine()) {
    retun check_no_promote;
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
