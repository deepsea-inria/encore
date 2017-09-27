

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

int notify_threshold = 128;
  
/*---------------------------------------------------------------------*/
/* Steal-one, work-stealing scheduler */

namespace steal_one_work_stealing {

std::atomic<int> nb_running_workers;

perworker_array<std::atomic<bool>> status;

static constexpr int no_request = -1;

perworker_array<std::atomic<int>> request;

perworker_array<std::mt19937> rngs;  // random-number generators

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
  
  auto random_other_worker = [&] {
    int P = data::perworker::get_nb_workers();
    assert(P != 1);
    std::uniform_int_distribution<int> distribution(0, 2*P);
    int i = distribution(rngs[my_id]) % (P - 1);
    if (i >= my_id) {
      i++;
    }
    return i;
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
        my_ready.push_front(vs.first);
        v = vs.second;
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
      int k = random_other_worker();
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

  auto promote = [&] {
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
        parallel_notify(v->is_future(), v->get_outset());
        delete v;
      }
    }
    return f;
  };
  
  while (! is_finished()) {
    if (! my_ready.empty()) {
      communicate();
      if (run() == fuel::check_yes_promote) {
        promote();
      }
      update_status();
    } else if (my_suspended.size() >= 1) {
      communicate();
      promote();
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
        parallel_notify(v->is_future(), v->get_outset());
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
      // this split operation might create and push into this frontier
      // new threads; as such, if the combined number of strands of the
      // threads is greater than zero, the frontier after this split
      // operation will hold more strands than before.
      auto vertices = v->split(vnb - spill);
      vertex* v1 = vertices.first;
      vertex* v2 = vertices.second;
      other.vs.push_back(v2);
      vs.push_front(v1);
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
  
perworker_array<std::mt19937> rngs;  // random-number generators
  
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
  
  auto random_other_worker = [&] {
    int P = data::perworker::get_nb_workers();
    assert(P != 1);
    std::uniform_int_distribution<int> distribution(0, 2*P);
    int i = distribution(rngs[my_id]) % (P - 1);
    if (i >= my_id) {
      i++;
    }
    return i;
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
      int k = random_other_worker();
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
  
  auto promote = [&] {
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
      promote();
      update_status();
    } else if (my_suspended.size() >= 1) {
      communicate();
      promote();
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
  
void launch_scheduler(int nb_workers, vertex* v) {
  if (scheduler == steal_half_work_stealing_tag) {
    steal_half_work_stealing::launch(nb_workers, v);
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
  if (v->is_suspended) {
    suspend(v);
    return;
  }
  if (v->nb_strands() == 0) {
    parallel_notify(v->is_future(), v->get_outset());
    delete v;
    return;
  }
  if (scheduler == steal_half_work_stealing_tag) {
    steal_half_work_stealing::frontiers.mine().push(v);
  } else if (scheduler == steal_one_work_stealing_tag) {
    steal_one_work_stealing::deques.mine().push_back(v);
  }
}
  
void new_edge(outset* source_outset, incounter* destination_incounter) {
  incounter_handle* h = destination_incounter->increment(source_outset);
  bool success = source_outset->insert(h);
  if (! success) {
    destination_incounter->decrement(h);
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
  assert(v->release_handle != nullptr);
  assert(v->get_incounter() != nullptr);
  v->get_incounter()->decrement(v->release_handle);
}
  
void suspend(vertex* v) {
  suspended.mine().push_back(v);
}
  
namespace {

class parallel_notify_future : public vertex {
public:
  
  using outset_tree_node_type = typename outset::node_type;
  using item_iterator = typename outset::item_iterator;
  
  outset* out;
  item_iterator lo, hi;
  std::deque<outset_tree_node_type*> todo;
  
  enum { entry, header };
  
  int trampoline = entry;
  
  void continue_with(int target) {
    trampoline = target;
    schedule(this);
  }
  
  parallel_notify_future(outset* out,
                         item_iterator lo,
                         item_iterator hi,
                         std::deque<outset_tree_node_type*>& _todo)
  : out(out), lo(lo), hi(hi) {
    todo = _todo;
  }
  
  int nb_strands() {
    if (todo.empty()) {
      return (int)(hi - lo);
    } else {
      return std::min((int)todo.size(), 2);
    }
  }
  
  fuel::check_type run() {
    switch (trampoline) {
      case entry: {
        continue_with(header);
        out->notify_nb(notify_threshold, lo, hi, todo, [&] (incounter_handle* h) {
          incounter::decrement(h);
        });
        break;
      }
      case header: {
        if (! todo.empty() || (hi - lo) > 0) {
          continue_with(entry);
        }
        break;
      }
      default:
        assert(false);
    }
    return fuel::check_no_promote;
  }
  
  std::pair<vertex*, vertex*> split(int nb) {
    vertex* v = nullptr;
    std::deque<outset_tree_node_type*> todo2;
    if (todo.empty()) {
      int n = (int)(hi - lo);
      assert(nb <= (int)n);
      item_iterator hi2 = lo + nb;
      v = new parallel_notify_future(out, hi2, hi, todo2);
      hi = hi2;
    } else {
      assert(nb == 1);
      auto n = todo.front();
      todo.pop_front();
      todo2.push_back(n);
      v = new parallel_notify_future(out, nullptr, nullptr, todo2);
    }
    return std::make_pair(this, v);
  }
  
};
  
} // end namespace
  
/* note: parallel_notify() parallelizes itself only if the target outset
 * represents a future; otherwise, it's going to be serial; maybe later,
 * we will find a good reason to parallelize non futures...
 */
  
void parallel_notify(bool is_future, outset* out) {
  using outset_tree_node_type = typename outset::node_type;
  using item_iterator = typename outset::item_iterator;
  outset_tree_node_type* root = out->notify_init([&] (incounter_handle* h) {
    incounter::decrement(h);
  });
  if (root == nullptr) {
    return;
  }
  std::deque<outset_tree_node_type*> todo;
  todo.push_back(root);
  item_iterator lo = nullptr;
  item_iterator hi = nullptr;
  out->notify_nb(notify_threshold, lo, hi, todo, [&] (incounter_handle* h) {
    incounter::decrement(h);
  });
  auto is_finished = [&] {
    return hi == lo && todo.empty();
  };
  if (is_finished()) {
    return;
  }
  if (is_future) {
    while (! is_finished()) {
      out->notify_nb(notify_threshold, lo, hi, todo, [&] (incounter_handle* h) {
        incounter::decrement(h);
      });
    }
  } else {
    auto v = new parallel_notify_future(out, lo, hi, todo);
    release(v);
  }
}
  
namespace {
  
class parallel_deallocate_heavy : public vertex {
public:
  
  using outset_tree_node_type = typename outset::node_type;
  
  std::deque<outset_tree_node_type*> todo;
  
  enum { entry, header };
  int trampoline = entry;
  
  void continue_with(int target) {
    trampoline = target;
    schedule(this);
  }
  
  int nb_strands() {
    return std::max((int)todo.size(), 2);
  }
  
  fuel::check_type run() {
    switch (trampoline) {
      case entry: {
        // continue_with() first so that the outset deallocator
        // gets lower priority than any vertices that are
        // unblocked by the call to deallocate_nb()
        continue_with(header);
        outset::deallocate_nb(notify_threshold, todo);
        break;
      }
      case header: {
        if (! todo.empty()) {
          continue_with(entry);
        }
        break;
      }
      default:
        assert(false);
    }
    return fuel::check_no_promote;
  }
  
  std::pair<vertex*, vertex*> split(int nb) {
    assert(todo.size() >= 2);
    assert(nb == 1);
    auto n = todo.front();
    todo.pop_front();
    auto v = new parallel_deallocate_heavy;
    v->todo.push_back(n);
    return std::make_pair(this, v);
  }
  
};
  
} // end namespace

void parallel_deallocate(outset* out) {
  using outset_tree_node_type = typename outset::node_type;
  outset_tree_node_type* root = out->get_root();
  if (root == nullptr) {
    return;
  }
  std::deque<outset_tree_node_type*> todo;
  todo.push_back(root);
  outset::deallocate_nb(notify_threshold, todo);
  if (! todo.empty()) {
    auto v = new parallel_deallocate_heavy;
    todo.swap(v->todo);
    release(v);
  }
}
  
} // end namespace
} // end namespace

#endif /*! _ENCORE_SCHEDULER_H_ */
