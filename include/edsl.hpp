
#include <functional>
#include <vector>
#include <map>
#include <algorithm>
#include <memory>
#include <array>

#include "vertex.hpp"
#include "cactus.hpp"
#include "stats.hpp"

#ifndef _ENCORE_EDSL_H_
#define _ENCORE_EDSL_H_

namespace encore {
namespace edsl {
  
/*---------------------------------------------------------------------*/
/* Parallel Control-Flow Graph */
  
namespace pcfg {
  
using stack_type = std::pair<cactus::stack_type, cactus::stack_type>;
  
class shared_activation_record;
  
using extended_stack_type = struct {
  shared_activation_record* oldest_shared;
  stack_type stack;
};
  
using basic_block_label_type = int;

static constexpr basic_block_label_type entry_block_label = 0;
static constexpr basic_block_label_type exit_block_label = -1;
  
static constexpr int suspend_tag = -1;

using basic_block_tag_type = enum {
  tag_unconditional_jump, tag_conditional_jump,
  tag_spawn_join, tag_spawn2_join,
  tag_tail,
  tag_join_plus, tag_spawn_minus,
  tag_spawn_plus, tag_join_minus,
  tag_none
};

template <class Shared_activation_record, class Private_activation_record>
class basic_block_type {
public:
  
  using sar_type = Shared_activation_record;
  using par_type = Private_activation_record;
  
  using unconditional_jump_code_type = std::function<void(sar_type&, par_type&)>;
  using conditional_jump_code_type = std::function<int(sar_type&, par_type&)>;
  using procedure_call_code_type = std::function<stack_type(sar_type&, par_type&, stack_type)>;
  using incounter_getter_code_type = std::function<sched::incounter**(sar_type&, par_type&)>;
  using outset_getter_code_type = std::function<sched::outset**(sar_type&, par_type&)>;
  
  basic_block_tag_type tag;
  
  union {
    struct {
      unconditional_jump_code_type code;
      basic_block_label_type next;
    } variant_unconditional_jump;
    struct {
      conditional_jump_code_type code;
      std::vector<basic_block_label_type> targets;
    } variant_conditional_jump;
    struct {
      procedure_call_code_type code;
      basic_block_label_type next;
    } variant_spawn_join;
    struct {
      procedure_call_code_type code1;
      basic_block_label_type next; // must be the label of a spawn_join block
    } variant_spawn2_join;
    struct {
      procedure_call_code_type code;
    } variant_tail;
    struct {
      procedure_call_code_type code;
      incounter_getter_code_type getter;
      basic_block_label_type next;
    } variant_join_plus;
    struct {
      procedure_call_code_type code;
      incounter_getter_code_type getter;
      basic_block_label_type next;
    } variant_spawn_minus;
    struct {
      procedure_call_code_type code;
      outset_getter_code_type getter;
      basic_block_label_type next;
    } variant_spawn_plus;
    struct {
      outset_getter_code_type getter;
      basic_block_label_type next;
    } variant_join_minus;
  };
  
  basic_block_type() : tag(tag_none) { }
  
private:
  
  void copy_constructor(basic_block_type const& other) {
    tag = other.tag;
    switch (tag) {
      case tag_unconditional_jump: {
        new (&variant_unconditional_jump.code) unconditional_jump_code_type(other.variant_unconditional_jump.code);
        variant_unconditional_jump.next = other.variant_unconditional_jump.next;
        break;
      }
      case tag_conditional_jump: {
        new (&variant_conditional_jump.code) conditional_jump_code_type(other.variant_conditional_jump.code);
        new (&variant_conditional_jump.targets) std::vector<basic_block_label_type>(other.variant_conditional_jump.targets);
        break;
      }
      case tag_spawn_join: {
        new (&variant_spawn_join.code) procedure_call_code_type(other.variant_spawn_join.code);
        variant_spawn_join.next = other.variant_spawn_join.next;
        break;
      }
      case tag_spawn2_join: {
        new (&variant_spawn2_join.code1) procedure_call_code_type(other.variant_spawn2_join.code1);
        variant_spawn2_join.next = other.variant_spawn2_join.next;
        break;
      }
      case tag_tail: {
        new (&variant_tail.code) procedure_call_code_type(other.variant_tail.code);
        break;
      }
      case tag_join_plus: {
        new (&variant_join_plus.code) procedure_call_code_type(other.variant_join_plus.code);
        new (&variant_join_plus.getter) incounter_getter_code_type(other.variant_join_plus.getter);
        variant_join_plus.next = other.variant_join_plus.next;
        break;
      }
      case tag_spawn_minus: {
        new (&variant_spawn_minus.code) procedure_call_code_type(other.variant_spawn_minus.code);
        new (&variant_spawn_minus.getter) incounter_getter_code_type(other.variant_spawn_minus.getter);
        variant_spawn_minus.next = other.variant_spawn_minus.next;
        break;
      }
      case tag_spawn_plus: {
        new (&variant_spawn_plus.code) procedure_call_code_type(other.variant_spawn_plus.code);
        new (&variant_spawn_plus.getter) outset_getter_code_type(other.variant_spawn_plus.getter);
        variant_spawn_plus.next = other.variant_spawn_plus.next;
        break;
      }
      case tag_join_minus: {
        new (&variant_join_minus.getter) outset_getter_code_type(other.variant_join_minus.getter);
        variant_join_minus.next = other.variant_join_minus.next;
        break;
      }
      default:
        break;
    }
  }
  
public:
  
  basic_block_type(basic_block_type const& other) {
    copy_constructor(other);
  }
  
private:
  
  void move_constructor(basic_block_type&& other) {
    tag = other.tag;
    other.tag = tag_none;
    switch (tag) {
      case tag_unconditional_jump: {
        new (&variant_unconditional_jump.code) unconditional_jump_code_type;
        variant_unconditional_jump.code = std::move(other.variant_unconditional_jump.code);
        variant_unconditional_jump.next = std::move(other.variant_unconditional_jump.next);
        break;
      }
      case tag_conditional_jump: {
        new (&variant_conditional_jump.code) conditional_jump_code_type;
        variant_conditional_jump.code = std::move(other.variant_conditional_jump.code);
        new (&variant_conditional_jump.targets) std::vector<basic_block_label_type>;
        variant_conditional_jump.targets = std::move(other.variant_conditional_jump.targets);
        break;
      }
      case tag_spawn_join: {
        new (&variant_spawn_join.code) procedure_call_code_type;
        variant_spawn_join.code = std::move(other.variant_spawn_join.code);
        variant_spawn_join.next = std::move(other.variant_spawn_join.next);
        break;
      }
      case tag_spawn2_join: {
        new (&variant_spawn2_join.code1) procedure_call_code_type;
        variant_spawn2_join.code1 = std::move(other.variant_spawn2_join.code1);
        variant_spawn2_join.next = std::move(other.variant_spawn2_join.next);
        break;
      }
      case tag_tail: {
        new (&variant_tail.code) procedure_call_code_type;
        variant_tail.code = std::move(other.variant_tail.code);
        break;
      }
      case tag_join_plus: {
        new (&variant_join_plus.code) procedure_call_code_type;
        variant_join_plus.code = std::move(other.variant_join_plus.code);
        new (&variant_join_plus.getter) incounter_getter_code_type;
        variant_join_plus.getter = std::move(other.variant_join_plus.getter);
        variant_join_plus.next = std::move(other.variant_join_plus.next);
        break;
      }
      case tag_spawn_minus: {
        new (&variant_spawn_minus.code) procedure_call_code_type;
        variant_spawn_minus.code = std::move(other.variant_spawn_minus.code);
        new (&variant_spawn_minus.getter) outset_getter_code_type;
        variant_spawn_minus.getter = std::move(other.variant_spawn_minus.getter);
        variant_spawn_minus.next = std::move(other.variant_spawn_minus.next);
        break;
      }
      case tag_spawn_plus: {
        new (&variant_spawn_plus.code) procedure_call_code_type;
        variant_spawn_plus.code = std::move(other.variant_spawn_plus.code);
        new (&variant_spawn_plus.getter) outset_getter_code_type;
        variant_spawn_plus.getter = std::move(other.variant_spawn_plus.getter);
        variant_spawn_plus.next = std::move(other.variant_spawn_plus.next);
        break;
      }
      case tag_join_minus: {
        new (&variant_join_minus.getter) outset_getter_code_type;
        variant_join_minus.getter = std::move(other.variant_join_minus.getter);
        variant_join_minus.next = std::move(other.variant_join_minus.next);
        break;
      }
      default:
        break;
    }
  }
  
public:
  
  basic_block_type(basic_block_type&& other) {
    move_constructor(std::move(other));
  }
  
  ~basic_block_type() {
    switch (tag) {
      case tag_unconditional_jump: {
        variant_unconditional_jump.code.~unconditional_jump_code_type();
        break;
      }
      case tag_conditional_jump: {
        variant_conditional_jump.code.~conditional_jump_code_type();
        using blks = std::vector<basic_block_label_type>;
        variant_conditional_jump.targets.~blks();
        break;
      }
      case tag_spawn_join: {
        variant_spawn_join.code.~procedure_call_code_type();
        break;
      }
      case tag_spawn2_join: {
        variant_spawn2_join.code1.~procedure_call_code_type();
        break;
      }
      case tag_tail: {
        variant_tail.code.~procedure_call_code_type();
        break;
      }
      case tag_join_plus: {
        variant_join_plus.getter.~incounter_getter_code_type();
        variant_join_plus.code.~procedure_call_code_type();
        break;
      }
      case tag_spawn_minus: {
        variant_spawn_minus.getter.~incounter_getter_code_type();
        variant_spawn_minus.code.~procedure_call_code_type();
        break;
      }
      case tag_spawn_plus: {
        variant_spawn_plus.getter.~outset_getter_code_type();
        variant_spawn_plus.code.~procedure_call_code_type();
        break;
      }
      case tag_join_minus: {
        variant_join_minus.getter.~outset_getter_code_type();
        break;
      }
      default:
        break;
    }
  }
  
  basic_block_type& operator=(basic_block_type const& other) {
    copy_constructor(other);
    return *this;
  }
  
  basic_block_type& operator=(basic_block_type&& other) {
    move_constructor(std::move(other));
    return *this;
  }
  
  static
  basic_block_type unconditional_jump(unconditional_jump_code_type code,
                                      basic_block_label_type next) {
    basic_block_type b;
    b.tag = tag_unconditional_jump;
    new (&b.variant_unconditional_jump.code) unconditional_jump_code_type(code);
    b.variant_unconditional_jump.next = next;
    return b;
  }
  
  static
  basic_block_type conditional_jump(conditional_jump_code_type code,
                                    std::vector<basic_block_label_type> targets) {
    basic_block_type b;
    b.tag = tag_conditional_jump;
    new (&b.variant_conditional_jump.code) conditional_jump_code_type(code);
    new (&b.variant_conditional_jump.targets) std::vector<basic_block_label_type>();
    b.variant_conditional_jump.targets = targets;
    return b;
  }
  
  static
  basic_block_type spawn_join(procedure_call_code_type code,
                              basic_block_label_type next) {
    basic_block_type b;
    b.tag = tag_spawn_join;
    new (&b.variant_spawn_join.code) procedure_call_code_type(code);
    b.variant_spawn_join.next = next;
    return b;
  }
  
  static
  basic_block_type spawn2_join(procedure_call_code_type code1,
                               basic_block_label_type next) {
    basic_block_type b;
    b.tag = tag_spawn2_join;
    new (&b.variant_spawn2_join.code1) procedure_call_code_type(code1);
    b.variant_spawn2_join.next = next;
    return b;
  }
  
  static
  basic_block_type tail(procedure_call_code_type code,
                        basic_block_label_type next) {
    basic_block_type b;
    b.t = tag_tail;
    new (&b.variant_tail.code) procedure_call_code_type(code);
    b.variant_tail.next = next;
    return b;
  }
  
  static
  basic_block_type join_plus(procedure_call_code_type code,
                             incounter_getter_code_type getter,
                             basic_block_label_type next) {
    basic_block_type b;
    b.tag = tag_join_plus;
    new (&b.variant_join_plus.code) procedure_call_code_type(code);
    new (&b.variant_join_plus.getter) incounter_getter_code_type(getter);
    b.variant_join_plus.next = next;
    return b;
  }
  
  static
  basic_block_type spawn_minus(procedure_call_code_type code,
                               incounter_getter_code_type getter,
                               basic_block_label_type next) {
    basic_block_type b;
    b.tag = tag_spawn_minus;
    new (&b.variant_spawn_minus.code) procedure_call_code_type(code);
    new (&b.variant_spawn_minus.getter) incounter_getter_code_type(getter);
    b.variant_spawn_minus.next = next;
    return b;
  }
  
  static
  basic_block_type spawn_plus(procedure_call_code_type code,
                              outset_getter_code_type getter,
                              basic_block_label_type next) {
    basic_block_type b;
    b.tag = tag_spawn_plus;
    new (&b.variant_spawn_plus.code) procedure_call_code_type(code);
    new (&b.variant_spawn_plus.getter) outset_getter_code_type(getter);
    b.variant_spawn_plus.next = next;
    return b;
  }
  
  static
  basic_block_type join_minus(outset_getter_code_type getter,
                              basic_block_label_type next) {
    basic_block_type b;
    b.tag = tag_join_minus;
    new (&b.variant_join_minus.getter) outset_getter_code_type(getter);
    b.variant_join_minus.next = next;
    return b;
  }
  
};
  
using trampoline_type = struct {
  basic_block_label_type pred;
  basic_block_label_type succ;
};
  
class parallel_for_descriptor {
public:
  
  parallel_for_descriptor()
  : lo(nullptr), hi(nullptr), join(nullptr) { }
  
  parallel_for_descriptor(int& lo, int& hi)
  : lo(&lo), hi(&hi), join(nullptr) { }
  
  int* lo;
  
  int* hi;
  
  sched::vertex* join = nullptr;
  
};

using parallel_loop_id_type = int;

template <class Private_activation_record>
class parallel_loop_descriptor_type {
public:
  
  using par = Private_activation_record;
  
  trampoline_type entry;
  
  trampoline_type exit;
  
  // parents is ordered from oldest to youngest
  std::vector<parallel_loop_id_type> parents;
  
  std::function<void(par&, parallel_for_descriptor&)> initializer;
  
};
  
template <class Shared_activation_record>
using private_activation_record_of = typename Shared_activation_record::private_activation_record;
  
template <class Shared_activation_record>
class cfg_type {
public:
  
  using sar = Shared_activation_record;
  using par = private_activation_record_of<sar>;
  
  using basic_block_type = basic_block_type<sar, par>;
  
  cfg_type() { }
  
  cfg_type(size_t nb_blocks)
  : basic_blocks(nb_blocks) { }
  
  cfg_type(size_t nb_blocks, size_t nb_loops)
  : basic_blocks(nb_blocks), loop_descriptors(nb_loops), loop_of(nb_blocks) { }
  
  std::vector<basic_block_type> basic_blocks;
  
  std::vector<parallel_loop_descriptor_type<par>> loop_descriptors;
  
  // to map from basic_block_id to parallel_loop_id
  // loop_of[i] yields the id of the parallel loop that is
  // closes to the basic block labeled i
  std::vector<parallel_loop_id_type> loop_of;
  
  size_t nb_basic_blocks() const {
    return basic_blocks.size();
  }
  
  size_t nb_loops() const {
    return loop_descriptors.size();
  }
  
  basic_block_type& operator[](size_t i) {
    return basic_blocks[i];
  }
  
};
  
/*---------------------------------------------------------------------*/
/* Parallel control-flow graph interpreter */
  
template <class Stack>
class interpreter;
  
class shared_activation_record {
public:
  
  virtual std::pair<stack_type, int> run(stack_type, int) const = 0;
  virtual std::pair<extended_stack_type, int> run(extended_stack_type, int) const = 0;
  
  virtual void promote(interpreter<stack_type>*) const = 0;
  virtual void promote(interpreter<extended_stack_type>*) const = 0;
  
};
  
class private_activation_record {
public:
  
  trampoline_type trampoline = { .pred = entry_block_label, .succ = entry_block_label };
  
  virtual int nb_strands() {
    return 1;
  }
  
  virtual std::pair<sched::vertex*, sched::vertex*> split(interpreter<stack_type>*, int) {
    assert(false); // impossible
    return std::make_pair(nullptr, nullptr);
  }
  
  virtual std::pair<sched::vertex*, sched::vertex*> split(interpreter<extended_stack_type>*, int) {
    assert(false); // impossible
    return std::make_pair(nullptr, nullptr);
  }
  
  virtual sched::vertex* get_join(parallel_loop_id_type) {
    assert(false); // impossible
    return nullptr;
  }
  
};
  
stack_type new_stack() {
  return std::make_pair(cactus::new_stack(), cactus::new_stack());
}

void delete_stack(stack_type s) {
  cactus::delete_stack(s.first);
  cactus::delete_stack(s.second);
}
  
bool empty_stack(stack_type s) {
  bool r = cactus::empty(s.first);
  assert(cactus::empty(s.second) == r);
  return r;
}
  
template <class Shared_activation_record, class ...Args>
stack_type push_call(stack_type s, Args... args) {
  using private_activation_record = private_activation_record_of<Shared_activation_record>;
  auto ss = cactus::push_back<Shared_activation_record>(s.first, args...);
  auto ps = cactus::push_back<private_activation_record>(s.second);
  return std::make_pair(ss, ps);
}

stack_type pop_call(stack_type s) {
  auto ss = cactus::pop_back<shared_activation_record>(s.first);
  auto ps = cactus::pop_back<private_activation_record>(s.second);
  return std::make_pair(ss, ps);
}
  
template <class Shared_activation_record>
Shared_activation_record& peek_newest_shared_frame(stack_type s) {
  return cactus::peek_back<Shared_activation_record>(s.first);
}
  
template <class Private_activation_record>
Private_activation_record& peek_newest_private_frame(stack_type s) {
  return cactus::peek_back<Private_activation_record>(s.second);
}
  
template <class Shared_activation_record>
Shared_activation_record& peek_oldest_shared_frame(stack_type s) {
  return cactus::peek_front<Shared_activation_record>(s.first);
}
 
template <class Private_activation_record>
Private_activation_record& peek_oldest_private_frame(stack_type s) {
  return cactus::peek_front<Private_activation_record>(s.second);
}
  
template <class Shared_activation_record>
std::pair<stack_type, stack_type> slice_stack(stack_type s) {
  using private_activation_record = private_activation_record_of<Shared_activation_record>;
  auto ss = cactus::slice_front<Shared_activation_record>(s.first);
  auto ps = cactus::slice_front<private_activation_record>(s.second);
  return std::make_pair(std::make_pair(ss.first, ps.first), std::make_pair(ss.second, ps.second));
}
  
template <class Shared_activation_record>
std::pair<stack_type, stack_type> fork_stack(stack_type s) {
  using private_activation_record = private_activation_record_of<Shared_activation_record>;
  auto ss = cactus::fork_front<Shared_activation_record>(s.first);
  auto ps = cactus::fork_front<private_activation_record>(s.second);
  return std::make_pair(std::make_pair(ss.first, ps.first), std::make_pair(ss.second, ps.second));
}
  
extended_stack_type new_stack(shared_activation_record* oldest_shared) {
  extended_stack_type s;
  s.oldest_shared = oldest_shared;
  s.stack = new_stack();
  return s;
}

void delete_stack(extended_stack_type s) {
  assert(empty_stack(s.stack));
  delete_stack(s.stack);
}
  
bool empty_stack(extended_stack_type s) {
  if (cactus::empty(s.stack.second)) {
    return true;
  }
  return cactus::peek_front<private_activation_record>(s.stack.second).nb_strands() == 0;
}
  
template <class Shared_activation_record, class ...Args>
extended_stack_type push_call(extended_stack_type s, Args... args) {
  s.stack = push_call<Shared_activation_record>(s.stack, args...);
  return s;
}

extended_stack_type pop_call(extended_stack_type s) {
  if (cactus::empty(s.stack.first)) {
    s.oldest_shared = nullptr;
  } else {
    s.stack.first = cactus::pop_back<shared_activation_record>(s.stack.first);
  }
  s.stack.second = cactus::pop_back<private_activation_record>(s.stack.second);
  return s;
}
 
template <class Shared_activation_record>
Shared_activation_record& peek_newest_shared_frame(extended_stack_type s) {
  if (cactus::empty(s.stack.first)) {
    return *((Shared_activation_record*)s.oldest_shared);
  } else {
    return cactus::peek_back<Shared_activation_record>(s.stack.first);
  }
}
  
template <class Private_activation_record>
Private_activation_record& peek_newest_private_frame(extended_stack_type s) {
  return cactus::peek_back<Private_activation_record>(s.stack.second);
}
  
template <class Shared_activation_record>
Shared_activation_record& peek_oldest_shared_frame(extended_stack_type s) {
  if (cactus::empty(s.stack.first)) {
    return *((Shared_activation_record*)s.oldest_shared);
  } else {
    return cactus::peek_front<Shared_activation_record>(s.stack.first);
  }
}
  
template <class Private_activation_record>
Private_activation_record& peek_oldest_private_frame(extended_stack_type s) {
  return cactus::peek_front<Private_activation_record>(s.stack.second);
}
  
template <class Shared_activation_record>
std::pair<extended_stack_type, stack_type> slice_stack(extended_stack_type s) {
  using private_activation_record = private_activation_record_of<Shared_activation_record>;
  extended_stack_type s1;
  stack_type s2;
  s1.oldest_shared = s.oldest_shared;
  s1.stack.first = cactus::new_stack();
  auto private_stacks = cactus::slice_front<private_activation_record>(s.stack.second);
  s1.stack.second = private_stacks.first;
  s2.first = s.stack.first;
  s2.second = private_stacks.second;
  return std::make_pair(s1, s2);
}
  
template <class Shared_activation_record>
std::pair<extended_stack_type, stack_type> fork_stack(extended_stack_type s) {
  using private_activation_record = private_activation_record_of<Shared_activation_record>;
  extended_stack_type s1;
  stack_type s2;
  s1.oldest_shared = s.oldest_shared;
  s1.stack.first = cactus::new_stack();
  s2.first = s.stack.first;
  auto private_stacks = cactus::fork_front<private_activation_record>(s.stack.second);
  s1.stack.second = private_stacks.first;
  s2.second = private_stacks.second;
  return std::make_pair(s1, s2);
}
 
template <class Stack>
class interpreter : public sched::vertex {
public:
  
  Stack stack;
  
  interpreter() {
    stack = new_stack();
  }
  
  interpreter(shared_activation_record* s) {
    stack = new_stack(s);
  }
  
  interpreter(Stack stack)
  : stack(stack) { }
  
  ~interpreter() {
    delete_stack(stack);
  }
  
  int nb_strands() {
    if (empty_stack(stack)) {
      return 0;
    } else {
      return peek_oldest_private_frame<private_activation_record>(stack).nb_strands();
    }
  }
  
  int run(int fuel) {
    Stack s = stack;
    while (! empty_stack(s) && fuel >= 1) {
      auto result = peek_newest_shared_frame<shared_activation_record>(s).run(s, fuel);
      s = result.first;
      fuel = result.second;
    }
    stack = s;
    if (fuel == suspend_tag) {
      suspend(this);
      fuel = 0;
    } else if (! empty_stack(stack)) {
      assert(fuel == 0);
      peek_oldest_shared_frame<shared_activation_record>(stack).promote(this);
    }
    return fuel;
  }
  
  std::pair<vertex*, vertex*> split(int nb) {
    return peek_oldest_private_frame<private_activation_record>(stack).split(this, nb);
  }
  
};
  
template <class Function, class Shared_activation_record, class Private_activation_record>
stack_type extend_stack_with(const Function& f, Shared_activation_record& sa,
                             Private_activation_record& pa, stack_type s) {
  return f(sa, pa, s);
}

template <class Function, class Shared_activation_record, class Private_activation_record>
extended_stack_type extend_stack_with(const Function& f, Shared_activation_record& sa,
                                      Private_activation_record& pa, extended_stack_type s) {
  s.stack = f(sa, pa, s.stack);
  return s;
}

template <class Shared_activation_record, class Stack>
std::pair<Stack, int> step(cfg_type<Shared_activation_record>& cfg, Stack stack, int fuel) {
  using private_activation_record = private_activation_record_of<Shared_activation_record>;
  assert(! empty_stack(stack));
  fuel--;
  auto& shared_newest = peek_newest_shared_frame<Shared_activation_record>(stack);
  auto& private_newest = peek_newest_private_frame<private_activation_record>(stack);
  basic_block_label_type pred = private_newest.trampoline.succ;
  basic_block_label_type succ;
  if (pred == exit_block_label) {
    stack = pop_call(stack);
    return std::make_pair(stack, fuel);
  }
  assert(pred >= 0 && pred < cfg.nb_basic_blocks());
  auto& block = cfg.basic_blocks[pred];
  switch (block.tag) {
    case tag_unconditional_jump: {
      block.variant_unconditional_jump.code(shared_newest, private_newest);
      succ = block.variant_unconditional_jump.next;
      break;
    }
    case tag_conditional_jump: {
      int target = block.variant_conditional_jump.code(shared_newest, private_newest);
      succ = block.variant_conditional_jump.targets[target];
      break;
    }
    case tag_spawn_join: {
      stack = extend_stack_with(block.variant_spawn_join.code, shared_newest, private_newest, stack);
      succ = block.variant_spawn_join.next;
      break;
    }
    case tag_spawn2_join: {
      stack = extend_stack_with(block.variant_spawn2_join.code1, shared_newest, private_newest, stack);
      succ = block.variant_spawn2_join.next;
      break;
    }
    case tag_tail: {
      stack = pop_call(stack);
      stack = extend_stack_with(block.variant_tail.code, shared_newest, private_newest, stack);
      break;
    }
    case tag_join_plus: {
      *block.variant_join_plus.getter(shared_newest, private_newest) = nullptr;
      stack = extend_stack_with(block.variant_join_plus.code, shared_newest, private_newest, stack);
      succ = block.variant_join_plus.next;
      break;
    }
    case tag_spawn_minus: {
      stack = extend_stack_with(block.variant_spawn_minus.code, shared_newest, private_newest, stack);
      succ = block.variant_spawn_minus.next;
      break;
    }
    case tag_spawn_plus: {
      *block.variant_spawn_plus.getter(shared_newest, private_newest) = nullptr;
      stack = extend_stack_with(block.variant_spawn_plus.code, shared_newest, private_newest, stack);
      succ = block.variant_spawn_plus.next;
      break;
    }
    case tag_join_minus: {
      sched::outset* outset = *block.variant_join_minus.getter(shared_newest, private_newest);
      if (outset != nullptr) {
        fuel = suspend_tag;
      }
      succ = block.variant_join_minus.next;
      break;
    }
    default: {
      assert(false);
    }
  }
  private_newest.trampoline.pred = pred;
  private_newest.trampoline.succ = succ;
  return std::make_pair(stack, fuel);
}

template <class Shared_activation_record, class Stack>
void promote(cfg_type<Shared_activation_record>& cfg, interpreter<Stack>* interp) {
  using private_activation_record = private_activation_record_of<Shared_activation_record>;
  auto nb_strands = interp->nb_strands();
  if (nb_strands >= 2) {
    auto vertices = interp->split(nb_strands / 2);
    schedule(vertices.second);
    schedule(vertices.first);
    stats::on_promotion();
    return;
  }
  Stack& stack = interp->stack;
  assert(! empty_stack(stack));
  auto& shared_oldest = peek_oldest_shared_frame<Shared_activation_record>(stack);
  auto& private_oldest = peek_oldest_private_frame<private_activation_record>(stack);
  basic_block_label_type pred = private_oldest.trampoline.pred;
  if (pred == exit_block_label) {
    return;
  }
  assert(pred >= 0 && pred < cfg.nb_basic_blocks());
  auto& block = cfg.basic_blocks[pred];
  switch (block.tag) {
    case tag_unconditional_jump: {
      schedule(interp);
      break;
    }
    case tag_conditional_jump: {
      schedule(interp);
      break;
    }
    case tag_spawn_join: {
      interpreter<Stack>* join = interp;
      auto stacks = slice_stack<Shared_activation_record>(interp->stack);
      join->stack = stacks.first;
      interpreter<stack_type>* branch = new interpreter<stack_type>(stacks.second);
      new_edge(branch, join);
      release(branch);
      stats::on_promotion();
      break;
    }
    case tag_spawn2_join: {
      interpreter<Stack>* join = interp;
      auto stacks = slice_stack<Shared_activation_record>(interp->stack);
      join->stack = stacks.first;
      interpreter<stack_type>* branch1 = new interpreter<stack_type>(stacks.second);
      interpreter<stack_type>* branch2 = new interpreter<stack_type>;
      basic_block_label_type pred = private_oldest.trampoline.succ;
      auto& spawn_join_block = cfg.basic_blocks[pred];
      assert(spawn_join_block.tag == tag_spawn_join);
      private_oldest.trampoline.pred = pred;
      private_oldest.trampoline.succ = spawn_join_block.variant_spawn_join.next;
      branch2->stack = spawn_join_block.variant_spawn_join.code(shared_oldest, private_oldest, branch2->stack);
      new_edge(branch2, join);
      new_edge(branch1, join);
      release(branch2);
      release(branch1);
      stats::on_promotion();
      break;
    }
    case tag_tail: {
      schedule(interp);
      break;
    }
    case tag_join_plus: {
      interpreter<Stack>* join = interp;
      auto stacks = fork_stack<Shared_activation_record>(interp->stack);
      join->stack = stacks.first;
      interpreter<stack_type>* branch = new interpreter<stack_type>(stacks.second);
      assert(*block.variant_join_plus.getter(shared_oldest, private_oldest) == nullptr);
      *block.variant_join_plus.getter(shared_oldest, private_oldest) = join->get_incounter();
      new_edge(branch, join);
      release(branch);
      stats::on_promotion();
      break;
    }
    case tag_spawn_minus: {
      interpreter<Stack>* continuation = interp;
      auto stacks = slice_stack<Shared_activation_record>(interp->stack);
      continuation->stack = stacks.first;
      interpreter<stack_type>* branch = new interpreter<stack_type>(stacks.second);
      sched::incounter* incounter = *block.variant_spawn_minus.getter(shared_oldest, private_oldest);
      assert(incounter != nullptr);
      new_edge(branch, incounter);
      schedule(continuation);
      release(branch);
      stats::on_promotion();
      break;
    }
    case tag_spawn_plus: {
      interpreter<Stack>* continuation = interp;
      auto stacks = fork_stack<Shared_activation_record>(interp->stack);
      continuation->stack = stacks.first;
      interpreter<stack_type>* branch = new interpreter<stack_type>(stacks.second);
      assert(*block.variant_spawn_plus.getter(shared_oldest, private_oldest) == nullptr);
      *block.variant_spawn_plus.getter(shared_oldest, private_oldest) = branch->get_outset();
      schedule(continuation);
      release(branch);
      stats::on_promotion();
      break;
    }
    case tag_join_minus: {
      interpreter<Stack>* join = interp;
      sched::outset* outset = *block.variant_join_minus.getter(shared_oldest, private_oldest);
      assert(outset != nullptr);
      new_edge(outset, join);
      stats::on_promotion();
      break;
    }
    default: {
      assert(false);
    }
  }
}
  
static constexpr parallel_loop_id_type not_a_parallel_loop_id = -1;
 
template <class Sar, class Par, int nb_loops>
class parallel_for_private_activation_record : public private_activation_record {
public:
  
  using sar = Sar;
  using par = Par;
  
  std::array<parallel_for_descriptor, nb_loops> parallel_for_descriptors;
  
  void initialize_descriptors() {
    assert(nb_loops == sar::cfg.nb_loops());
    for (parallel_loop_id_type id = 0; id < nb_loops; id++) {
      auto& d = sar::cfg.loop_descriptors[id];
      d.initializer(*(par*)this, parallel_for_descriptors[id]);
    }
  }
  
  parallel_for_private_activation_record() {
    initialize_descriptors();
  }
  
  parallel_loop_id_type get_id_of_current_parallel_loop() const {
    return sar::cfg.loop_of[trampoline.pred];
  }
  
  parallel_loop_id_type get_id_of_oldest_nonempty() const {
    parallel_loop_id_type current = get_id_of_current_parallel_loop();
    if (current == not_a_parallel_loop_id) {
      return not_a_parallel_loop_id;
    }
    for (parallel_loop_id_type id : sar::cfg.loop_descriptors[current].parents) {
      assert(id != not_a_parallel_loop_id);
      auto& d = parallel_for_descriptors[id];
      if (*d.hi - *d.lo >= 2) {
        return id;
      }
    }
    auto& d = parallel_for_descriptors[current];
    if (*d.hi - *d.lo >= 2) {
      return current;
    } else {
      return not_a_parallel_loop_id;
    }
  }
  
  parallel_for_descriptor* get_oldest_nonempty() {
    auto id = get_id_of_oldest_nonempty();
    if (id == not_a_parallel_loop_id) {
      return nullptr;
    } else {
      return &parallel_for_descriptors[id];
    }
  }
  
  sched::vertex* get_join(parallel_loop_id_type id) {
    return parallel_for_descriptors[id].join;
  }
  
  sched::vertex* get_join() {
    return get_join(get_id_of_current_parallel_loop());
  }
  
  int nb_strands() {
    if (trampoline.pred == exit_block_label) {
      return 0;
    }
    auto d = get_oldest_nonempty();
    if (d == nullptr) {
      return 1;
    }
    return std::max(1, *d->hi - *d->lo);
  }
  
  template <class Stack>
  std::pair<sched::vertex*, sched::vertex*> split2(interpreter<Stack>* interp, int nb) {
    interpreter<extended_stack_type>* interp1 = nullptr;
    interpreter<extended_stack_type>* interp2 = nullptr;
    sar* oldest_shared = &peek_oldest_shared_frame<sar>(interp->stack);
    par* oldest_private = &peek_oldest_private_frame<par>(interp->stack);
    parallel_loop_id_type id = oldest_private->get_id_of_oldest_nonempty();
    parallel_for_descriptor& pf_descr = oldest_private->parallel_for_descriptors[id];
    parallel_loop_descriptor_type<par>& pl_descr = sar::cfg.loop_descriptors[id];
    sched::vertex* join = pf_descr.join;
    int lo = *pf_descr.lo;
    int hi = *pf_descr.hi;
    int mid = *pf_descr.lo + nb;
    if (join == nullptr) {
      join = interp;
      auto stacks = slice_stack<sar>(interp->stack);
      interp->stack = stacks.first;
      interp1 = new interpreter<extended_stack_type>(oldest_shared);
      extended_stack_type& stack1 = interp1->stack;
      stack1.stack.second = cactus::push_back<par>(stack1.stack.second, *oldest_private);
      par& private1 = peek_oldest_private_frame<par>(stack1);
      private1.initialize_descriptors();
      auto& pf1_descr = private1.parallel_for_descriptors[id];
      *pf1_descr.lo = lo;
      *pf1_descr.hi = mid;
      pf1_descr.join = join;
      private1.trampoline = pl_descr.entry;
      oldest_private->trampoline = pl_descr.exit;
      pf_descr.join = nullptr;
      *pf_descr.lo = -1;
      *pf_descr.hi = -1;
      sched::new_edge(interp1, join);
      interp1->release_handle->decrement();
    } else {
      *pf_descr.hi = mid;
      interp1 = (interpreter<extended_stack_type>*)interp;
    }
    interp2 = new interpreter<extended_stack_type>(oldest_shared);
    extended_stack_type& stack2 = interp2->stack;
    stack2.stack.second = cactus::push_back<par>(stack2.stack.second, *oldest_private);
    par& private2 = peek_oldest_private_frame<par>(stack2);
    private2.initialize_descriptors();
    auto& pf2_descr = private2.parallel_for_descriptors[id];
    *pf2_descr.lo = mid;
    *pf2_descr.hi = hi;
    pf2_descr.join = join;
    private2.trampoline = pl_descr.entry;
    sched::new_edge(interp2, join);
    interp2->release_handle->decrement();
    return std::make_pair(interp1, interp2);
  }
  
  std::pair<sched::vertex*, sched::vertex*> split(interpreter<stack_type>* interp, int nb) {
    return split2(interp, nb);
  }
  
  std::pair<sched::vertex*, sched::vertex*> split(interpreter<extended_stack_type>* interp, int nb) {
    return split2(interp, nb);
  }
  
};
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Dag Calculus */

namespace dc {
  
using stmt_tag_type = enum {
  tag_stmt, tag_stmts, tag_cond, tag_exit,
  tag_sequential_loop, tag_parallel_for_loop,
  tag_spawn_join, tag_spawn2_join,
  tag_join_plus, tag_spawn_minus,
  tag_spawn_plus, tag_join_minus,
  tag_none
};
  
template <class Shared_activation_record, class Private_activation_record>
class stmt_type {
public:

  using sar_type = Shared_activation_record;
  using par_type = Private_activation_record;
  
  using unconditional_jump_code_type = std::function<void(sar_type&, par_type&)>;
  using predicate_code_type = std::function<bool(sar_type&, par_type&)>;
  using procedure_call_code_type = std::function<pcfg::stack_type(sar_type&, par_type&, pcfg::stack_type)>;
  using incounter_getter_code_type = std::function<sched::incounter**(sar_type&, par_type&)>;
  using outset_getter_code_type = std::function<sched::outset**(sar_type&, par_type&)>;
  using conds_type = std::vector<std::pair<predicate_code_type, stmt_type>>;
  using parallel_for_getter_type = std::function<std::pair<int*, int*>(par_type&)>;
  
  stmt_tag_type tag;
  
  union {
    struct {
      unconditional_jump_code_type code;
    } variant_stmt;
    struct {
      std::vector<stmt_type> stmts;
    } variant_stmts;
    struct {
      conds_type conds;
      std::unique_ptr<stmt_type> otherwise;
    } variant_cond;
    struct {
      // nothing to put here
    } variant_exit;
    struct {
      predicate_code_type predicate;
      std::unique_ptr<stmt_type> body;
    } variant_sequential_loop;
    struct {
      predicate_code_type predicate;
      parallel_for_getter_type getter;
      std::unique_ptr<stmt_type> body;
    } variant_parallel_for_loop;
    struct {
      procedure_call_code_type code;
    } variant_spawn_join;
    struct {
      procedure_call_code_type code1;
      procedure_call_code_type code2;
    } variant_spawn2_join;
    struct {
      procedure_call_code_type code;
      incounter_getter_code_type getter;
    } variant_join_plus;
    struct {
      procedure_call_code_type code;
      incounter_getter_code_type getter;
    } variant_spawn_minus;
    struct {
      procedure_call_code_type code;
      outset_getter_code_type getter;
    } variant_spawn_plus;
    struct {
      outset_getter_code_type getter;
    } variant_join_minus;
  };
  
  stmt_type() : tag(tag_none) { }
  
private:
  
  void copy_constructor(stmt_type const& other) {
    tag = other.tag;
    switch (tag) {
      case tag_stmt: {
        new (&variant_stmt.code) unconditional_jump_code_type(other.variant_stmt.code);
        break;
      }
      case tag_stmts: {
        new (&variant_stmts.stmts) std::vector<stmt_type>(other.variant_stmts.stmts);
        break;
      }
      case tag_cond: {
        new (&variant_cond.conds) conds_type(other.variant_cond.conds);
        auto p = new stmt_type;
        p->copy_constructor(*other.variant_cond.otherwise);
        new (&variant_cond.otherwise) std::unique_ptr<stmt_type>(p);
        break;
      }
      case tag_exit: {
        // nothing to put here
        break;
      }
      case tag_sequential_loop: {
        new (&variant_sequential_loop.predicate) predicate_code_type(other.variant_sequential_loop.predicate);
        auto p = new stmt_type;
        p->copy_constructor(*other.variant_sequential_loop.body);
        new (&variant_sequential_loop.body) std::unique_ptr<stmt_type>(p);
        break;
      }
      case tag_parallel_for_loop: {
        new (&variant_parallel_for_loop.predicate) predicate_code_type(other.variant_parallel_for_loop.predicate);
        new (&variant_parallel_for_loop.getter) parallel_for_getter_type(other.variant_parallel_for_loop.getter);
        auto p = new stmt_type;
        p->copy_constructor(*other.variant_parallel_for_loop.body);
        new (&variant_parallel_for_loop.body) std::unique_ptr<stmt_type>(p);
        break;
      }
      case tag_spawn_join: {
        new (&variant_spawn_join.code) procedure_call_code_type(other.variant_spawn_join.code);
        break;
      }
      case tag_spawn2_join: {
        new (&variant_spawn2_join.code1) procedure_call_code_type(other.variant_spawn2_join.code1);
        new (&variant_spawn2_join.code2) procedure_call_code_type(other.variant_spawn2_join.code2);
        break;
      }
      case tag_join_plus: {
        new (&variant_join_plus.code) procedure_call_code_type(other.variant_join_plus.code);
        new (&variant_join_plus.getter) incounter_getter_code_type(other.variant_join_plus.getter);
        break;
      }
      case tag_spawn_minus: {
        new (&variant_spawn_minus.code) procedure_call_code_type(other.variant_spawn_minus.code);
        new (&variant_spawn_minus.getter) incounter_getter_code_type(other.variant_spawn_minus.getter);
        break;
      }
      case tag_spawn_plus: {
        new (&variant_spawn_plus.code) procedure_call_code_type(other.variant_spawn_plus.code);
        new (&variant_spawn_plus.getter) outset_getter_code_type(other.variant_spawn_plus.getter);
        break;
      }
      case tag_join_minus: {
        new (&variant_join_minus.getter) outset_getter_code_type(other.variant_join_minus.getter);
        break;
      }
      default: {
        break;
      }
    }
  }
  
public:
  
  stmt_type(stmt_type const& other) {
    copy_constructor(other);
  }
  
private:
  
  void move_constructor(stmt_type&& other) {
    tag = other.tag;
    other.tag = tag_none;
    switch (tag) {
      case tag_stmt: {
        new (&variant_stmt.code) unconditional_jump_code_type;
        variant_stmt.code = std::move(other.variant_stmt.code);
        break;
      }
      case tag_stmts: {
        new (&variant_stmts.stmts) std::vector<stmt_type>;
        variant_stmts.stmts = std::move(other.variant_stmts.stmts);
        break;
      }
      case tag_cond: {
        new (&variant_cond.conds) conds_type;
        variant_cond.conds = std::move(other.variant_cond.conds);
        new (&variant_cond.otherwise) std::unique_ptr<stmt_type>;
        variant_cond.otherwise = std::move(other.variant_cond.otherwise);
        break;
      }
      case tag_exit: {
        // nothing to put here
        break;
      }
      case tag_sequential_loop: {
        new (&variant_sequential_loop.predicate) predicate_code_type;
        variant_sequential_loop.predicate = std::move(other.variant_sequential_loop.predicate);
        new (&variant_sequential_loop.body) std::unique_ptr<stmt_type>;
        variant_sequential_loop.body = std::move(other.variant_sequential_loop.body);
        break;
      }
      case tag_parallel_for_loop: {
        new (&variant_parallel_for_loop.predicate) predicate_code_type;
        new (&variant_parallel_for_loop.getter) parallel_for_getter_type;
        variant_parallel_for_loop.getter = std::move(other.variant_parallel_for_loop.getter);
        new (&variant_parallel_for_loop.body) std::unique_ptr<stmt_type>;
        variant_parallel_for_loop.body = std::move(other.variant_parallel_for_loop.body);
        break;
      }
      case tag_spawn_join: {
        new (&variant_spawn_join.code) procedure_call_code_type;
        variant_spawn_join.code = std::move(other.variant_spawn_join.code);
        break;
      }
      case tag_spawn2_join: {
        new (&variant_spawn2_join.code1) procedure_call_code_type;
        variant_spawn2_join.code1 = std::move(other.variant_spawn2_join.code1);
        new (&variant_spawn2_join.code2) procedure_call_code_type;
        variant_spawn2_join.code2 = std::move(other.variant_spawn2_join.code2);
        break;
      }
      case tag_join_plus: {
        new (&variant_join_plus.code) procedure_call_code_type;
        variant_join_plus.code = std::move(other.variant_join_plus.code);
        new (&variant_join_plus.getter) incounter_getter_code_type;
        variant_join_plus.getter = std::move(other.variant_join_plus.getter);
        break;
      }
      case tag_spawn_minus: {
        new (&variant_spawn_minus.code) procedure_call_code_type;
        variant_spawn_minus.code = std::move(other.variant_spawn_minus.code);
        new (&variant_spawn_minus.getter) incounter_getter_code_type;
        variant_spawn_minus.getter = std::move(other.variant_spawn_minus.getter);
        break;
      }
      case tag_spawn_plus: {
        new (&variant_spawn_plus.code) procedure_call_code_type;
        variant_spawn_plus.code = std::move(other.variant_spawn_plus.code);
        new (&variant_spawn_plus.getter) outset_getter_code_type;
        variant_spawn_plus.getter = std::move(other.variant_spawn_plus.getter);
        break;
      }
      case tag_join_minus: {
        new (&variant_join_minus.getter) outset_getter_code_type;
        variant_join_minus.getter = std::move(other.variant_join_minus.getter);
        break;
      }
      default: {
        break;
      }
    }
  }
  
public:
  
  stmt_type(stmt_type&& other) {
    move_constructor(std::move(other));
  }
  
  ~stmt_type() {
    switch (tag) {
      case tag_stmt: {
        variant_stmt.code.~unconditional_jump_code_type();
        break;
      }
      case tag_stmts: {
        using st = std::vector<stmt_type>;
        variant_stmts.stmts.~st();
        break;
      }
      case tag_cond: {
        variant_cond.conds.~conds_type();
        using st = std::unique_ptr<stmt_type>;
        variant_cond.otherwise.~st();
        break;
      }
      case tag_exit: {
        // nothing to put here
        break;
      }
      case tag_sequential_loop: {
        variant_sequential_loop.predicate.~predicate_code_type();
        using st = std::unique_ptr<stmt_type>;
        variant_sequential_loop.body.~st();
        break;
      }
      case tag_parallel_for_loop: {
        variant_parallel_for_loop.predicate.~predicate_code_type();
        variant_parallel_for_loop.getter.~parallel_for_getter_type();
        using st = std::unique_ptr<stmt_type>;
        variant_parallel_for_loop.body.~st();
        break;
      }
      case tag_spawn_join: {
        variant_spawn_join.code.~procedure_call_code_type();
        break;
      }
      case tag_spawn2_join: {
        variant_spawn2_join.code1.~procedure_call_code_type();
        variant_spawn2_join.code2.~procedure_call_code_type();
        break;
      }
      case tag_join_plus: {
        variant_join_plus.code.~procedure_call_code_type();
        variant_join_plus.getter.~incounter_getter_code_type();
        break;
      }
      case tag_spawn_minus: {
        variant_spawn_minus.code.~procedure_call_code_type();
        variant_spawn_minus.getter.~incounter_getter_code_type();
        break;
      }
      case tag_spawn_plus: {
        variant_spawn_plus.code.~procedure_call_code_type();
        variant_spawn_plus.getter.~outset_getter_code_type();
        break;
      }
      case tag_join_minus: {
        variant_join_minus.getter.~outset_getter_code_type();
        break;
      }
      default: {
        break;
      }
    }
  }
  
  stmt_type& operator=(stmt_type const& other) {
    copy_constructor(other);
    return *this;
  }
  
  stmt_type& operator=(stmt_type&& other) {
    move_constructor(std::move(other));
    return *this;
  }
  
  static
  stmt_type stmt(unconditional_jump_code_type code) {
    stmt_type s;
    s.tag = tag_stmt;
    new (&s.variant_stmt.code) unconditional_jump_code_type(code);
    return s;
  }
  
  static
  stmt_type stmts(std::vector<stmt_type> stmts) {
    stmt_type s;
    s.tag = tag_stmts;
    new (&s.variant_stmts.stmts) std::vector<stmt_type>(stmts);
    return s;
  }
  
  static
  stmt_type cond(conds_type conds, stmt_type otherwise) {
    stmt_type s;
    s.tag = tag_cond;
    new (&s.variant_cond.conds) conds_type(conds);
    new (&s.variant_cond.otherwise) std::unique_ptr<stmt_type>(new stmt_type(otherwise));
    return s;
  }
  
  static
  stmt_type exit() {
    stmt_type s;
    s.tag = tag_exit;
    return s;
  }
  
  static
  stmt_type sequential_loop(predicate_code_type predicate, stmt_type body) {
    stmt_type s;
    s.tag = tag_sequential_loop;
    new (&s.variant_sequential_loop.predicate) predicate_code_type(predicate);
    new (&s.variant_sequential_loop.body) std::unique_ptr<stmt_type>(new stmt_type(body));
    return s;
  }
  
  static
  stmt_type parallel_for_loop(predicate_code_type predicate, parallel_for_getter_type getter, stmt_type body) {
    stmt_type s;
    s.tag = tag_parallel_for_loop;
    new (&s.variant_parallel_for_loop.predicate) predicate_code_type(predicate);
    new (&s.variant_parallel_for_loop.getter) parallel_for_getter_type(getter);
    new (&s.variant_parallel_for_loop.body) std::unique_ptr<stmt_type>(new stmt_type(body));
    return s;
  }
  
  static
  stmt_type spawn_join(procedure_call_code_type code) {
    stmt_type s;
    s.tag = tag_spawn_join;
    new (&s.variant_spawn_join) procedure_call_code_type(code);
    return s;
  }
  
  static
  stmt_type spawn2_join(procedure_call_code_type code1, procedure_call_code_type code2) {
    stmt_type s;
    s.tag = tag_spawn2_join;
    new (&s.variant_spawn2_join.code1) procedure_call_code_type(code1);
    new (&s.variant_spawn2_join.code2) procedure_call_code_type(code2);
    return s;
  }
  
  static
  stmt_type join_plus(procedure_call_code_type code, incounter_getter_code_type getter) {
    stmt_type s;
    s.tag = tag_join_plus;
    new (&s.variant_join_plus.code) procedure_call_code_type(code);
    new (&s.variant_join_plus.getter) incounter_getter_code_type(getter);
    return s;
  }
  
  static
  stmt_type spawn_minus(procedure_call_code_type code, incounter_getter_code_type getter) {
    stmt_type s;
    s.tag = tag_spawn_minus;
    new (&s.variant_spawn_minus.code) procedure_call_code_type(code);
    new (&s.variant_spawn_minus.getter) incounter_getter_code_type(getter);
    return s;
  }
  
  static
  stmt_type spawn_plus(procedure_call_code_type code, outset_getter_code_type getter) {
    stmt_type s;
    s.tag = tag_spawn_plus;
    new (&s.variant_spawn_plus.code) procedure_call_code_type(code);
    new (&s.variant_spawn_plus.getter) outset_getter_code_type(getter);
    return s;
  }
  
  static
  stmt_type join_minus(outset_getter_code_type getter) {
    stmt_type s;
    s.tag = tag_join_minus;
    new (&s.variant_join_minus.getter) outset_getter_code_type(getter);
    return s;
  }
  
  static
  stmt_type mk_if(predicate_code_type pred, stmt_type branch1, stmt_type branch2) {
    return cond({ std::make_pair(pred, branch1) }, branch2);
  }
  
  static
  stmt_type mk_if(predicate_code_type pred, stmt_type branch1) {
    return mk_if(pred, branch1, stmt([] (sar_type&, par_type&) { }));
  }
  
};
  
template <class Shared_activation_record, class Private_activation_record>
class linearize {
private:

  using sar = Shared_activation_record;
  using par = Private_activation_record;
  using stmt_type = stmt_type<sar, par>;
  
  using basic_block_label_type = pcfg::basic_block_label_type;
  using basic_block_type = pcfg::basic_block_type<sar, par>;
  using bbt = basic_block_type;
  using lt = basic_block_label_type;
  using block_map_type = std::map<basic_block_label_type, basic_block_type>;
  
  using parallel_loop_id_type = pcfg::parallel_loop_id_type;
  using parallel_loop_of_basic_block_type = std::map<basic_block_label_type, parallel_loop_id_type>;
  using parallel_loop_descriptor_type = pcfg::parallel_loop_descriptor_type<Private_activation_record>;
  using parallel_loop_descriptor_table_type = std::map<parallel_loop_id_type, parallel_loop_descriptor_type>;
  
  static
  lt next_block_label;
  
  static
  parallel_loop_id_type next_parallel_loop_id;
  
  using configuration_type = struct {
    block_map_type blocks;
    parallel_loop_of_basic_block_type loop_of_basic_block;
    parallel_loop_descriptor_table_type descriptor_of_loop;
  };
  
  static
  configuration_type transform(stmt_type stmt,
                               lt entry, lt exit,
                               std::vector<parallel_loop_id_type> loop_scope,
                               configuration_type config) {
    configuration_type result = config;
    auto new_label = [&] {
      return next_block_label++;
    };
    auto add_block = [&] (lt label, bbt block) {
      assert(result.blocks.find(label) == result.blocks.end());
      result.blocks.insert(std::make_pair(label, block));
      if (! loop_scope.empty()) {
        result.loop_of_basic_block.insert(std::make_pair(label, loop_scope.back()));
      }
    };
    auto new_parallel_loop_label = [&] {
      return next_parallel_loop_id++;
    };
    auto add_parallel_loop = [&] (parallel_loop_id_type label, parallel_loop_descriptor_type d) {
      assert(result.descriptor_of_loop.find(label) == result.descriptor_of_loop.end());
      result.descriptor_of_loop.insert(std::make_pair(label, d));
    };
    switch (stmt.tag) {
      case tag_stmt: {
        add_block(entry, bbt::unconditional_jump(stmt.variant_stmt.code, exit));
        break;
      }
      case tag_stmts: {
        std::vector<lt> entries;
        auto stmts = stmt.variant_stmts.stmts;
        for (int i = 0; i < stmts.size(); i++) {
          entries.push_back((i == 0) ? entry : new_label());
        }
        int i = 0;
        for (auto& s : stmts) {
          lt entry = entries.at(i);
          lt exit2 = (i + 1 == entries.size()) ? exit : entries.at(i + 1);
          result = transform(s, entry, exit2, loop_scope, result);
          i++;
        }
        break;
      }
      case tag_cond: {
        auto nb_branches = stmt.variant_cond.conds.size();
        std::vector<typename stmt_type::predicate_code_type> predicates(nb_branches);
        std::vector<lt> entries(nb_branches);
        std::vector<stmt_type> stmts(nb_branches);
        int i = 0;
        for (auto& c : stmt.variant_cond.conds) {
          predicates[i] = c.first;
          stmts[i] = c.second;
          entries[i] = new_label();
          i++;
        }
        auto otherwise_label = new_label();
        i = 0;
        for (auto& s : stmts) {
          result = transform(s, entries.at(i), exit, loop_scope, result);
          i++;
        }
        entries.push_back(otherwise_label);
        result = transform(*stmt.variant_cond.otherwise, otherwise_label, exit, loop_scope, result);
        auto selector = [=] (sar& s, par& p) {
          int i = 0;
          for (auto& pred : predicates) {
            if (pred(s, p)) {
              break;
            }
            i++;
          }
          return i;
        };
        add_block(entry, bbt::conditional_jump(selector, entries));
        break;
      }
      case tag_exit: {
        add_block(entry, bbt::unconditional_jump([] (sar&, par&) { }, pcfg::exit_block_label));
        break;
      }
      case tag_sequential_loop: {
        auto header_label = entry;
        auto body_label = new_label();
        auto predicate = stmt.variant_sequential_loop.predicate;
        auto selector = [predicate] (sar& s, par& p) {
          return predicate(s, p) ? 0 : 1;
        };
        std::vector<lt> targets = { body_label, exit };
        add_block(header_label, bbt::conditional_jump(selector, targets));
        result = transform(*stmt.variant_sequential_loop.body, body_label, header_label, loop_scope, result);
        break;
      }
      case tag_parallel_for_loop: {
        parallel_loop_id_type loop_label = new_parallel_loop_label();
        loop_scope.push_back(loop_label);
        parallel_loop_descriptor_type descriptor;
        descriptor.entry = { .pred=entry, .succ=entry };
        descriptor.exit = { .pred=exit, .succ=exit };
        descriptor.parents = loop_scope;
        auto getter = stmt.variant_parallel_for_loop.getter;
        descriptor.initializer = [getter] (Private_activation_record& p, pcfg::parallel_for_descriptor& d) {
          std::pair<int*, int*> range = getter(p);
          d.lo = range.first;
          d.hi = range.second;
        };
        add_parallel_loop(loop_label, descriptor);
        auto header_label = entry;
        auto body_label = new_label();
        auto footer_label = new_label();
        auto predicate = stmt.variant_parallel_for_loop.predicate;
        auto selector = [predicate] (sar& s, par& p) {
          return predicate(s, p) ? 0 : 1;
        };
        std::vector<lt> targets = { body_label, footer_label };
        add_block(header_label, bbt::conditional_jump(selector, targets));
        auto footer_selector = [loop_label] (sar&, par& p) {
          return p.get_join(loop_label) == nullptr ? 0 : 1;
        };
        std::vector<lt> footer_targets = { exit, pcfg::exit_block_label };
        add_block(footer_label, bbt::conditional_jump(footer_selector, footer_targets));
        result = transform(*stmt.variant_parallel_for_loop.body, body_label, header_label, loop_scope, result);
        break;
      }
      case tag_spawn_join: {
        add_block(entry, bbt::spawn_join(stmt.variant_spawn_join.code, exit));
        break;
      }
      case tag_spawn2_join: {
        auto branch1_label = entry;
        auto branch2_label = new_label();
        add_block(branch1_label, bbt::spawn2_join(stmt.variant_spawn2_join.code1, branch2_label));
        add_block(branch2_label, bbt::spawn_join(stmt.variant_spawn2_join.code2, exit));
        break;
      }
      case tag_join_plus: {
        add_block(entry, bbt::join_plus(stmt.variant_join_plus.code, stmt.variant_join_plus.getter, exit));
        break;
      }
      case tag_spawn_minus: {
        add_block(entry, bbt::spawn_minus(stmt.variant_spawn_minus.code, stmt.variant_spawn_minus.getter, exit));
        break;
      }
      case tag_spawn_plus: {
        add_block(entry, bbt::spawn_plus(stmt.variant_spawn_plus.code, stmt.variant_spawn_plus.getter, exit));
        break;
      }
      case tag_join_minus: {
        add_block(entry, bbt::join_minus(stmt.variant_join_minus.getter, exit));
        break;
      }
      default: {
        break;
      }
    }
    return result;
  }
  
public:
  
  static
  pcfg::cfg_type<Shared_activation_record> transform(stmt_type stmt) {
    lt entry = pcfg::entry_block_label;
    lt exit = pcfg::exit_block_label;
    configuration_type result = transform(stmt, entry, exit, { }, { });
    pcfg::cfg_type<Shared_activation_record> cfg(result.blocks.size());
    for (int i = 0; i < cfg.nb_basic_blocks(); i++) {
      cfg[i] = result.blocks[i];
    }
    auto nb_loops = result.descriptor_of_loop.size();
    assert(nb_loops == next_parallel_loop_id);
    cfg.loop_descriptors.resize(nb_loops);
    for (auto& p : result.descriptor_of_loop) {
      cfg.loop_descriptors[p.first] = p.second;
    }
    cfg.loop_of.resize(cfg.nb_basic_blocks());
    for (int i = 0; i < cfg.nb_basic_blocks(); i++) {
      parallel_loop_id_type loop_id = pcfg::not_a_parallel_loop_id;
      auto it = result.loop_of_basic_block.find(i);
      if (it != result.loop_of_basic_block.end()) {
        loop_id = it->second;
      }
      cfg.loop_of[i] = loop_id;
    }
    return cfg;
  }
  
};
  
template <class Sar, class Par>
typename linearize<Sar, Par>::linearize::lt linearize<Sar, Par>::next_block_label = pcfg::entry_block_label + 1;
  
template <class Sar, class Par>
pcfg::parallel_loop_id_type linearize<Sar, Par>::next_parallel_loop_id = 0;
  
} // end namespace
  
} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/
/* Macros */

#define encore_pcfg_driver(edsl) \
std::pair<edsl::stack_type, int> run(edsl::stack_type stack, int fuel) const { \
  return edsl::step(cfg, stack, fuel); \
} \
\
std::pair<edsl::extended_stack_type, int> run(edsl::extended_stack_type stack, int fuel) const { \
  return edsl::step(cfg, stack, fuel); \
} \
\
void promote(edsl::interpreter<edsl::stack_type>* interp) const { \
  edsl::promote(cfg, interp); \
} \
\
void promote(edsl::interpreter<edsl::extended_stack_type>* interp) const { \
  edsl::promote(cfg, interp); \
}

#define encore_pcfg_default_private_activation_record(edsl) \
class private_activation_record : public edsl::private_activation_record { };

#define encore_pcfg_declare(edsl, name, sar, par, bb) \
encore_pcfg_driver(edsl::pcfg) \
\
using cfg_type = edsl::pcfg::cfg_type<name>; \
using sar = name; \
using par = private_activation_record; \
using stt = edsl::pcfg::stack_type; \
using bb = typename cfg_type::basic_block_type; \
\
template <class Activation_record, class ...Args> \
static stt ecall(stt s, Args... args) { \
  return edsl::pcfg::push_call<Activation_record>(s, args...); \
} \
\
static \
cfg_type cfg; \

#define encore_dc_declare(edsl, name, sar, par, dc, get_dc) \
encore_pcfg_default_private_activation_record(edsl::pcfg) \
encore_pcfg_declare(edsl, name, sar, par, _bb0123) \
using dc = edsl::dc::stmt_type<sar, par>; \
\
static \
cfg_type get_cfg() { \
  return edsl::dc::linearize<sar, par>::transform(get_dc()); \
} \

#define encore_pcfg_allocate(name, get_cfg) \
name::cfg_type name::cfg = name::get_cfg(); \

#define encore_dc_loop_declare(edsl, name, sar, par, dc, get_dc) \
encore_pcfg_declare(edsl, name, sar, par, _bb0123) \
using dc = edsl::dc::stmt_type<sar, par>; \
\
static \
cfg_type get_cfg() { \
return edsl::dc::linearize<sar, par>::transform(get_dc()); \
} \

#define encore_pcfg_allocate(name, get_cfg) \
name::cfg_type name::cfg = name::get_cfg(); \

#endif /*! _ENCORE_EDSL_H_ */
