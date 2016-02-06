
#include <atomic>

#include "aligned.hpp"

#ifndef _PASL_PERWORKER_H_
#define _PASL_PERWORKER_H_

namespace pasl {
namespace data {
namespace perworker {
  
namespace {
  
static constexpr int default_max_nb_workers = 128;
  
std::atomic<int> fresh_id(0);
  
} // end namespace
  
class my_fresh_id {
public:
  
  int operator()() {
    return fresh_id++;
  }
  
};
  
class my_encore_id {
public:
  
  // ...
  
};
  
  // later: make default for My_id the encore built-in one, when it exists
  
int my_id() {
  my_fresh_id id;
  return id();
}

template <class Item, class My_id=my_fresh_id, int max_nb_workers=default_max_nb_workers>
class array {
private:
  
  cache_aligned_fixed_capacity_array<Item, max_nb_workers> items;
  
  int get_my_id() {
    My_id my_id;
    int id = my_id();
    assert(id >= 0);
    assert(id < max_nb_workers);
    return id;
  }
  
public:
  
  array() { }
  
  array(const Item& x)
  : items(x) { }
  
  array(std::initializer_list<Item> l) {
    assert(l.size() == 1);
    items.init(*(l.begin()));
  }
  
  Item& mine() {
    return items[get_my_id()];
  }
  
  Item& operator[](std::size_t i) {
    assert(i >= 0);
    assert(i < max_nb_workers);
    return items[i];
  }
  
  void init(const Item& x) {
    items.init(x);
  }
  
};
  
  
} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_PERWORKER_H_ */