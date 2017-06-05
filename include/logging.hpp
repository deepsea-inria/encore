
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <sys/time.h>
#include <time.h>
#include <algorithm>

#include "perworker.hpp"
#include "cmdline.hpp"

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
  nb_kinds
};

using event_tag_type = enum {
  enter_launch = 0, exit_launch,
  enter_algo,       exit_algo,
  enter_wait,       exit_wait,
  algo_phase,
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
    case exit_wait: return phases;
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
  
class event_type {
public:
  
  double timestamp;
  
  event_tag_type tag;
  
  int worker_id;
  
  event_type() { }
  
  event_type(event_tag_type tag)
  : tag(tag) { }
  
  union {
    
  } extra;
  
  void print_byte_header(FILE* f) {
    fwrite_int64 (f, (int64_t) timestamp);
    fwrite_int64 (f, worker_id);
    fwrite_int64 (f, tag  );
  }
  
  void print_byte(FILE* f) {
    
  }
  
  void print_text_header(FILE* f) {
    fprintf(f, "%lf\t%d\t%s\t", timestamp, worker_id, name_of(tag).c_str());
  }
  
  void print_text_descr(FILE* f) {

  }
  
  void print_text(FILE* f) {
    print_text_header (f);
    print_text_descr (f);
    fprintf (f, "\n");
  }
  
  
};
  
/*---------------------------------------------------------------------*/
/* Log buffer */
  
using buffer_type = std::vector<event_type>;

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
  uint64_t basetime;
  
  static
  void initialize() {
    if (! enabled) {
      return;
    }
    real_time = deepsea::cmdline::parse_or_default_bool("log_stdout", false);
    tracking_kind[phases] = deepsea::cmdline::parse_or_default_bool("log_phases", false);
    tracking_kind[threads] = deepsea::cmdline::parse_or_default_bool("log_threads", false);
    tracking_kind[migration] = deepsea::cmdline::parse_or_default_bool("log_migration", false);
    bool pview = deepsea::cmdline::parse_or_default_bool("pview", false);
    if (pview) {
      tracking_kind[phases] = true;
    }
    push(event_type(enter_launch));
  }
  
  static
  uint64_t now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return 1000000l * ((uint64_t) tv.tv_sec) + ((uint64_t) tv.tv_usec);
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
    e.timestamp = now() - basetime;
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
    std::string fname = deepsea::cmdline::parse_or_default_string("log_bytes_fname", "");
    if (fname == "") {
      return;
    }
    FILE* f = fopen(fname.c_str(), "w");
    for (auto it = b.begin(); it != b.end(); it++) {
      event_type& e = *it;
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
    for (auto it = b.begin(); it != b.end(); it++) {
      event_type& e = *it;
      e.print_text(f);
    }
    fclose(f);
  }
  
  static
  void output() {
    push(event_type(exit_launch));
    buffer_type b;
    for (auto id = 0; id != data::perworker::get_nb_workers(); id++) {
      buffer_type& b_id = buffers[id];
      for (auto it = b_id.begin(); it != b_id.end(); it++) {
        b.push_back(*it);
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
uint64_t logging_base<enabled>::basetime;

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

} // end namespace
} // end namespace

#endif /*! _ENCORE_LOGGING_H_ */
