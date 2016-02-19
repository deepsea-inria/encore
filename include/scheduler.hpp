
#include <vector>
#include <deque>
#include <thread>

#include "vertex.hpp"
#include "perworker.hpp"
#include "chunkedseq.hpp"

#ifndef _PASL_SCHED_SCHEDULER_H_
#define _PASL_SCHED_SCHEDULER_H_

namespace pasl {
namespace sched {
  
/*---------------------------------------------------------------------*/
/* Scheduling parameters */
  
// to control the rate of DAG construction
static constexpr int D = 2000;

// to control the eagerness of work distribution
static constexpr int K = 2 * D;
  
/*---------------------------------------------------------------------*/
/* Frontier */
  
namespace {
  
int vertex_run(int fuel, vertex* v) {
  assert(v->nb_strands() > 0);
  fuel = v->run(fuel);
  if (v->nb_strands() == 0) {
    parallel_notify(v->is_future, v->get_out());
    delete v;
    fuel--;
  }
  return fuel;
}
  
class frontier {
private:
  
  class measure_env {
  public:
    
    int operator()(vertex* v) const {
      return v->nb_strands();
    }
    
  };
  
  static constexpr int chunk_capacity = 512;
  using cache_type = data::cachedmeasure::weight<vertex*, int, size_t, measure_env>;
  using weighted_seq_type = data::chunkedseq::bootstrapped::stack<vertex*, chunk_capacity, cache_type>;
  
  weighted_seq_type vs;
  
public:
  
  int nb_strands() {
    return vs.get_cached();
  }
  
  bool empty() {
    return nb_strands() == 0;
  }
  
  void push(vertex* v) {
    assert(v->nb_strands() > 0);
    vs.push_back(v);
  }
  
  int run(int fuel) {
    while (fuel > 0 && ! vs.empty()) {
      vertex* v = vs.pop_back();
      fuel = vertex_run(fuel, v);
    }
    return fuel;
  }
  
  void split(int nb, frontier& other) {
    vertex* v = nullptr;
    vs.split([&] (int n) { return nb <= n; }, v, other.vs);
    int spill = (v->nb_strands() + other.nb_strands()) - nb;
    assert(spill >= 0);
    if (spill == 0) {
      other.vs.push_back(v);
    } else {
      vertex* v2 = v->split(spill);
      bool b = v2->release_handle->decrement();
      assert(b);
      other.vs.push_back(v2);
      assert(v->nb_strands() > 0);
      vs.push_front(v);
    }
  }
  
  void swap(frontier& other) {
    vs.swap(other.vs);
  }
  
};
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Trivial uniprocessor scheduler, just for debugging */
  
namespace uniprocessor {

std::vector<vertex*> ready;
  
void scheduler_loop() {
  int fuel = D;
  while (! ready.empty()) {
    vertex* v = ready.back();
    ready.pop_back();
    fuel = vertex_run(fuel, v);
    if (fuel == 0) {
      fuel = D;
    }
  }
}
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Parallel work-stealing scheduler */
  
namespace {
  
std::atomic<int> nb_active_workers; // later: user SNZI instead
  
data::perworker::array<std::atomic<bool>> status;

static constexpr int no_request = -1;
  
data::perworker::array<std::atomic<int>> request;
  
frontier* no_response = tagged::tag_with((frontier*)nullptr, 1);
  
data::perworker::array<std::atomic<frontier*>> transfer;
  
data::perworker::array<std::mt19937> scheduler_rngs;  // random-number generators
  
void scheduler_init() {
  nb_active_workers.store(1);
  status.for_each([&] (int, std::atomic<bool>& b) {
    b.store(false);
  });
  request.for_each([&] (int, std::atomic<int>& r) {
    r.store(no_request);
  });
  transfer.for_each([&] (int, std::atomic<frontier*>& t) {
    t.store(no_response);
  });
}
  
int random_other_worker(int my_id) {
  int P = data::perworker::get_nb_workers();
  std::uniform_int_distribution<int> distribution(0, 2*P);
  int i = distribution(scheduler_rngs[my_id]) & (P - 1);
  if (i >= my_id) {
    i++;
  }
  return i;
}
  
bool is_finished() {
  return nb_active_workers.load() == 0;
}
  
// one instance of this function is to be run by each
// participating worker thread
void worker_loop(vertex* v) {
  int my_id = data::perworker::get_my_id();
  frontier my_ready;
  int fuel = D;
  int nb = 0;
  
  if (v != nullptr) {
    my_ready.push(v);
  }
  
  // update the status flag
  auto update_status = [&] {
    bool b = (my_ready.nb_strands() > 1);
    if (status[my_id].load() != b) {
      status[my_id].store(b);
    }
  };
  
  // check for an incoming steal request
  // returns true iff a migration was performed by the call
  auto communicate = [&] {
    bool migrated_work = false;
    int j = request[my_id].load();
    if (j != no_request) {
      int sz = my_ready.nb_strands();
      if (sz > K || (nb > K && sz > 1)) {
        nb_active_workers++;
        nb = 0;
        // transfer half of the local frontier to worker with id j
        frontier* f = new frontier;
        my_ready.split(sz / 2, *f);
        transfer[j].store(f);
        migrated_work = true;
      } else {
        transfer[j].store(nullptr); // reject query
      }
    }
    request[my_id].store(no_request);
    return migrated_work;
  };
  
  // called by workers when running out of work
  auto acquire = [&] {
    nb_active_workers--;
    while (true) {
      transfer[my_id].store(no_response);
      int k = random_other_worker(my_id);
      int orig = no_request;
      if (status[k].load() && request[k].compare_exchange_strong(orig, my_id)) {
        while (transfer[my_id].load() == no_response) {
          communicate();
        }
        frontier* f = transfer[my_id].load();
        if (f != nullptr) {
          f->swap(my_ready);
        }
        request[my_id].store(no_request);
        return;
      }
      communicate();
    }
  };
  
  while (true) {
    if (my_ready.nb_strands() == 0) {
      if (is_finished()) {
        return;
      }
      fuel = D;
      acquire();
    } else {
      if (communicate()) {
        fuel = D;
      }
      int t = my_ready.run(fuel);
      nb += fuel - t;
      fuel = t;
      update_status();
    }
  }
}

} // end namespace
  
void launch(int nb_workers, vertex* v) {
  scheduler_init();
  for (int i = 1; i < nb_workers; i++) {
    std::thread t([] {
      worker_loop(nullptr);
    });
    t.detach();
  }
  worker_loop(v);
}
  
/*---------------------------------------------------------------------*/
/* Scheduling primitives */
  
void reset_incounter(vertex* v) {
  v->reset_incounter();
}
  
void schedule(vertex* v) {
  uniprocessor::ready.push_back(v);
}

void new_edge(vertex* source, vertex* destination) {
  incounter_handle* h = destination->get_in()->increment(destination);
  bool success = source->get_out()->insert(h);
  if (! success) {
    destination->get_in()->decrement(h);
  }
}
  
void release(vertex* v) {
  assert(v != nullptr);
  assert(v->release_handle != nullptr);
  assert(v->get_in() != nullptr);
  v->get_in()->decrement(v->release_handle);
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
  
  int run(int fuel) {
    switch (trampoline) {
      case entry: {
        continue_with(header);
        out->notify_nb(fuel, lo, hi, todo, [&] (incounter_handle* h) {
          incounter::decrement(h);
        });
        break;
      }
      case header: {
        if (! todo.empty() || (hi - lo) > 0) {
          continue_with(entry);
        }
        fuel--;
        break;
      }
      default:
        assert(false);
    }
    return fuel;
  }
  
  vertex* split(int nb) {
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
    return v;
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
  out->notify_nb(D, lo, hi, todo, [&] (incounter_handle* h) {
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
      out->notify_nb(D, lo, hi, todo, [&] (incounter_handle* h) {
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
  
  int run(int fuel) {
    switch (trampoline) {
      case entry: {
        // continue_with() first so that the outset deallocator
        // gets lower priority than any vertices that are
        // unblocked by the call to deallocate_nb()
        continue_with(header);
        outset::deallocate_nb(fuel, todo);
        fuel -= std::min(fuel, D);
        break;
      }
      case header: {
        if (! todo.empty()) {
          continue_with(entry);
        }
        fuel--;
        break;
      }
      default:
        assert(false);
    }
    return fuel;
  }
  
  vertex* split(int nb) {
    assert(todo.size() >= 2);
    assert(nb == 1);
    auto n = todo.front();
    todo.pop_front();
    auto v = new parallel_deallocate_heavy;
    v->todo.push_back(n);
    return v;
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
  outset::deallocate_nb(D, todo);
  if (! todo.empty()) {
    auto v = new parallel_deallocate_heavy;
    todo.swap(v->todo);
    release(v);
  }
}
  
} // end namespace
} // end namespace

#endif /*! _PASL_SCHED_SCHEDULER_H_ */