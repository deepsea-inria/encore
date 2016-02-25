
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
  
using stack_type = cactus::stack_type;
  
using basic_block_label_type = int;

static constexpr basic_block_label_type entry_block_label = 0;
static constexpr basic_block_label_type exit_block_label = -1;
  
static constexpr int suspend_tag = -1;

using tag = enum {
  tag_unconditional_jump, tag_conditional_jump,
  tag_spawn_join, tag_spawn2_join,
  tag_tail,
  tag_join_plus, tag_spawn_minus,
  tag_spawn_plus, tag_join_minus,
  tag_none
};

template <class Activation_record>
class basic_block_type {
public:
  
  using unconditional_jump_code_type = std::function<void(Activation_record&)>;
  using conditional_jump_code_type = std::function<int(Activation_record&)>;
  using procedure_call_code_type = std::function<stack_type(Activation_record&, stack_type)>;
  using incounter_getter_code_type = std::function<sched::incounter**(Activation_record&)>;
  using outset_getter_code_type = std::function<sched::outset**(Activation_record&)>;
  
  tag t;
  
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
  
  basic_block_type() : t(tag_none) { }
  
  void copy_constructor(basic_block_type const& other) {
    t = other.t;
    switch (t) {
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
  
  basic_block_type(basic_block_type const& other) {
    copy_constructor(other);
  }
  
  void move_constructor(basic_block_type&& other) {
    t = other.t;
    other.t = tag_none;
    switch (t) {
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
  
  basic_block_type(basic_block_type&& other) {
    move_constructor(std::move(other));
  }
  
  ~basic_block_type() {
    switch (t) {
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
    b.t = tag_unconditional_jump;
    new (&b.variant_unconditional_jump.code) unconditional_jump_code_type(code);
    b.variant_unconditional_jump.next = next;
    return b;
  }
  
  static
  basic_block_type conditional_jump(conditional_jump_code_type code,
                                    std::vector<basic_block_label_type> targets) {
    basic_block_type b;
    b.t = tag_conditional_jump;
    new (&b.variant_conditional_jump.code) conditional_jump_code_type(code);
    new (&b.variant_conditional_jump.targets) std::vector<basic_block_label_type>();
    b.variant_conditional_jump.targets = targets;
    return b;
  }
  
  static
  basic_block_type spawn_join(procedure_call_code_type code,
                              basic_block_label_type next) {
    basic_block_type b;
    b.t = tag_spawn_join;
    new (&b.variant_spawn_join.code) procedure_call_code_type(code);
    b.variant_spawn_join.next = next;
    return b;
  }
  
  static
  basic_block_type spawn2_join(procedure_call_code_type code1,
                               basic_block_label_type next) {
    basic_block_type b;
    b.t = tag_spawn2_join;
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
  
template <class Activation_record>
using cfg_type = std::vector<basic_block_type<Activation_record>>;
  
using trampoline_type = struct {
  basic_block_label_type pred;
  basic_block_label_type succ;
};
  
class interpreter;
  
class activation_record {
public:
  
  trampoline_type trampoline = { .pred = entry_block_label, .succ = entry_block_label };
  
  virtual std::pair<stack_type, int> run(stack_type, int) = 0;
  
  virtual void promote(interpreter*) = 0;
  
  virtual int nb_strands() {
    assert(trampoline.succ != exit_block_label);
    return 1;
  }
  
  virtual void split(activation_record*) {
    assert(false); // impossible
  }
  
};
  
template <class Activation_record, class ...Args>
stack_type procedure_call(stack_type s, Args... args) {
  return cactus::push_back<Activation_record>(s, args...);
}

stack_type procedure_return(stack_type s) {
  return cactus::pop_back<activation_record>(s);
}
 
class interpreter : public sched::vertex {
public:
  
  stack_type stack;
  
  interpreter() {
    stack = cactus::new_stack();
  }
  
  ~interpreter() {
    cactus::delete_stack(stack);
  }
  
  int nb_strands() {
    if (cactus::empty(stack)) {
      return 0;
    } else {
      return cactus::peek_front<activation_record>(stack).nb_strands();
    }
  }
  
  int run(int fuel) {
    while (! cactus::empty(stack) && fuel >= 1) {
      auto result = cactus::peek_back<activation_record>(stack).run(stack, fuel);
      stack = result.first;
      fuel = result.second;
    }
    if (fuel == suspend_tag) {
      suspend(this);
      fuel = 0;
    } else if (! cactus::empty(stack)) {
      assert(fuel == 0);
      cactus::peek_back<activation_record>(stack).promote(this);
    }
    return fuel;
  }
  
  vertex* split(int nb) {
    return nullptr;
  }
  
};

template <class Activation_record>
std::pair<stack_type, int> step(cfg_type<Activation_record>& cfg, stack_type stack, int fuel) {
  assert(! cactus::empty(stack));
  Activation_record& newest = cactus::peek_back<Activation_record>(stack);
  basic_block_label_type pred = newest.trampoline.succ;
  basic_block_label_type succ;
  basic_block_type<Activation_record>& block = cfg[pred];
  fuel--;
  switch (block.t) {
    case tag_unconditional_jump: {
      block.variant_unconditional_jump.code(newest);
      succ = block.variant_unconditional_jump.next;
      break;
    }
    case tag_conditional_jump: {
      int target = block.variant_conditional_jump.code(newest);
      succ = block.variant_conditional_jump.targets[target];
      break;
    }
    case tag_spawn_join: {
      stack = block.variant_spawn_join.code(newest, stack);
      succ = block.variant_spawn_join.next;
      break;
    }
    case tag_spawn2_join: {
      stack = block.variant_spawn2_join.code1(newest, stack);
      succ = block.variant_spawn2_join.next;
      break;
    }
    case tag_tail: {
      stack = cactus::pop_back<Activation_record>(stack);
      stack = block.variant_tail.code(newest, stack);
      break;
    }
    case tag_join_plus: {
      *block.variant_join_plus.getter(newest) = nullptr;
      stack = block.variant_join_plus.code(newest, stack);
      succ = block.variant_join_plus.next;
      break;
    }
    case tag_spawn_minus: {
      stack = block.variant_spawn_minus.code(newest, stack);
      succ = block.variant_spawn_minus.next;
      break;
    }
    case tag_spawn_plus: {
      *block.variant_spawn_plus.getter(newest) = nullptr;
      stack = block.variant_spawn_plus.code(newest, stack);
      succ = block.variant_spawn_plus.next;
      break;
    }
    case tag_join_minus: {
      fuel = suspend_tag;
      break;
    }
    default: {
      assert(false);
    }
  }
  if (succ == exit_block_label) {
    stack = procedure_return(stack);
  } else {
    newest.trampoline.pred = pred;
    newest.trampoline.succ = succ;
  }
  return std::make_pair(stack, fuel);
}

template <class Activation_record>
void promote(cfg_type<Activation_record>& cfg, interpreter* interp) {
  stack_type& stack = interp->stack;
  assert(! cactus::empty(stack));
  Activation_record& oldest = cactus::peek_front<Activation_record>(stack);
  basic_block_type<Activation_record>& block = cfg[oldest.trampoline.pred];
  switch (block.t) {
    case tag_unconditional_jump: {
      schedule(interp);
      break;
    }
    case tag_conditional_jump: {
      schedule(interp);
      break;
    }
    case tag_spawn_join: {
      interpreter* join = interp;
      interpreter* branch = new interpreter;
      auto stacks = cactus::split_front<Activation_record>(interp->stack);
      join->stack = stacks.first;
      branch->stack = stacks.second;
      new_edge(branch, join);
      release(branch);
      stats::on_promotion();
      break;
    }
    case tag_spawn2_join: {
      interpreter* join = interp;
      interpreter* branch1 = new interpreter;
      interpreter* branch2 = new interpreter;
      auto stacks = cactus::split_front<Activation_record>(interp->stack);
      join->stack = stacks.first;
      branch1->stack = stacks.second;
      branch2->stack = cactus::new_stack();
      basic_block_label_type pred = oldest.trampoline.succ;
      basic_block_type<Activation_record>& spawn_join_block = cfg[pred];
      assert(spawn_join_block.t == tag_spawn_join);
      oldest.trampoline.pred = pred;
      oldest.trampoline.succ = spawn_join_block.variant_spawn_join.next;
      branch2->stack = spawn_join_block.variant_spawn_join.code(oldest, branch2->stack);
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
      interpreter* join = interp;
      interpreter* branch = new interpreter;
      auto stacks = cactus::split_front<Activation_record>(interp->stack);
      join->stack = stacks.first;
      branch->stack = stacks.second;
      assert(*block.variant_join_plus.getter(oldest) == nullptr);
      *block.variant_join_plus.getter(oldest) = join->get_incounter();
      new_edge(branch, join);
      release(branch);
      stats::on_promotion();
      break;
    }
    case tag_spawn_minus: {
      interpreter* continuation = interp;
      interpreter* branch = new interpreter;
      auto stacks = cactus::split_front<Activation_record>(interp->stack);
      continuation->stack = stacks.first;
      branch->stack = stacks.second;
      sched::incounter* incounter = *block.variant_spawn_minus.getter(oldest);
      assert(incounter != nullptr);
      new_edge(branch, incounter);
      schedule(continuation);
      release(branch);
      stats::on_promotion();
      break;
    }
    case tag_spawn_plus: {
      interpreter* continuation = interp;
      interpreter* branch = new interpreter;
      auto stacks = cactus::split_front<Activation_record>(interp->stack);
      continuation->stack = stacks.first;
      branch->stack = stacks.second;
      assert(*block.variant_spawn_plus.getter(oldest) == nullptr);
      *block.variant_spawn_plus.getter(oldest) = branch->get_outset();
      schedule(continuation);
      release(branch);
      stats::on_promotion();
      break;
    }
    case tag_join_minus: {
      interpreter* join = interp;
      sched::outset* outset = *block.variant_join_minus.getter(oldest);
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

#endif /*! _ENCORE_SCHED_EDSL_H_ */
