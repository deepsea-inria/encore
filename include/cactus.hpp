
#include <stdlib.h>

#include "atomic.hpp"
#include "tagged.hpp"
#include "perworker.hpp"

#ifndef _ENCORE_CACTUS_H_
#define _ENCORE_CACTUS_H_

namespace encore {
namespace cactus {

namespace {
  
static constexpr int lg_K = 12;

static constexpr int K = 1 << lg_K;
  
struct chunk_struct;
  
static constexpr int F = K - sizeof(chunk_struct*);
  
using chunk_type = struct chunk_struct {
  char frames[F];
  struct chunk_struct* next;
};
  
using stack_type = struct stack_struct {
  char* fp;  // to point at the first byte of the newest frame
  char* sp;  // to point at the first byte after the newest frame
  char* top; // to point at the first byte of the oldest frame
};
  
using descriptor_type = struct {
  
  enum { empty_tag, frame_tag, overflow_tag, initial_tag } tag;
  
  union {
    
    struct {
      char* fp;
      char* sp;
    } frame;
    
    struct {
      chunk_type* c;
    } overflow;
    
  };
};
  
void* aligned_alloc(size_t alignment, size_t size) {
  void* p;
  assert(alignment % sizeof(void*) == 0);
  if (posix_memalign(&p, alignment, size)) {
    p = nullptr;
  }
  return p;
}
  
chunk_type* allocate_chunk() {
  chunk_type* c = (chunk_type*)aligned_alloc(K, K);
  new (c) chunk_type();
  c->next = nullptr;
  return c;
}
  
chunk_type* chunk_of(char* p) {
  uintptr_t p2 = (uintptr_t)p;
  p2 = p2 & ~(K - 1);
  chunk_type* res = (chunk_type*)p2;
  return res;
}
  
static constexpr int freelist_min_refill_size = 5;
  
data::perworker::array<chunk_type*> freelist(nullptr);
  
void fill_freelist_if_empty() {
  chunk_type*& head = freelist.mine();
  if (head == nullptr) {
    for (int i = 0; i < freelist_min_refill_size; i++) {
      chunk_type* c = allocate_chunk();
      c->next = head;
      head = c;
    }
  }
  assert(head != nullptr);
}

chunk_type* new_chunk() {
  chunk_type* c;
  fill_freelist_if_empty();
  chunk_type*& head = freelist.mine();
  c = head;
  head = c->next;
  c->next = nullptr;
  return c;
}

void delete_chunk(chunk_type* c) {
  chunk_type*& head = freelist.mine();
  c->next = head;
  head = c;
}
  
void create_overflow(descriptor_type*& d, char*& fp) {
  chunk_type* next_chunk = new_chunk();
  d->tag = descriptor_type::overflow_tag;
  d->overflow.c = next_chunk;
  d = (descriptor_type*)next_chunk;
  fp = (char*)next_chunk + sizeof(descriptor_type);
}
  
} // end namespace
  
bool empty(struct stack_struct s) {
  return s.fp == nullptr;
}

void empty_freelist() {
  chunk_type*& head = freelist.mine();
  chunk_type* c = head;
  while (c != nullptr) {
    chunk_type* t = c->next;
    free(c);
    c = t;
  }
}
  
stack_type new_stack() {
  stack_struct s;
  chunk_type* c = new_chunk();
  s.sp = (char*)c;
  s.top = s.sp;
  s.fp = nullptr;
  ((descriptor_type*)s.sp)->tag = descriptor_type::initial_tag;
  return s;
}
  
void delete_stack(stack_type s) {
  assert(empty(s));
  chunk_type* c = chunk_of(s.fp);
  if ((char*)c + sizeof(descriptor_type) == s.top) {
    delete_chunk(c);
  }
}

template <class Activation_record, class ...Args>
stack_type push_back(stack_type s, Args... args) {
  stack_type t = s;
  descriptor_type* current_descriptor = (descriptor_type*)s.sp;
  switch (current_descriptor->tag) {
    case descriptor_type::empty_tag: {
      chunk_type* current_chunk = chunk_of(s.fp);
      if (s.sp + sizeof(Activation_record) + sizeof(descriptor_type) <= (char*)current_chunk + F) {
        t.fp = s.sp + sizeof(descriptor_type);
      } else {
        create_overflow(current_descriptor, t.fp);
      }
      break;
    }
    case descriptor_type::overflow_tag: {
      create_overflow(current_descriptor, t.fp);
      break;
    }
    case descriptor_type::initial_tag: {
      assert(s.fp == nullptr);
      t.fp = s.sp + sizeof(descriptor_type);
      t.top = t.fp;
      break;
    }
    default: {
      assert(false);
    }
  }
  current_descriptor->tag = descriptor_type::frame_tag;
  current_descriptor->frame.fp = s.fp;
  current_descriptor->frame.sp = s.sp;
  t.sp = t.fp + sizeof(Activation_record);
  new (t.fp) Activation_record(args...);
  ((descriptor_type*)t.sp)->tag = descriptor_type::empty_tag;
  return t;
}

template <class Activation_record>
stack_type pop_back(stack_type s) {
  assert(! empty(s));
  stack_type t = s;
  ((Activation_record*)s.fp)->~Activation_record();
  descriptor_type* current_descriptor = (descriptor_type*)(t.fp - sizeof(descriptor_type));
  switch (current_descriptor->tag) {
    case descriptor_type::frame_tag: {
      current_descriptor->tag = descriptor_type::empty_tag;
      char* fp = current_descriptor->frame.fp;
      char* sp = current_descriptor->frame.sp;
      chunk_type* current_chunk = chunk_of(s.fp);
      if (current_chunk != chunk_of(fp)) {
        delete_chunk(current_chunk);
      }
      t.fp = fp;
      t.sp = sp;
      break;
    }
    case descriptor_type::empty_tag:
    case descriptor_type::overflow_tag: {
      t.fp = nullptr;
      break;
    }
    default: {
      assert(false);
    }
  }
  return t;
}
  
template <class Activation_record>
Activation_record& peek_back(stack_type s) {
  assert(! empty(s));
  return *((Activation_record*)s.fp);
}
  
template <class Activation_record>
Activation_record& peek_front(stack_type s) {
  assert(! empty(s));
  assert(s.top != nullptr);
  return *((Activation_record*)s.top);
}
  
template <class Activation_record>
std::pair<stack_type, stack_type> fork_front(stack_type s) {
  assert(! empty(s));
  stack_type s1, s2;
  s1.top = s.top;
  s1.fp = s1.top;
  s1.sp = s1.fp + sizeof(Activation_record);
  descriptor_type* next_descriptor = (descriptor_type*)s1.sp;
  switch (next_descriptor->tag) {
    case descriptor_type::frame_tag: {
      next_descriptor->tag = descriptor_type::overflow_tag;
      next_descriptor->overflow.c = nullptr;
      s2.top = s.top + sizeof(Activation_record) + sizeof(descriptor_type);
      break;
    }
    case descriptor_type::overflow_tag: {
      chunk_type* c = next_descriptor->overflow.c;
      if (c == nullptr) {
        s2.top = nullptr;
        s2.fp = nullptr;
        s2.sp = nullptr;
        return std::make_pair(s1, s2);
      }
      s2.top = (char*)c + sizeof(descriptor_type);
      break;
    }
    case descriptor_type::empty_tag: {
      s2.top = nullptr;
      s2.fp = nullptr;
      s2.sp = nullptr;
      return std::make_pair(s1, s2);
    }
    default: {
      assert(false);
    }
  }
  s2.fp = s.fp;
  s2.sp = s.sp;
  return std::make_pair(s1, s2);
}
 
template <class Activation_record>
std::pair<stack_type, stack_type> slice_front(stack_type s) {
  assert(! empty(s));
  stack_type s1, s2;
  s1.top = s.top;
  s1.fp = s1.top;
  s1.sp = s1.fp + sizeof(Activation_record);
  descriptor_type* next_descriptor = (descriptor_type*)s1.sp;
  switch (next_descriptor->tag) {
    case descriptor_type::frame_tag: {
      s2.top = s1.sp + sizeof(descriptor_type);
      break;
    }
    case descriptor_type::overflow_tag: {
      chunk_type* c = next_descriptor->overflow.c;
      if (c == nullptr) {
        s2.top = nullptr;
        s2.fp = nullptr;
        s2.sp = nullptr;
        return std::make_pair(s1, s2);
      }
      s2.top = (char*)c + sizeof(descriptor_type);
      next_descriptor = (descriptor_type*)c;
      break;
    }
    case descriptor_type::empty_tag: {
      s2.top = nullptr;
      s2.fp = nullptr;
      s2.sp = nullptr;
      return std::make_pair(s1, s2);
    }
    default: {
      assert(false);
    }
  }
  assert(next_descriptor->tag == descriptor_type::frame_tag);
  assert(next_descriptor->frame.fp != nullptr);
  next_descriptor->tag = descriptor_type::empty_tag;
  assert(s.fp != s.top);
  s2.fp = s.fp;
  s2.sp = s.sp;
  return std::make_pair(s1, s2);
}
  
} // end namespace
} // end namespace

#endif /*! _ENCORE_CACTUS_H_ */
