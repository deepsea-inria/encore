
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

using tag = enum {
  tag_unconditional_jump, tag_conditional_jump,
  tag_spawn_join, tag_spawn2_join,
  tag_demand, tag_none
};

template <class Activation_record>
class basic_block_type {
public:
  
  basic_block_type() : t(tag_none) { }

  ~basic_block_type() {
    switch (t) {
      case tag_unconditional_jump: {
        using fct = std::function<void(Activation_record&)>;
        variant_unconditional_jump.code.~fct();
        break;
      }
      case tag_conditional_jump: {
        using fct = std::function<int(Activation_record&)>;
        variant_conditional_jump.code.~fct();
        using blks = std::vector<basic_block_label_type>;
        variant_conditional_jump.targets.~blks();
        break;
      }
      case tag_spawn_join: {
        using fct = std::function<stack_type(Activation_record&, stack_type)>;
        variant_spawn_join.code.~fct();
        break;
      }
      case tag_spawn2_join: {
        using fct = std::function<stack_type(Activation_record&, stack_type)>;
        variant_spawn2_join.code1.~fct();
        break;
      }
      case tag_demand: {
        // nothing to do here
        break;
      }
      default:
        break;
    }
  }
  
  basic_block_type(basic_block_type const& other) {
    t = other.t;
    switch (t) {
      case tag_unconditional_jump: {
        new (&variant_unconditional_jump.code) std::function<void(Activation_record&)>(other.variant_unconditional_jump.code);
        variant_unconditional_jump.next = other.variant_unconditional_jump.next;
        break;
      }
      case tag_conditional_jump: {
        new (&variant_conditional_jump.code) std::function<int(Activation_record&)>(other.variant_conditional_jump.code);
        new (&variant_conditional_jump.targets) std::vector<basic_block_label_type>(other.variant_conditional_jump.targets);
        break;
      }
      case tag_spawn_join: {
        new (&variant_spawn_join.code) std::function<stack_type(Activation_record&, stack_type)>(other.variant_spawn_join.code);
        variant_spawn_join.next = other.variant_spawn_join.next;
        break;
      }
      case tag_spawn2_join: {
        new (&variant_spawn2_join.code1) std::function<stack_type(Activation_record&, stack_type)>(other.variant_spawn2_join.code1);
        variant_spawn2_join.next = other.variant_spawn2_join.next;
        break;
      }
      case tag_demand: {
        variant_demand.next = other.variant_demand.next;
        break;
      }
      default:
        break;
    }
  }
  
  basic_block_type(basic_block_type&& other) {
    t = other.t;
    other.t = tag_none;
    switch (t) {
      case tag_unconditional_jump: {
        new (&variant_unconditional_jump.code) std::function<void(Activation_record&)>();
        variant_unconditional_jump.code = std::move(other.variant_unconditional_jump.code);
        variant_unconditional_jump.next = std::move(other.variant_unconditional_jump.next);
        break;
      }
      case tag_conditional_jump: {
        new (&variant_conditional_jump.code) std::function<int(Activation_record&)>();
        variant_conditional_jump.code = std::move(other.variant_conditional_jump.code);
        new (&variant_conditional_jump.targets) std::vector<basic_block_label_type>();
        variant_conditional_jump.targets = std::move(other.variant_conditional_jump.targets);
        break;
      }
      case tag_spawn_join: {
        new (&variant_spawn_join.code) std::function<stack_type(Activation_record&, stack_type)>();
        variant_spawn_join.code = std::move(other.variant_spawn_join.code);
        variant_spawn_join.next = std::move(other.variant_spawn_join.next);
        break;
      }
      case tag_spawn2_join: {
        new (&variant_spawn2_join.code1) std::function<stack_type(Activation_record&, stack_type)>();
        variant_spawn2_join.code1 = std::move(other.variant_spawn2_join.code1);
        variant_spawn2_join.next = std::move(other.variant_spawn2_join.next);
        break;
      }
      case tag_demand: {
        variant_demand.next = std::move(other.variant_demand.next);
        break;
      }
      default:
        break;
    }
  }
  
  basic_block_type& operator=(basic_block_type const&) = delete;
  basic_block_type& operator=(basic_block_type&&) = delete;
  
  tag t;
  
  union {
    struct {
      std::function<void(Activation_record&)> code;
      basic_block_label_type next;
    } variant_unconditional_jump;
    struct {
      std::function<int(Activation_record&)> code;
      std::vector<basic_block_label_type> targets;
    } variant_conditional_jump;
    struct {
      std::function<stack_type(Activation_record&, stack_type)> code;
      basic_block_label_type next;
    } variant_spawn_join;
    struct {
      std::function<stack_type(Activation_record&, stack_type)> code1;
      basic_block_label_type next; // must be the label of a spawn_join block
    } variant_spawn2_join;
    struct {
      basic_block_label_type next;
    } variant_demand;
  };
  

  static
  basic_block_type unconditional_jump(std::function<void(Activation_record&)> code,
                                      basic_block_label_type next) {
    basic_block_type b;
    b.t = tag_unconditional_jump;
    new (&b.variant_unconditional_jump.code) std::function<void(Activation_record&)>(code);
    b.variant_unconditional_jump.next = next;
    return b;
  }
  
  static
  basic_block_type conditional_jump(std::function<int(Activation_record&)> code,
                                    std::vector<basic_block_label_type> targets) {
    basic_block_type b;
    b.t = tag_conditional_jump;
    new (&b.variant_conditional_jump.code) std::function<int(Activation_record&)>(code);
    new (&b.variant_conditional_jump.targets) std::vector<basic_block_label_type>();
    b.variant_conditional_jump.targets = targets;
    return b;
  }
  
  static
  basic_block_type spawn_join(std::function<stack_type(Activation_record&, stack_type)> code,
                         basic_block_label_type next) {
    basic_block_type b;
    b.t = tag_spawn_join;
    new (&b.variant_spawn_join.code) std::function<stack_type(Activation_record&, stack_type)>(code);
    b.variant_spawn_join.next = next;
    return b;
  }
  
  static
  basic_block_type spawn2_join(std::function<stack_type(Activation_record&, stack_type)> code1,
                         basic_block_label_type next) {
    basic_block_type b;
    b.t = tag_spawn2_join;
    new (&b.variant_spawn2_join.code1) std::function<stack_type(Activation_record&, stack_type)>(code1);
    b.variant_spawn2_join.next = next;
    return b;
  }
  
  static
  basic_block_type demand(basic_block_label_type next) {
    basic_block_type b;
    b.t = tag_demand;
    b.variant_demand.next = next;
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
  
  virtual stack_type run(stack_type) = 0;
  
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
      int n = cactus::peek_front<activation_record>(stack).nb_strands();
      return std::max(1, n);
    }
  }
  
  int run(int fuel) {
    assert(fuel > 0);
    while (! cactus::empty(stack)) {
      if (fuel == 0) {
        break;
      } else {
        fuel--;
      }
      stack = cactus::peek_back<activation_record>(stack).run(stack);
    }
    if (! cactus::empty(stack)) {
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
stack_type step(cfg_type<Activation_record>& cfg, stack_type stack) {
  assert(! cactus::empty(stack));
  Activation_record& newest = cactus::peek_back<Activation_record>(stack);
  basic_block_label_type pred = newest.trampoline.succ;
  basic_block_label_type succ;
  basic_block_type<Activation_record>& block = cfg[pred];
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
    case tag_demand: {
      assert(false); // todo
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
  return stack;
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
    case tag_demand: {
      assert(false); // todo
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
