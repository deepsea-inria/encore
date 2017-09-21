
#include <atomic>
#include <functional>
#include <cstdint>

#include "perworker.hpp"
#include "logging.hpp"

#ifndef _ENCORE_GRAIN_H_
#define _ENCORE_GRAIN_H_

namespace encore {
namespace grain {
  
typedef void (*callback_function_type)(uint64_t, int, int);

using callback_environment_type = struct {
  callback_function_type callback;
  int lg_nb_iters_predicted;
  int nb_iters_performed;
};
  
void noop_callback(uint64_t, int, int) {
  // nothing to do
}

uint64_t threshold_upper = 0;
  
uint64_t threshold_lower = 0;
  
callback_environment_type initial_env = {
  .callback = noop_callback,
  .lg_nb_iters_predicted = 0,
  .nb_iters_performed = 0
};

data::perworker::array<callback_environment_type> envs(initial_env);

void initialize(double cpu_freq_ghz, double grain_nsec, double promotion_threshold_nsec) {
  double cycles_per_nsec = cpu_freq_ghz;
  threshold_lower = (uint64_t) (cycles_per_nsec * grain_nsec);
  threshold_upper = (uint64_t) (cycles_per_nsec * promotion_threshold_nsec);
}
  
void callback(uint64_t elapsed) {
  callback_environment_type& env = envs.mine();
  auto cb = env.callback;
  if (cb == noop_callback) {
    return;
  }
  cb(elapsed, env.lg_nb_iters_predicted, env.nb_iters_performed);
  env = initial_env;
}
  
static constexpr
int automatic = -1;

static constexpr
int max_lg_nb_iters = 20;
  
template <int threshold, class Id>
class controller {
public:
  
  static
  std::atomic<int> grain;
  
  controller() {
    grain.store(0);
  }

  static
  void callback(uint64_t elapsed, int lg_nb_iters_predicted, int nb_iters_performed) {
    int lg_nb_iters_new = lg_nb_iters_predicted;
    auto nb_iters_predicted = 1 << lg_nb_iters_predicted;
    if (elapsed < threshold_lower) {
      lg_nb_iters_new = std::min(max_lg_nb_iters, lg_nb_iters_new + 1);
    } else if ((elapsed > threshold_upper) && (nb_iters_performed >= (nb_iters_predicted))) {
      lg_nb_iters_new = std::max(0, lg_nb_iters_new - 1);
    } else {
      return;
    }
    if (lg_nb_iters_new == lg_nb_iters_predicted) {
      return;
    }
    if (lg_nb_iters_predicted != predict_lg_nb_iterations()) {
      return;
    }
#if defined(ENCORE_ENABLE_LOGGING)
    if (grain.compare_exchange_strong(lg_nb_iters_predicted, lg_nb_iters_new)) {
      auto nb_iters_new = 1 << lg_nb_iters_new;
      logging::push_leaf_loop_update(nb_iters_predicted, nb_iters_new, elapsed, &grain);
    }
#else
    grain.compare_exchange_strong(lg_nb_iters_predicted, lg_nb_iters_new);
#endif
  }
  
  static
  void register_callback(int lg_nb_iters_predicted, int nb_iters_performed) {
    envs.mine() = {
      .callback = callback,
      .lg_nb_iters_predicted = lg_nb_iters_predicted,
      .nb_iters_performed = nb_iters_performed
    };
  }
  
  static
  int predict_lg_nb_iterations() {
    return grain.load();
  }
  
  static
  int predict_nb_iterations(int lg_nb_iters_predicted) {
    if (threshold != automatic) {
      return threshold;
    } else {
      return 1 << lg_nb_iters_predicted;
    }
  }
  
  static
  void set_ppt(int line_nb, const char* source_fname) {
    logging::push_program_point(line_nb, source_fname, &grain);
  }
  
};
  
template <int threshold, class Id>
std::atomic<int> controller<threshold, Id>::grain;
  
} // end namespace
} // end namespace

#endif /*! _ENCORE_GRAIN_H_ */
