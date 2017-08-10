#include <chrono>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <algorithm>

#include "perworker.hpp"
#include "cmdline.hpp"
#include "atomic.hpp"

#ifndef _ENCORE_LOGGING_H_
#define _ENCORE_LOGGING_H_

namespace encore {
namespace logging {

/*---------------------------------------------------------------------*/
/* Event categories and types */
  
using event_kind_type = enum {
  phases = 0,
  threads,
  migration,
  communicate,
  leaf_loop,
  program,
  nb_kinds
};

using event_tag_type = enum {
  enter_launch = 0,   exit_launch,
  enter_algo,         exit_algo,
  enter_wait,         exit_wait,
  worker_communicate, interrupt,
  algo_phase,
  frontier_acquire,   frontier_split,
  leaf_loop_update,
  program_point,
  nb_events
};

std::string name_of(event_tag_type e) {
  switch (e) {
    case enter_launch: return "enter_launch ";
    case exit_launch: return "exit_launch ";
    case enter_algo: return "enter_algo ";
    case exit_algo: return "exit_algo ";
    case enter_wait: return "enter_wait ";
    case exit_wait: return "exit_wait ";
    case worker_communicate: return "worker_communicate ";
    case interrupt: return "interrupt ";
    case algo_phase: return "algo_phase ";
    case frontier_acquire: return "frontier_acquire ";
    case frontier_split: return "frontier_split ";
    case leaf_loop_update: return "leaf_loop_update ";
    case program_point: return "program_point";
    default: return "unknown_event ";
  }
}

event_kind_type kind_of(event_tag_type e) {
  switch (e) {
    case enter_launch:
    case exit_launch:
    case enter_algo:
    case exit_algo:
    case enter_wait:
    case exit_wait:
    case algo_phase:                return phases;
    case worker_communicate:
    case interrupt:                 return communicate;
    case frontier_acquire:
    case frontier_split:            return migration;
    case leaf_loop_update:          return leaf_loop;
    case program_point:             return program;
    default: return nb_kinds;
  }
}

static inline
void fwrite_double (FILE* f, double v) {
  fwrite(&v, sizeof(v), 1, f);
}

static inline
void fwrite_int64 (FILE* f, int64_t v) {
  fwrite(&v, sizeof(v), 1, f);
}

using program_point_type = struct {
  
  int line_nb;
  
  const char* source_fname;

  void* ptr;
      
};

static constexpr
program_point_type dflt_ppt = { .line_nb = -1, .source_fname = nullptr, .ptr = nullptr };
  
class event_type {
public:
  
  double timestamp;
  
  event_tag_type tag;
  
  int worker_id;
  
  event_type() { }
  
  event_type(event_tag_type tag)
  : tag(tag) { }
  
  union {
    int n1;
    int n2;
    struct {
      int nb_iters;
      int nb_iters_new;
      double elapsed;
      void* estimator;
    } leaf_loop;
    program_point_type ppt;
  } extra;
      
  void print_byte(FILE* f) {
    fwrite_int64 (f, (int64_t) timestamp);
    fwrite_int64 (f, worker_id);
    fwrite_int64 (f, tag  );
  }
      
  void print_text(FILE* f) {
    fprintf(f, "%lf\t%d\t%s\t", timestamp, worker_id, name_of(tag).c_str());
    switch (tag) {
      case frontier_acquire: {
        fprintf(f, "%d", extra.n1);
        break;
      }
      case frontier_split: {
        fprintf(f, "%d\t%d", extra.n1, extra.n2);
        break;
      }
      case leaf_loop_update: {
        fprintf(f, "%d \t %d \t %lf \t %p",
                extra.leaf_loop.nb_iters,
                extra.leaf_loop.nb_iters_new,
                extra.leaf_loop.elapsed,
                extra.leaf_loop.estimator);
        break;
      }
      case program_point: {
        fprintf(f, "%s \t %d \t %p",
                extra.ppt.source_fname,
                extra.ppt.line_nb,
                extra.ppt.ptr);
        break;
      }
      default: {
        // nothing to do
      }
    }
    fprintf (f, "\n");
  }
  
  
};
  
/*---------------------------------------------------------------------*/
/* Log buffer */
  
using buffer_type = std::vector<event_type>;
  
using time_point_type = std::chrono::time_point<std::chrono::system_clock>;

static constexpr
int max_nb_ppts = 50000;

template <bool enabled>
class logging_base {
public:
  
  static
  bool real_time;
  
  static
  data::perworker::array<buffer_type> buffers;
  
  static
  bool tracking_kind[nb_kinds];
  
  static
  time_point_type basetime;

  static
  program_point_type ppts[max_nb_ppts];

  static
  int nb_ppts;
  
  static
  void initialize() {
    if (! enabled) {
      return;
    }
    real_time = deepsea::cmdline::parse_or_default_bool("log_stdout", false);
    tracking_kind[phases] = deepsea::cmdline::parse_or_default_bool("log_phases", false);
    tracking_kind[threads] = deepsea::cmdline::parse_or_default_bool("log_threads", false);
    tracking_kind[migration] = deepsea::cmdline::parse_or_default_bool("log_migration", false);
    tracking_kind[leaf_loop] = deepsea::cmdline::parse_or_default_bool("log_leaf_loop", false);
    tracking_kind[program] = tracking_kind[leaf_loop];
    bool pview = deepsea::cmdline::parse_or_default_bool("pview", false);
    if (pview) {
      tracking_kind[phases] = true;
    }
    basetime = std::chrono::system_clock::now();
    push(event_type(enter_launch));
  }
  
  static inline
  void push(event_type e) {
    if (! enabled) {
      return;
    }
    auto k = kind_of(e.tag);
    assert(k != nb_kinds);
    if (! tracking_kind[k]) {
      return;
    }
    std::chrono::duration<double> elapsed = std::chrono::system_clock::now() - basetime;
    e.timestamp = elapsed.count() * 1000000;
    e.worker_id = data::perworker::get_my_id();
    if (real_time) {
      atomic::acquire_print_lock();
      e.print_text(stdout);
      atomic::release_print_lock();
    }
    buffers.mine().push_back(e);
  }
  
  static
  void output_bytes(buffer_type& b) {
    bool pview = deepsea::cmdline::parse_or_default_bool("pview", false);
    auto dflt = pview ? "LOG_BIN" : "";
    std::string fname = deepsea::cmdline::parse_or_default_string("log_bytes_fname", dflt);
    if (fname == "") {
      return;
    }
    FILE* f = fopen(fname.c_str(), "w");
    for (auto e : b) {
      e.print_byte(f);
    }
    fclose(f);
  }
  
  static
  void output_text(buffer_type& b) {
    std::string fname = deepsea::cmdline::parse_or_default_string("log_text_fname", "");
    if (fname == "") {
      return;
    }
    FILE* f = fopen(fname.c_str(), "w");
    for (auto e : b) {
      e.print_text(f);
    }
    fclose(f);
  }
  
  static
  void output() {
    push(event_type(exit_launch));
    for (auto i = 0; i < nb_ppts; i++) {
      event_type e(program_point);
      e.extra.ppt = ppts[i];
      push(e);
    }
    buffer_type b;
    for (auto id = 0; id != data::perworker::get_nb_workers(); id++) {
      buffer_type& b_id = buffers[id];
      for (auto e : b_id) {
        b.push_back(e);
      }
    }
    std::stable_sort(b.begin(), b.end(), [] (const event_type& e1, const event_type& e2) {
      return e1.timestamp < e2.timestamp;
    });
    output_bytes(b);
    output_text(b);
  }
  
};
  
template <bool enabled>
data::perworker::array<buffer_type> logging_base<enabled>::buffers;

template <bool enabled>
bool logging_base<enabled>::tracking_kind[nb_kinds];

template <bool enabled>
bool logging_base<enabled>::real_time;

template <bool enabled>
int logging_base<enabled>::nb_ppts = 0;

template <bool enabled>
program_point_type logging_base<enabled>::ppts[max_nb_ppts];

template <bool enabled>
time_point_type logging_base<enabled>::basetime;

#ifdef ENCORE_ENABLE_LOGGING
using log_buffer = logging_base<true>;
#else
using log_buffer = logging_base<false>;
#endif
  
/*---------------------------------------------------------------------*/
/* Shortcuts */
  
static inline
void push_event(event_tag_type tag) {
  log_buffer::push(event_type(tag));
}

static inline
void push_frontier_acquire(int id) {
  event_type e(frontier_acquire);
  e.extra.n1 = id;
  log_buffer::push(e);
}

static inline
void push_frontier_split(int n1, int n2) {
  event_type e(frontier_split);
  e.extra.n1 = n1;
  e.extra.n2 = n2;
  log_buffer::push(e);
}
  
static inline
void push_leaf_loop_update(int nb_iters,
                           int nb_iters_new,
                           double elapsed,
                           void* estimator) {
  event_type e(leaf_loop_update);
  e.extra.leaf_loop.nb_iters = nb_iters;
  e.extra.leaf_loop.nb_iters_new = nb_iters_new;
  e.extra.leaf_loop.elapsed = elapsed;
  e.extra.leaf_loop.estimator = estimator;
  log_buffer::push(e);
}

static inline
void push_program_point(int line_nb,
                        const char* source_fname,
                        void* ptr) {
  if ((line_nb == -1) || (log_buffer::nb_ppts >= max_nb_ppts)) {
    return;
  }
  program_point_type ppt;
  ppt.line_nb = line_nb;
  ppt.source_fname = source_fname;
  ppt.ptr = ptr;
  log_buffer::ppts[log_buffer::nb_ppts++] = ppt;
}

/*---------------------------------------------------------------------*/
/* Profiling */

class profiling_channel {
public:
  
  std::atomic<uint64_t> work_cell;
  std::atomic<uint64_t> span_cell;

  profiling_channel* join = nullptr;

  profiling_channel() {
    work_cell.store(0);
    span_cell.store(0);
  }

  void update(uint64_t work) {
    while (true) {
      auto prev = work_cell.load();
      auto next = prev + work;
      if (atomic::compare_exchange(work_cell, prev, next)) {
        break;
      }
    }
    while (true) {
      auto prev = span_cell.load();
      auto next = prev + work;
      if (atomic::compare_exchange(span_cell, prev, next)) {
        break;
      }
    }
  }

  void update(uint64_t work, uint64_t span) {
    while (true) {
      auto prev = work_cell.load();
      auto next = prev + work;
      if (atomic::compare_exchange(work_cell, prev, next)) {
        break;
      }
    }
    while (true) {
      auto prev = span_cell.load();
      auto next = std::max(prev, span);
      if (atomic::compare_exchange(span_cell, prev, next)) {
        break;
      }
    }
  }

  void set_join(profiling_channel* join) {
    this->join = join;
  }

  void emit() {
    auto work = work_cell.load();
    auto span = span_cell.load();
    if (join != nullptr) {
      join->update(work, span);
    }
  }
  
};
  
} // end namespace
} // end namespace

#endif /*! _ENCORE_LOGGING_H_ */
