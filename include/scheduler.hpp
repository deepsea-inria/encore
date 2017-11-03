
#include <vector>
#include <deque>
#include <thread>

#include "vertex.hpp"
#include "perworker.hpp"
#include "chunkedseq.hpp"
#include "logging.hpp"
#include "stats.hpp"

#ifndef _ENCORE_SCHEDULER_H_
#define _ENCORE_SCHEDULER_H_

namespace encore {
namespace sched {
  
/*---------------------------------------------------------------------*/
/* Global scheduler configuration */
  
using scheduler_tag = enum {
  steal_half_work_stealing_tag,
  encore_work_stealing_tag,
  steal_one_work_stealing_tag
};
  
scheduler_tag scheduler = steal_half_work_stealing_tag;
  
template <class Item>
using perworker_array = data::perworker::array<Item>;
  
perworker_array<vertex*> vertices;
  
perworker_array<std::deque<vertex*>> suspended;
  
fuel::check_type run_vertex(vertex* v) {
  vertices.mine() = v;
  return v->run();
}
  
bool should_exit = false;

perworker_array<std::mt19937> schedule_random_number_generators;  // random-number generators
  
perworker_array<std::mt19937> random_number_generators;  // random-number generators

int random_other_worker(int my_id) {
  int nb_workers = data::perworker::get_nb_workers();
  assert(nb_workers != 1);
  std::uniform_int_distribution<int> distribution(0, nb_workers - 2);
  int i = distribution(random_number_generators[my_id]);
  if (i >= my_id) {
    i++;
  }
  assert(i >= 0);
  assert(i < nb_workers);
  assert(i != my_id);
  return i;  
}
  
/*---------------------------------------------------------------------*/
/* Steal-one, work-stealing scheduler */

namespace steal_one_work_stealing {

std::atomic<int> nb_running_workers;

perworker_array<std::atomic<bool>> status;

static constexpr int no_request = -1;

perworker_array<std::atomic<int>> request;

perworker_array<std::deque<vertex*>> deques;
  
vertex* no_response = tagged::tag_with((vertex*)nullptr, 1);

perworker_array<std::atomic<vertex*>> transfer;
  
// one instance of this function is to be run by each
// participating worker thread
void worker_loop(vertex* v) {
  int my_id = data::perworker::get_my_id();
  std::deque<vertex*>& my_ready = deques[my_id];
  std::deque<vertex*>& my_suspended = suspended[my_id];
  fuel::initialize_worker();
  
  if (v != nullptr) {
    // this worker is the leader
    release(v);
    v = nullptr;
  }
  
  auto is_finished = [&] {
    return should_exit && my_ready.empty();
  };
    
  // update the status flag
  auto update_status = [&] {
    bool b = false;
    if (my_ready.size() >= 2) {
      b = true;
    } else if (my_ready.size() == 1) {
      vertex* v = my_ready.front();
      b = (v->nb_strands() >= 2);
    }
    if (status[my_id].load() != b) {
      status[my_id].store(b);
    }
  };
  
  // check for an incoming steal request
  auto communicate = [&] {
    logging::push_event(logging::worker_communicate);
    int j = request[my_id].load();
    if (j == no_request) {
      return;
    }
    auto sz = my_ready.size();
    if (sz > 1) {
      assert(sz >= 1);
      vertex* v = my_ready.front();
      auto n = v->nb_strands();
      if (n >= 2) {
        my_ready.pop_front();
        auto vs = v->split(n / 2);
        my_ready.push_front(vs.v1);
        if (vs.v0 != nullptr) {
          my_ready.push_front(vs.v0);
        }
        v = vs.v2;
      } else {
        my_ready.pop_front();
      }
      transfer[j].store(v);
    } else {
      transfer[j].store(nullptr); // reject query
    }
    request[my_id].store(no_request);
  };
  
  // called by workers when running out of work
  auto acquire = [&] {
    if (data::perworker::get_nb_workers() == 1) {
      return;
    }
    assert(my_ready.empty() && my_suspended.empty());
    logging::push_event(logging::enter_wait);
    while (! is_finished()) {
      transfer[my_id].store(no_response);
      int k = random_other_worker(my_id);
      int orig = no_request;
      if (status[k].load() && atomic::compare_exchange(request[k], orig, my_id)) {
        while (transfer[my_id].load() == no_response) {
          if (is_finished()) {
            logging::push_event(logging::exit_wait);
            return;
          }
          communicate();
        }
        vertex* v = transfer[my_id].load();
        if (v != nullptr) {
          my_ready.push_back(v);
          request[my_id].store(no_request);
          stats::on_steal();
          logging::push_frontier_acquire(k);
          logging::push_event(logging::exit_wait);
          return;
        }
      }
      communicate();
    }
    logging::push_event(logging::exit_wait);
  };

  auto unblock = [&] {
    if (my_suspended.empty()) {
      return;
    }
    vertex* v = my_suspended.front();
    my_suspended.pop_front();
    run_vertex(v);
  };
  
  auto run = [&] () {
    fuel::check_type f = fuel::check_no_promote;
    while ((f == fuel::check_no_promote) && (! my_ready.empty())) {
      vertex* v = my_ready.back();
      my_ready.pop_back();
      f = run_vertex(v);
      if (v->nb_strands() == 0) {
        parallel_notify(v->get_outset());
        delete v;
      }
    }
#ifdef ENCORE_RANDOMIZE_SCHEDULE
    auto N = (int)my_ready.size();
    if (N > 1) {
      std::uniform_int_distribution<int> distribution(0, 2*N);
      int i = distribution(schedule_random_number_generators[my_id]) % (N - 1);
      if (i >= 0) {
        i++;
      }
      std::swap(my_ready[0], my_ready[i]);
    }    
#endif    
    return f;
  };
  
  while (! is_finished()) {
    if (! my_ready.empty()) {
      communicate();
      run();
      unblock();
      update_status();
    } else if (my_suspended.size() >= 1) {
      communicate();
      unblock();
    } else if (data::perworker::get_nb_workers() == 1) {
      break;
    } else {
      auto s = stats::on_enter_acquire();
      acquire();
      stats::on_exit_acquire(s);
      update_status();
    }
  }
  
  assert(my_ready.empty());
  assert(my_suspended.empty());
  nb_running_workers--;
}
  
void launch(int nb_workers, vertex* v) {
  status.for_each([&] (int, std::atomic<bool>& b) {
    b.store(false);
  });
  request.for_each([&] (int, std::atomic<int>& r) {
    r.store(no_request);
  });
  transfer.for_each([&] (int, std::atomic<vertex*>& t) {
    t.store(no_response);
  });
  nb_running_workers.store(nb_workers);
  for (int i = 1; i < nb_workers; i++) {
    auto t = std::thread([] {
      worker_loop(nullptr);
    });
    t.detach();
  }
  logging::push_event(logging::enter_algo);
  worker_loop(v);
  while (nb_running_workers.load() > 0);
  logging::push_event(logging::exit_algo);
}
  
} // end namespace
 
/*---------------------------------------------------------------------*/
/* Steal-half, work-stealing scheduler */
  
namespace steal_half_work_stealing {
  
class frontier {
private:
  
  template <class Item, int Chunk_capacity, class Cache>
  using weighted_stack = pasl::data::chunkedseq::bootstrapped::stack<Item, Chunk_capacity, Cache>;
  
  class measure_env {
  public:
    
    int operator()(vertex* v) const {
      return v->nb_strands();
    }
    
  };
  
  static constexpr int chunk_capacity = 512;
  
  using cache_type = pasl::data::cachedmeasure::weight<vertex*, int, size_t, measure_env>;
  
  using weighted_seq_type = weighted_stack<vertex*, chunk_capacity, cache_type>;
  
  weighted_seq_type vs;
  
  vertex* pop() {
    assert(! vs.empty());
#ifdef ENCORE_RANDOMIZE_SCHEDULE
    int pos = rand() % vs.size();
    vertex* tmp = vs.pop_back();
    vs.insert(vs.begin() + pos, tmp);
#endif
    return vs.pop_back();
  }
  
public:
  
  int nb_strands() {
    return vs.get_cached();
  }
  
  bool empty() {
    return nb_strands() == 0;
  }
  
  void push(vertex* v) {
#ifndef NDEBUG
    vs.for_each([&] (vertex* v2) {
      assert(v != v2);
    });
#endif
    assert(v->nb_strands() > 0);
    vs.push_back(v);
  }
  
  void run() {
    fuel::check_type f = fuel::check_no_promote;
    while ((f == fuel::check_no_promote) && (! empty())) {
      vertex* v = pop();
      f = run_vertex(v);
      if (v->nb_strands() == 0) {
        parallel_notify(v->get_outset());
        delete v;
      }
    }
  }
  
  void split(int nb, frontier& other) {
    vertex* v = nullptr;
    vs.split([&] (int n) { return nb < n; }, v, other.vs);
    int vnb = v->nb_strands();
    int spill = (vnb + other.nb_strands()) - nb;
    assert(spill >= 0);
    if (spill == 0) {
      other.vs.push_back(v);
    } else if (spill == vnb) {
      vs.push_back(v);
    } else {
      vertex_split_type sr = v->split(vnb - spill);
      if (sr.v0 != nullptr) {
        vs.push_front(sr.v0);
      }
      vs.push_front(sr.v1);
      other.vs.push_back(sr.v2);
    }
#if defined(ENCORE_ENABLE_LOGGING)
    int n2 = nb_strands();
    int n3 = other.nb_strands();
    logging::push_frontier_split(n2, n3);
#endif
  }
  
  void swap(frontier& other) {
    vs.swap(other.vs);
  }
  
};
  
std::atomic<int> nb_running_workers;
  
perworker_array<std::atomic<bool>> status;

static constexpr int no_request = -1;
  
perworker_array<std::atomic<int>> request;
  
frontier* no_response = tagged::tag_with((frontier*)nullptr, 1);
  
perworker_array<std::atomic<frontier*>> transfer;
  
perworker_array<frontier> frontiers;
  
// one instance of this function is to be run by each
// participating worker thread
void worker_loop(vertex* v) {
  int my_id = data::perworker::get_my_id();
  frontier& my_ready = frontiers[my_id];
  std::deque<vertex*>& my_suspended = suspended[my_id];
  fuel::initialize_worker();
  
  if (v != nullptr) {
    // this worker is the leader
    release(v);
    v = nullptr;
  }
  
  auto is_finished = [&] {
    return should_exit && my_ready.empty();
  };
    
  // update the status flag
  auto update_status = [&] {
    bool b = (my_ready.nb_strands() >= 2);
    if (status[my_id].load() != b) {
      status[my_id].store(b);
    }
  };
  
  // check for an incoming steal request
  auto communicate = [&] {
    logging::push_event(logging::worker_communicate);    
    int j = request[my_id].load();
    if (j == no_request) {
      return;
    }
    int sz = my_ready.nb_strands();
    if (sz > 1) {
      // transfer half of the local frontier to worker with id j
      frontier* f = new frontier;
      my_ready.split(sz / 2, *f);
      transfer[j].store(f);
    } else {
      transfer[j].store(nullptr); // reject query
    }
    request[my_id].store(no_request);
  };
  
  // called by workers when running out of work
  auto acquire = [&] {
    if (data::perworker::get_nb_workers() == 1) {
      return;
    }
    assert(my_ready.empty() && my_suspended.empty());
    logging::push_event(logging::enter_wait);
    while (! is_finished()) {
      transfer[my_id].store(no_response);
      int k = random_other_worker(my_id);
      int orig = no_request;
      if (status[k].load() && atomic::compare_exchange(request[k], orig, my_id)) {
        while (transfer[my_id].load() == no_response) {
          if (is_finished()) {
            logging::push_event(logging::exit_wait);
            return;
          }
          communicate();
        }
        frontier* f = transfer[my_id].load();
        if (f != nullptr) {
          f->swap(my_ready);
          delete f;
          request[my_id].store(no_request);
          stats::on_steal();
          logging::push_frontier_acquire(k);
          logging::push_event(logging::exit_wait);
          return;
        }        
      }
      communicate();
    }
    logging::push_event(logging::exit_wait);
  };
  
  auto unblock = [&] {
    if (my_suspended.empty()) {
      return;
    }
    vertex* v = my_suspended.front();
    my_suspended.pop_front();
    run_vertex(v);
  };
  
  while (! is_finished()) {
    if (my_ready.nb_strands() >= 1) {
      communicate();
      my_ready.run();
      unblock();
      update_status();
    } else if (my_suspended.size() >= 1) {
      communicate();
      unblock();
    } else if (data::perworker::get_nb_workers() == 1) {
      break;
    } else {
      auto s = stats::on_enter_acquire();
      acquire();
      stats::on_exit_acquire(s);
      update_status();
    }
  }

  assert(my_ready.empty());
  assert(my_suspended.empty());
  nb_running_workers--;
}
  
void launch(int nb_workers, vertex* v) {
  status.for_each([&] (int, std::atomic<bool>& b) {
    b.store(false);
  });
  request.for_each([&] (int, std::atomic<int>& r) {
    r.store(no_request);
  });
  transfer.for_each([&] (int, std::atomic<frontier*>& t) {
    t.store(no_response);
  });
  nb_running_workers.store(nb_workers);
  for (int i = 1; i < nb_workers; i++) {
    auto t = std::thread([] {
      worker_loop(nullptr);
    });
    t.detach();
  }
  logging::push_event(logging::enter_algo);
  worker_loop(v);
  while (nb_running_workers.load() > 0);
  logging::push_event(logging::exit_algo);
}

} // end namespace

/*---------------------------------------------------------------------*/
/* Encore's work-stealing scheduler */
  
namespace encore_work_stealing {
  
class frontier {
private:
  
  template <class Item, int Chunk_capacity, class Cache>
  using weighted_stack = pasl::data::chunkedseq::bootstrapped::stack<Item, Chunk_capacity, Cache>;
  
  class measure_env {
  public:
    
    int operator()(vertex* v) const {
      return v->nb_strands();
    }
    
  };
  
  static constexpr int chunk_capacity = 512;
  
  using cache_type = pasl::data::cachedmeasure::weight<vertex*, int, size_t, measure_env>;
  
  using weighted_seq_type = weighted_stack<vertex*, chunk_capacity, cache_type>;
  
  weighted_seq_type vs;
  
  vertex* pop() {
    assert(! vs.empty());
#ifdef ENCORE_RANDOMIZE_SCHEDULE
    int pos = rand() % vs.size();
    vertex* tmp = vs.pop_back();
    vs.insert(vs.begin() + pos, tmp);
#endif
    return vs.pop_back();
  }
  
public:
  
  int nb_strands() {
    return vs.get_cached();
  }
  
  bool empty() {
    return nb_strands() == 0;
  }
  
  void push(vertex* v) {
#ifndef NDEBUG
    vs.for_each([&] (vertex* v2) {
      assert(v != v2);
    });
#endif
    assert(v->nb_strands() > 0);
    vs.push_back(v);
  }
  
  void run() {
    fuel::check_type f = fuel::check_no_promote;
    while ((f == fuel::check_no_promote) && (! empty())) {
      vertex* v = pop();
      f = run_vertex(v);
      if (v->nb_strands() == 0) {
        parallel_notify(v->get_outset());
        delete v;
      }
    }
  }
  
  void split(int nb, frontier& other) {
    vertex* v = nullptr;
    vs.split([&] (int n) { return nb < n; }, v, other.vs);
    int vnb = v->nb_strands();
    int spill = (vnb + other.nb_strands()) - nb;
    assert(spill >= 0);
    if (spill == 0) {
      other.vs.push_back(v);
    } else if (spill == vnb) {
      vs.push_back(v);
    } else {
      vertex_split_type sr = v->split(vnb - spill);
      if (sr.v0 != nullptr) {
        vs.push_front(sr.v0);
      }
      vs.push_front(sr.v1);
      other.vs.push_back(sr.v2);
    }
#if defined(ENCORE_ENABLE_LOGGING)
    int n2 = nb_strands();
    int n3 = other.nb_strands();
    logging::push_frontier_split(n2, n3);
#endif
  }
  
  void swap(frontier& other) {
    vs.swap(other.vs);
  }
  
};
  
std::atomic<int> nb_running_workers;
  
perworker_array<std::atomic<frontier*>> transfers;
  
perworker_array<frontier> frontiers;
  
// one instance of this function is to be run by each
// participating worker thread
void worker_loop(vertex* v) {
  int my_id = data::perworker::get_my_id();
  frontier& my_ready = frontiers[my_id];
  std::atomic<frontier*>& my_transfer = transfers[my_id];
  std::deque<vertex*>& my_suspended = suspended[my_id];
  std::unique_ptr<frontier> my_transfer_buf(new frontier);
  fuel::initialize_worker();
  
  if (v != nullptr) {
    // this worker is the leader
    release(v);
    v = nullptr;
  }

  auto is_my_ready_empty = [&] {
    return my_ready.empty() && (my_transfer.load() == nullptr);
  };

  auto is_my_pool_empty = [&] {
    return is_my_ready_empty() && my_suspended.empty();
  };
  
  auto is_finished = [&] {
    return should_exit && is_my_ready_empty();
  };
      
  // check for an incoming steal request
  auto communicate = [&] {
    if (data::perworker::get_nb_workers() == 1) {
      return;
    }
    int nb_strands = my_ready.nb_strands();
    if (nb_strands < 2) {
      return;
    }
    if (my_transfer.load() != nullptr) {
      return;
    }
    frontier* f = my_transfer_buf.release();
    if (f == nullptr) {
      f = new frontier;
    }
    my_ready.split(nb_strands / 2, *f);
    my_transfer.store(f);
    logging::push_event(logging::worker_communicate);    
  };
  
  auto unblock = [&] {
    if (my_suspended.empty()) {
      return;
    }
    vertex* v = my_suspended.front();
    my_suspended.pop_front();
    run_vertex(v);
  };

  // called by workers when running out of work
  auto acquire = [&] {
    if (data::perworker::get_nb_workers() == 1) {
      return;
    }
    assert(is_my_ready_empty());
    logging::push_event(logging::enter_wait);
    while (! is_finished()) {
      int k = random_other_worker(my_id);
      std::atomic<frontier*>& transfer_k = transfers[k];
      frontier* orig = transfer_k.load();
      if (orig == nullptr) {
        continue;
      }
      if (transfer_k.compare_exchange_strong(orig, nullptr)) {
        orig->swap(my_ready);
        my_transfer_buf.reset(orig);
        stats::on_steal();
        logging::push_frontier_acquire(k);
        break;
      }
      unblock();
      if (my_ready.nb_strands() >= 1) {
        break;
      }
    }
    logging::push_event(logging::exit_wait);
  };

  while (! is_finished()) {
    frontier* f;
    if (my_ready.nb_strands() >= 1) {
      my_ready.run();
    } else if (data::perworker::get_nb_workers() == 1) {
      break;
    } else if ((f = my_transfer.load()) != nullptr) {
      frontier* orig = f;
      if (my_transfer.compare_exchange_strong(orig, nullptr)) {
        f->swap(my_ready);
        my_transfer_buf.reset(f);
      }
    } else {
      auto s = stats::on_enter_acquire();
      acquire();
      stats::on_exit_acquire(s);
    }
    unblock();
    communicate();
  }

  assert(is_my_pool_empty());
  nb_running_workers--;
}
  
void launch(int nb_workers, vertex* v) {
  transfers.for_each([&] (int, std::atomic<frontier*>& t) {
    t.store(nullptr);
  });
  nb_running_workers.store(nb_workers);
  for (int i = 1; i < nb_workers; i++) {
    auto t = std::thread([] {
      worker_loop(nullptr);
    });
    t.detach();
  }
  logging::push_event(logging::enter_algo);
  worker_loop(v);
  while (nb_running_workers.load() > 0);
  logging::push_event(logging::exit_algo);
}

} // end namespace

void launch_scheduler(int nb_workers, vertex* v) {
  if (scheduler == steal_half_work_stealing_tag) {
    steal_half_work_stealing::launch(nb_workers, v);
  } else if (scheduler == encore_work_stealing_tag) {
    encore_work_stealing::launch(nb_workers, v);
  } else if (scheduler == steal_one_work_stealing_tag) {
    steal_one_work_stealing::launch(nb_workers, v);
  }
}
  
/*---------------------------------------------------------------------*/
/* Scheduling primitives */
  
vertex* my_vertex() {
  return vertices.mine();
}
  
void schedule(vertex* v) {
  assert(v->is_ready());
  if (v->is_suspended) {
    suspend(v);
    return;
  }
  if (v->nb_strands() == 0) {
    parallel_notify(v->get_outset());
    delete v;
    return;
  }
  if (scheduler == steal_half_work_stealing_tag) {
    steal_half_work_stealing::frontiers.mine().push(v);
  } else if (scheduler == encore_work_stealing_tag) {
    encore_work_stealing::frontiers.mine().push(v);
  } else if (scheduler == steal_one_work_stealing_tag) {
    steal_one_work_stealing::deques.mine().push_back(v);
  }
}

void new_edge(future& source_future, incounter* destination_incounter) {
  auto future_pointer = source_future.get_pointer();
  incounter_handle h = destination_incounter->increment(future_pointer);
  bool success = source_future.insert(h);
  if (! success) {
    incounter::decrement(h);
  }
}

void new_edge(future& source_future, vertex* destination) {
  new_edge(source_future, destination->get_incounter());
}
  
void new_edge(outset* source_outset, incounter* destination_incounter) {
  incounter_handle h = destination_incounter->increment(source_outset);
  bool success = source_outset->insert(h);
  if (! success) {
    incounter::decrement(h);
  }
}
  
void new_edge(vertex* source, incounter* destination_incounter) {
  new_edge(source->get_outset(), destination_incounter);
}
  
void new_edge(outset* source_outset, vertex* destination) {
  new_edge(source_outset, destination->get_incounter());
}

void new_edge(vertex* source, vertex* destination) {
  new_edge(source->get_outset(), destination->get_incounter());
}
  
void release(vertex* v) {
  assert(v != nullptr);
  assert(v->get_incounter() != nullptr);
  incounter::decrement(v->release_handle);
}
  
void suspend(vertex* v) {
  suspended.mine().push_back(v);
}
  
void parallel_notify(outset* out) {
  out->notify([&] (incounter_handle h) {
    incounter::decrement(h);
  });
}
    
} // end namespace
} // end namespace

#endif /*! _ENCORE_SCHEDULER_H_ */
