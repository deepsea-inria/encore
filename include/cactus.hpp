
#include <stdlib.h>

#include "atomic.hpp"
#include "tagged.hpp"

#ifndef _PASL_CACTUS_H_
#define _PASL_CACTUS_H_

namespace pasl {
namespace cactus {

namespace {
  
static constexpr int lg_K = 10;

// size of a chunk
static constexpr int K = 1 << lg_K;
  
static constexpr int lg_B = 4; // must be < lg_K
// number of buckets to be used by the successor table
static constexpr int B = 1 << lg_B;

using version_number_type = char*;
  
struct chunk_struct;
  
class successor_table {
private:
  
  using node_type = struct node_struct {
    version_number_type version;
    struct chunk_struct* chunk;
    struct node_struct* next;
  };
  
  static constexpr int tag_locked = 1;
  
  std::atomic<node_type*> table[B];
  
  int bucket_of(version_number_type v) {
    uintptr_t p2 = (uintptr_t)v;
    p2 = p2 & (K - 1);
    p2 = p2 >> (lg_K - lg_B);
    int bucket = (int)p2;
    assert(bucket >= 0);
    assert(bucket < B);
    return bucket;
  }
  
  // loop until taking the lock on bucket b
  node_type* lock_bucket(int b) {
    node_type* n = nullptr;
    while (true) {
      n = table[b].load();
      if (tagged::tag_of(n) != tag_locked) {
        node_type* oldv = n;
        node_type* newv = tagged::tag_with(n, tag_locked);
        if (atomic::compare_exchange(table[b], oldv, newv)) {
          break;
        }
      }
    }
    return n;
  }
  
  node_type* find_node_by_version(node_type* n, version_number_type v) {
    node_type* m = n;
    while (m != nullptr) {
      if (m->version == v) {
        break;
      }
      m = m->next;
    }
    return m;
  }
  
  node_type* remove_node_by_version(node_type* n, version_number_type v) {
    node_type* m = nullptr;
    node_type* a = n;
    while (a != nullptr) {
      node_type* t = a->next;
      if (a->version == v) {
        delete a;
        a = t;
        continue;
      }
      a->next = m;
      m = a;
      a = t;
    }
    return m;
  }
  
public:
  
  successor_table() {
    for (int i = 0; i < B; i++) {
      table[i].store(nullptr);
    }
  }
  
  ~successor_table() {
    for (int i = 0; i < B; i++) {
      node_type* n = table[i].load();
      while (n != nullptr) {
        node_type* m = n;
        n = m->next;
        delete m;
      }
    }
  }
  
  bool empty() const {
    for (int i = 0; i < B; i++) {
      if (table[i].load() != nullptr) {
        return false;
      }
    }
    return true;
  }
  
  struct chunk_struct* find(version_number_type v) {
    struct chunk_struct* c = nullptr;
    int b = bucket_of(v);
    node_type* n = lock_bucket(b);
    // bucket b now locked
    node_type* m = find_node_by_version(n, v);
    if (m != nullptr) {
      c = m->chunk;
    }
    table[b].store(n);
    // bucket b now unlocked
    return c;
  }
  
  void insert(version_number_type v, struct chunk_struct* c) {
    int b = bucket_of(v);
    node_type* n = lock_bucket(b);
    // bucket b now locked
    node_type* m = find_node_by_version(n, v);
    if (m == nullptr) {
      // there was no node representing version v
      // so create one and add it to the bucket
      node_type* p = new node_type;
      p->version = v;
      p->chunk = c;
      p->next = n;
      n = p;
    } else {
      m->chunk = c;
    }
    // version v updated to chunk c locally
    table[b].store(n);
    // bucket b now unlocked, update visible globally
  }
  
  void remove(version_number_type v) {
    int b = bucket_of(v);
    node_type* n = lock_bucket(b);
    // bucket b now locked
    node_type* m = remove_node_by_version(n, v);
    // version v updated to chunk c locally
    table[b].store(m);
    // bucket b now unlocked, update visible globally
  }
  
};
  
using descriptor_type = struct {
  char* predecessor;
  successor_table successors;
  struct chunk_struct* overflow;
};
  
static constexpr int F = K - sizeof(descriptor_type);
  
using chunk_type = struct chunk_struct {
  char frames[F];
  descriptor_type descriptor;
};
  
struct stack_struct {
  char* first;
  char* last;
};
  
// for some reason, this function is provisioned by c11 standard
// but not provided by mac os; therefore, i'm implementing it
// here, for the moment
void* aligned_alloc(size_t alignment, size_t size) {
  void* p;
  assert(alignment % sizeof(void*) == 0);
  if (posix_memalign(&p, alignment, size)) {
    p = nullptr;
  }
  return p;
}
  
chunk_type* new_chunk() {
  chunk_type* c = (chunk_type*)aligned_alloc(K, K);
  new (c) chunk_struct();
  c->descriptor.predecessor = nullptr;
  c->descriptor.overflow = nullptr;
  return c;
}
  
void delete_chunk(chunk_type* c) {
  assert(c->descriptor.successors.empty());
  chunk_type* overflow = c->descriptor.overflow;
  c->descriptor.overflow = nullptr;
  if (overflow != nullptr) {
    delete_chunk(overflow);
  }
  c->~chunk_struct();
  free(c);
}
  
chunk_type* chunk_of(char* p) {
  uintptr_t p2 = (uintptr_t)p;
  p2 = p2 & ~(K - 1);
  chunk_type* res = (chunk_type*)p2;
  return res;
}

bool empty(struct stack_struct s) {
  return s.first == s.last;
}

// type of the field to be used to determine the
// size in bytes of a given frame
using frame_size_type = int;
  
frame_size_type szb_of_frame(frame_size_type frame_contents_szb) {
  return frame_contents_szb + sizeof(frame_size_type);
}
  
// returns the number of bytes required to store the given
// activation record in the stack
template <class Activation_record>
frame_size_type szb_of_frame() {
  // the second term accounts for space to store the
  // frame-size descriptor
  return szb_of_frame(sizeof(Activation_record));
}
  
} // end namespace
  
using stack_type = struct stack_struct;
  
stack_type new_stack() {
  stack_struct s;
  chunk_type* c = new_chunk();
  s.first = (char*)c;
  s.last = s.first;
  return s;
}
  
void delete_stack(stack_type s) {
  assert(empty(s));
  chunk_type* c = chunk_of(s.last);
  if ((char*)c == s.first) {
    delete_chunk(c);
  }
}

template <class Activation_record, class ...Args>
stack_type push_back(stack_type s, Args... args) {
  assert(szb_of_frame<Activation_record>() + sizeof(descriptor_type) <= sizeof(chunk_type));
  stack_type t = s;
  char* old_last = s.last;
  char* new_last = old_last + szb_of_frame<Activation_record>();
  if (new_last > (char*)chunk_of(old_last) + F) {
    chunk_type* old_chunk = chunk_of(old_last);
    chunk_type* succ_chunk = old_chunk->descriptor.overflow;
    if (succ_chunk == nullptr) {
      succ_chunk = new_chunk();
    } else {
      old_chunk->descriptor.overflow = nullptr;
    }
    old_chunk->descriptor.successors.insert(old_last, succ_chunk);
    succ_chunk->descriptor.predecessor = old_last;
    new_last = (char*)succ_chunk + szb_of_frame<Activation_record>();
  }
  t.last = new_last;
  new (t.last - szb_of_frame<Activation_record>()) Activation_record(args...);
  new (t.last - sizeof(frame_size_type)) frame_size_type(sizeof(Activation_record));
  return t;
}

template <class Activation_record>
stack_type pop_back(stack_type s) {
  stack_type t = s;
  char* old_last = s.last;
  chunk_type* old_chunk = chunk_of(old_last);
  if (old_last == (char*)old_chunk) {
    old_last = old_chunk->descriptor.predecessor;
    if (old_chunk->descriptor.overflow != nullptr) {
      old_chunk->descriptor.overflow = nullptr;
    }
    chunk_type* pred_chunk = chunk_of(old_last);
    pred_chunk->descriptor.successors.remove(old_last);
    pred_chunk->descriptor.overflow = old_chunk;
  }
  char* new_last = old_last - szb_of_frame(((frame_size_type*)old_last)[-1]);
  t.last = new_last;
  ((Activation_record*)new_last)->~Activation_record();
  return t;
}
  
template <class Activation_record>
Activation_record& peek_back(stack_type s) {
  char* old_last = s.last;
  chunk_type* old_chunk = chunk_of(old_last);
  if (old_last == (char*)old_chunk) {
    old_last = old_chunk->descriptor.predecessor;
    assert(old_last != nullptr);
  }
  char* new_last = old_last - szb_of_frame(((frame_size_type*)old_last)[-1]);
  return *((Activation_record*)new_last);
}
  
template <class Activation_record>
Activation_record& peek_front(stack_type s) {
  return *((Activation_record*)s.first);
}
  
template <class Activation_record>
std::pair<stack_type, stack_type> split_front(stack_type s) {
  stack_type s1, s2;
  char* old_first = s.first;
  char* new_first = old_first + szb_of_frame<Activation_record>();
  chunk_type* old_chunk = chunk_of(old_first);
  chunk_type* s1_succ_chunk = new_chunk();
  s1.first = old_first;
  s1.last = (char*)s1_succ_chunk;
  s1_succ_chunk->descriptor.predecessor = new_first;
  chunk_type* s2_chunk = old_chunk->descriptor.successors.find(new_first);
  old_chunk->descriptor.successors.insert(new_first, s1_succ_chunk);
  if (s2_chunk == nullptr) {
    s2.first = new_first;
  } else {
    old_chunk->descriptor.successors.remove(new_first);
    s2_chunk->descriptor.predecessor = nullptr;
    s2.first = (char*)s2_chunk;
  }
  s2.last = s.last;
  return std::make_pair(s1, s2);
}
  
} // end namespace
} // end namespace

#endif /*! _PASL_CACTUS_H_ */