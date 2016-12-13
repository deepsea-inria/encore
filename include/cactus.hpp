
#include <stdlib.h>

#include "atomic.hpp"
#include "tagged.hpp"
#include "perworker.hpp"
#include "stats.hpp"

#ifndef _ENCORE_CACTUS_H_
#define _ENCORE_CACTUS_H_

namespace encore {
namespace cactus {

namespace {
  
struct chunk_struct;

static constexpr int lg_K = 12;
// size in bytes of a chunk
static constexpr int K = 1 << lg_K;
  
using chunk_tag_type = enum { trunk_tag, branch_tag };
  
using chunk_header = struct {
  chunk_tag_type tag;
  char* sp;      // used only if trunk
  std::atomic<int> refcount;
};
  
// objects of this type must be K-byte aligned
using chunk_type = struct chunk_struct {
  chunk_header hdr;
  char frames[K - sizeof(chunk_header)];
};
  
using stack_type = struct stack_struct {
  char* fp;  // points to the first byte of the newest frame
  char* sp;  // points to the first byte after the newest frame
  char* top; // points to the first byte of the oldest frame
};
  
void* aligned_alloc(size_t alignment, size_t size) {
  void* p;
  assert(alignment % sizeof(void*) == 0);
  if (posix_memalign(&p, alignment, size)) {
    p = nullptr;
  }
  return p;
}
  
char* first_free_byte_of_chunk(chunk_type* c) {
  return &(c->frames[0]);
}
  
chunk_type* new_chunk(chunk_tag_type tag, char* sp) {
  chunk_type* c = (chunk_type*)aligned_alloc(K, K);
  new (c) chunk_type();
  c->hdr.tag = tag;
  c->hdr.sp = sp;
  c->hdr.refcount.store(1);
  stats::on_stacklet_allocation();
  return c;
}
  
chunk_type* chunk_of(char* p) {
  uintptr_t p2 = (uintptr_t)p;
  p2 = p2 & ~(K - 1);
  chunk_type* res = (chunk_type*)p2;
  return res;
}
  
void incr_refcount(chunk_type* c) {
  c->hdr.refcount++;
}

void decr_refcount(chunk_type* c) {
  assert(c->hdr.refcount.load() >= 1);
  if (--c->hdr.refcount == 0) {
    free(c);
  }
}
  
char* first_byte_of_frame(char* fp) {
  return fp + sizeof(char*);
}
  
} // end namespace
  
inline
bool empty(struct stack_struct s) {
  return s.fp == nullptr;
}
  
stack_type new_stack() {
  stack_struct s;
  s.fp = nullptr;
  s.sp = first_free_byte_of_chunk(new_chunk(trunk_tag, s.fp));
  s.top = s.sp;
  assert(s.sp != nullptr);
  return s;
}
  
void delete_stack(stack_type s) {
  decr_refcount(chunk_of(s.sp - 1));
}
  
template <class Activation_record, class ...Args>
stack_type push_back(stack_type s, Args... args) {
  stack_type t = s;
  auto b = sizeof(char*) + sizeof(Activation_record);
  t.fp = s.sp;
  t.sp = s.sp + b;
  if (t.sp >= ((char*)chunk_of(s.sp)) + K) {
    chunk_type* c = new_chunk(trunk_tag, s.sp);
    t.fp = first_free_byte_of_chunk(c);
    t.sp = t.fp + b;
  }
  *((char**)t.fp) = s.fp;
  new (first_byte_of_frame(t.fp)) Activation_record(args...);
  assert(t.sp != nullptr);
  return t;
}
  
template <class Activation_record>
stack_type pop_back(stack_type s) {
  assert(! empty(s));
  char* p = first_byte_of_frame(s.fp);
  ((Activation_record*)(p))->~Activation_record();
  stack_type t = s;
  t.fp = *((char**)s.fp);
  t.sp = s.fp;
  chunk_type* c_s = chunk_of(s.fp);
  chunk_type* c_t = chunk_of(t.fp);
  if (empty(t)) {
    chunk_type* c2 = chunk_of(s.sp - 1);
    if (c2 != c_s) {
      assert(c_s != nullptr);
      decr_refcount(c_s);
      t.sp = s.sp;
    }
  } else if (c_s == c_t) {
    // do nothing
  } else if (c_s->hdr.tag == trunk_tag) {
    t.sp = c_s->hdr.sp;
    decr_refcount(c_s);
  } else if (c_s->hdr.tag == branch_tag) {
    decr_refcount(c_s);
    chunk_type* c = new_chunk(branch_tag, nullptr);
    t.sp = first_free_byte_of_chunk(c);
  } else {
    assert(false);
  }
  return t;
}
  
template <class Activation_record>
inline
Activation_record& peek_back(stack_type s) {
  assert(! empty(s));
  return *((Activation_record*)(first_byte_of_frame(s.fp)));
}
  
template <class Activation_record>
Activation_record& peek_front(stack_type s) {
  assert(! empty(s));
  assert(s.top != nullptr);
  return *((Activation_record*)(first_byte_of_frame(s.top)));
}
  
std::pair<stack_type, stack_type> fork_front(stack_type s, int activation_record_szb) {
  assert(! empty(s));
  incr_refcount(chunk_of(s.top));
  stack_type s1 = s;
  s1.fp = s.top;
  chunk_type* c = new_chunk(branch_tag, nullptr);
  s1.sp = first_free_byte_of_chunk(c);
  stack_type s2 = s;
  int b = sizeof(char*) + activation_record_szb;
  s2.top = s.top + b;
  *((char**)s2.top) = nullptr;
  return std::make_pair(s1, s2);
}

template <class Activation_record>
std::pair<stack_type, stack_type> fork_front(stack_type s) {
  return fork_front(s, sizeof(Activation_record));
}
 
template <class Activation_record>
std::pair<stack_type, stack_type> slice_front(stack_type s) {
  return fork_front(s, sizeof(Activation_record));
}
  
} // end namespace
} // end namespace

#endif /*! _ENCORE_CACTUS_H_ */
