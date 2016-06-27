
#include <map>

#include "perworker.hpp"

#ifndef _ENCORE_STATS_H_
#define _ENCORE_STATS_H_

namespace encore {

template <bool enabled>
class stats_base {
private:
  
  using counter_id_type = enum {
    nb_promotions,
    nb_steals,
    nb_stacklet_allocations,
    nb_counters
  };
  
  static
  const char* name_of_counter(counter_id_type id) {
    std::map<counter_id_type, const char*> names;
    names[nb_promotions] = "nb_promotions";
    names[nb_steals] = "nb_steals";
    names[nb_stacklet_allocations] = "nb_stacklet_allocations";
    return names[id];
  }

  using buffer = struct {
    long counters[nb_counters];
  };
  
  static
  data::perworker::array<buffer> buffers;
  
  static inline
  void increment(counter_id_type id) {
    if (! enabled) {
      return;
    }
    buffers.mine().counters[id]++;
  }
  
public:
  
  static inline
  void on_promotion() {
    increment(nb_promotions);
  }
  
  static inline
  void on_steal() {
    increment(nb_steals);
  }
  
  static inline
  void on_stacklet_allocation() {
    increment(nb_stacklet_allocations);
  }
  
  static
  void initialize() {
    for (int counter_id = 0; counter_id < nb_counters; counter_id++) {
      buffers.for_each([&] (int, buffer& b) {
        b.counters[counter_id] = 0;
      });
    }
  }
  
  static
  void report() {
    if (! enabled) {
      return;
    }
    for (int counter_id = 0; counter_id < nb_counters; counter_id++) {
      long counter_value = 0;
      buffers.for_each([&] (int, buffer& b) {
        counter_value += b.counters[counter_id];
      });
      const char* counter_name = name_of_counter((counter_id_type)counter_id);
      std::cout << counter_name << " " << counter_value << std::endl;
    }
  }
  
};
  
template <bool enabled>
data::perworker::array<typename stats_base<enabled>::buffer> stats_base<enabled>::buffers;
  
#ifdef ENCORE_ENABLE_STATS
using stats = stats_base<true>;
#else
using stats = stats_base<false>;
#endif
  
} // end namespace

#endif /* _ENCORE_STATS_H_ */