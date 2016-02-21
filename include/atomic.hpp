#include <atomic>

#include "time.hpp"

#ifndef _ENCORE_ATOMIC_H_
#define _ENCORE_ATOMIC_H_

namespace encore {
namespace atomic {
  
namespace {
static constexpr int backoff_nb_cycles = 1l << 17;
} // end namespace

template <class T>
bool compare_exchange(std::atomic<T>& cell, T& expected, T desired) {
  if (cell.compare_exchange_strong(expected, desired)) {
    return true;
  }
  time::spin_for(backoff_nb_cycles);
  return false;
}
    
} // end namespace
} // end namespace

#endif /*! _ENCORE_ATOMIC_H_ */
