
#include <functional>
#include <vector>
#include <algorithm>
#include <memory>

#include "vertex.hpp"
#include "cactus.hpp"
#include "stats.hpp"

#ifndef _ENCORE_SCHED_EDSL_H_
#define _ENCORE_SCHED_EDSL_H_

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
    tag = other.t;
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
        new (&variant_unconditional_jump.code) unconditional_jump_code_type();
        variant_unconditional_jump.code = std::move(other.variant_unconditional_jump.code);
        variant_unconditional_jump.next = std::move(other.variant_unconditional_jump.next);
        break;
      }
      case tag_conditional_jump: {
        new (&variant_conditional_jump.code) conditional_jump_code_type();
        variant_conditional_jump.code = std::move(other.variant_conditional_jump.code);
        new (&variant_conditional_jump.targets) std::vector<basic_block_label_type>();
        variant_conditional_jump.targets = std::move(other.variant_conditional_jump.targets);
        break;
      }
      case tag_spawn_join: {
        new (&variant_spawn_join.code) procedure_call_code_type();
        variant_spawn_join.code = std::move(other.variant_spawn_join.code);
        variant_spawn_join.next = std::move(other.variant_spawn_join.next);
        break;
      }
      case tag_spawn2_join: {
        new (&variant_spawn2_join.code1) procedure_call_code_type();
        variant_spawn2_join.code1 = std::move(other.variant_spawn2_join.code1);
        variant_spawn2_join.next = std::move(other.variant_spawn2_join.next);
        break;
      }
      case tag_tail: {
        new (&variant_tail.code) procedure_call_code_type();
        variant_tail.code = std::move(other.variant_tail.code);
        break;
      }
      case tag_join_plus: {
        new (&variant_join_plus.code) procedure_call_code_type();
        variant_join_plus.code = std::move(other.variant_join_plus.code);
        variant_join_plus.getter = std::move(other.variant_join_plus.getter);
        variant_join_plus.next = std::move(other.variant_join_plus.next);
        break;
      }
      case tag_spawn_minus: {
        new (&variant_spawn_minus.code) procedure_call_code_type();
        variant_spawn_minus.code = std::move(other.variant_spawn_minus.code);
        variant_spawn_minus.getter = std::move(other.variant_spawn_minus.getter);
        variant_spawn_minus.next = std::move(other.variant_spawn_minus.next);
        break;
      }
      case tag_spawn_plus: {
        new (&variant_spawn_plus.code) procedure_call_code_type();
        variant_spawn_plus.code = std::move(other.variant_spawn_plus.code);
        variant_spawn_plus.getter = std::move(other.variant_spawn_plus.getter);
        variant_spawn_plus.next = std::move(other.variant_spawn_plus.next);
        break;
      }
      case tag_join_minus: {
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
    b.t = tag_join_plus;
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
    b.t = tag_spawn_minus;
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
    b.t = tag_spawn_plus;
    new (&b.variant_spawn_plus.code) procedure_call_code_type(code);
    new (&b.variant_spawn_plus.getter) outset_getter_code_type(getter);
    b.variant_spawn_plus.next = next;
    return b;
  }
  
  static
  basic_block_type join_minus(outset_getter_code_type getter,
                              basic_block_label_type next) {
    basic_block_type b;
    b.t = tag_join_minus;
    new (&b.variant_join_minus.getter) outset_getter_code_type(getter);
    b.variant_join_minus.next = next;
    return b;
  }
  
};
  
template <class Shared_activation_record>
using private_activation_record_of = typename Shared_activation_record::private_activation_record;
  
template <class Sar> // Sar := Shared_activation_record
using cfg_type = std::vector<basic_block_type<Sar, private_activation_record_of<Sar>>>;
  
/*---------------------------------------------------------------------*/
/* Parallel control-flow graph interpreter */
  
using trampoline_type = struct {
  basic_block_label_type pred;
  basic_block_label_type succ;
};
  
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
    assert(trampoline.succ != exit_block_label);
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
  assert(cactus::empty(s.stack.first));
  assert(! cactus::empty(s.stack.second));
  cactus::pop_back<private_activation_record>(s.stack.second);
  assert(cactus::empty(s.stack.second));
  delete_stack(s.stack);
}
  
bool empty_stack(extended_stack_type s) {
  assert(! cactus::empty(s.stack.second));
  return cactus::peek_front<private_activation_record>(s.stack.second).nb_strands() == 0;
}
  
template <class Shared_activation_record, class ...Args>
extended_stack_type push_call(extended_stack_type s, Args... args) {
  s.stack = push_call<Shared_activation_record>(s.stack, args...);
  return s;
}

extended_stack_type pop_call(extended_stack_type s) {
  s.stack = pop_call(s.stack);
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
  s1.stack.first = s.stack.first;
  s1.stack.first.last = s1.stack.first.first;
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
    while (! empty_stack(stack) && fuel >= 1) {
      auto result = peek_newest_shared_frame<shared_activation_record>(stack).run(stack, fuel);
      stack = result.first;
      fuel = result.second;
    }
    if (fuel == suspend_tag) {
      suspend(this);
      fuel = 0;
    } else if (! empty_stack(stack)) {
      assert(fuel == 0);
      peek_newest_shared_frame<shared_activation_record>(stack).promote(this);
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
  auto& shared_newest = peek_newest_shared_frame<Shared_activation_record>(stack);
  auto& private_newest = peek_newest_private_frame<private_activation_record>(stack);
  basic_block_label_type pred = private_newest.trampoline.succ;
  basic_block_label_type succ;
  auto& block = cfg[pred];
  fuel--;
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
  if (succ == exit_block_label) {
    stack = pop_call(stack);
  } else {
    private_newest.trampoline.pred = pred;
    private_newest.trampoline.succ = succ;
  }
  return std::make_pair(stack, fuel);
}

template <class Shared_activation_record, class Stack>
void promote(cfg_type<Shared_activation_record>& cfg, interpreter<Stack>* interp) {
  using private_activation_record = private_activation_record_of<Shared_activation_record>;
  Stack& stack = interp->stack;
  assert(! empty_stack(stack));
  auto& shared_oldest = peek_oldest_shared_frame<Shared_activation_record>(stack);
  auto& private_oldest = peek_oldest_private_frame<private_activation_record>(stack);
  auto& block = cfg[private_oldest.trampoline.pred];
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
      auto& spawn_join_block = cfg[pred];
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
  
} // end namespace
  
} // end namespace
} // end namespace

/*---------------------------------------------------------------------*/
/* Macros */

#define encore_pcfg_driver \
std::pair<dsl::stack_type, int> run(dsl::stack_type stack, int fuel) const { \
  return dsl::step(cfg, stack, fuel); \
} \
\
std::pair<dsl::extended_stack_type, int> run(dsl::extended_stack_type stack, int fuel) const { \
  return dsl::step(cfg, stack, fuel); \
} \
\
void promote(dsl::interpreter<dsl::stack_type>* interp) const { \
  dsl::promote(cfg, interp); \
} \
\
void promote(dsl::interpreter<dsl::extended_stack_type>* interp) const { \
  dsl::promote(cfg, interp); \
}

#define encore_pcfg_default_private_activation_record \
class private_activation_record : public dsl::private_activation_record { };

#define encore_pcfg_private_shared_driver(split2) \
std::pair<sched::vertex*, sched::vertex*> split(dsl::interpreter<dsl::stack_type>* interp, int nb) { \
  return split2(interp, nb); \
} \
\
std::pair<sched::vertex*, sched::vertex*> split(dsl::interpreter<dsl::extended_stack_type>* interp, int nb) { \
  return split2(interp, nb); \
}

#endif /*! _ENCORE_SCHED_EDSL_H_ */
