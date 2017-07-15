
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
    nb_stacklet_deallocations,
    nb_counters
  };
  
  static
  const char* name_of_counter(counter_id_type id) {
    std::map<counter_id_type, const char*> names;
    names[nb_promotions] = "nb_promotions";
    names[nb_steals] = "nb_steals";
    names[nb_stacklet_allocations] = "nb_stacklet_allocations";
    names[nb_stacklet_deallocations] = "nb_stacklet_deallocations";
    return names[id];
  }

  using private_counters = struct {
    long counters[nb_counters];
  };
  
  static
  data::perworker::array<private_counters> all_counters;
  
  static inline
  void increment(counter_id_type id) {
    if (! enabled) {
      return;
    }
    all_counters.mine().counters[id]++;
  }
  
  static
  uint64_t now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return 1000000l * ((uint64_t) tv.tv_sec) + ((uint64_t) tv.tv_usec);
  }
  
  static
  double seconds(uint64_t t) {
    return ((double) t) / 1000000l;
  }
  
  static
  uint64_t enter_launch_time;
  
  static
  double launch_duration;
  
  static
  data::perworker::array<double> all_total_idle_time;
  
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
  
  static inline
  void on_stacklet_deallocation() {
    increment(nb_stacklet_deallocations);
  }
  
  static
  void on_enter_launch() {
    enter_launch_time = now();
  }
  
  static
  void on_exit_launch() {
    launch_duration = seconds(now() - enter_launch_time);
  }
  
  static
  uint64_t on_enter_acquire() {
    if (! enabled) {
      return 0;
    }
    return now();
  }
  
  static
  void on_exit_acquire(uint64_t enter_acquire_time) {
    if (! enabled) {
      return;
    }
    all_total_idle_time.mine() += seconds(now() - enter_acquire_time);
  }
  
  static
  void initialize() {
    for (int counter_id = 0; counter_id < nb_counters; counter_id++) {
      all_counters.for_each([&] (int, private_counters& b) {
        b.counters[counter_id] = 0;
      });
    }
    all_total_idle_time.for_each([&] (int, double& d) {
      d = 0.0;
    });
  }
  
  static
  void report() {
    if (! enabled) {
      return;
    }
    for (int counter_id = 0; counter_id < nb_counters; counter_id++) {
      long counter_value = 0;
      all_counters.for_each([&] (int, private_counters& b) {
        counter_value += b.counters[counter_id];
      });
      const char* counter_name = name_of_counter((counter_id_type)counter_id);
      std::cout << counter_name << " " << counter_value << std::endl;
    }
    std::cout << "launch_duration " << launch_duration << std::endl;
    double cumulated_time = launch_duration * data::perworker::get_nb_workers();
    double total_idle_time = 0.0;
    all_total_idle_time.for_each([&] (int, double& d) {
      total_idle_time += d;
    });
    double relative_idle = total_idle_time / cumulated_time;
    double utilization = 1.0 - relative_idle;
    std::cout << "utilization " << utilization << std::endl;
  }
  
};
  
template <bool enabled>
data::perworker::array<typename stats_base<enabled>::private_counters> stats_base<enabled>::all_counters;
  
template <bool enabled>
uint64_t stats_base<enabled>::enter_launch_time;
  
template <bool enabled>
double stats_base<enabled>::launch_duration;
  
template <bool enabled>
data::perworker::array<double> stats_base<enabled>::all_total_idle_time;
  
#ifdef ENCORE_ENABLE_STATS
using stats = stats_base<true>;
#else
using stats = stats_base<false>;
#endif
  
} // end namespace

#endif /* _ENCORE_STATS_H_ */
