
#include <functional>
#include <vector>
#include <map>
#include <algorithm>
#include <memory>
#include <array>

#include "vertex.hpp"
#include "cactus-plus.hpp"
#include "stats.hpp"

#ifndef _ENCORE_EDSL_H_
#define _ENCORE_EDSL_H_

namespace encore {
namespace edsl {
  
/*---------------------------------------------------------------------*/
/* Parallel Control-Flow Graph */
  
namespace pcfg {
  
namespace cactus = cactus_stack::plus;
  
using stack_type = cactus::stack_type;
  
class shared_activation_record;
  
class private_activation_record;
  
class interpreter;
  
static constexpr
int suspend_tag = -1;
  
using basic_block_label_type = int;

static constexpr
basic_block_label_type entry_block_label = 0;
  
static constexpr
basic_block_label_type exit_block_label = -1;

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
  using conditional_jump_code_type = std::function<basic_block_label_type(sar_type&, par_type&)>;
  using procedure_call_code_type = std::function<stack_type(sar_type&, par_type&, cactus::parent_link_type, stack_type)>;
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
      basic_block_label_type next;
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
  basic_block_type conditional_jump(conditional_jump_code_type code) {
    basic_block_type b;
    b.tag = tag_conditional_jump;
    new (&b.variant_conditional_jump.code) conditional_jump_code_type(code);
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
  
class children_record {
public:
  
  int nb = 0;
  
  using future_type = std::pair<sched::outset*, private_activation_record*>;
  
  std::vector<future_type> futures;
  
};
  
class parallel_loop_activation_record {
public:
  
  virtual int nb_strands() = 0;
  
  virtual void split(parallel_loop_activation_record*, int) = 0;
  
  virtual sched::vertex*& get_join() = 0;
  
  virtual std::unique_ptr<children_record>& get_children() = 0;
  
  virtual private_activation_record*& get_destination() = 0;
  
};

using parallel_loop_id_type = int;

template <class Private_activation_record>
class parallel_loop_descriptor_type {
public:
  
  using par = Private_activation_record;
  
  using join_type = enum {
    join_trivial,
    join_binary_associative_combine,
    join_binary_associative_commutative_combine
  };
  
  join_type join = join_binary_associative_commutative_combine;
  
  trampoline_type entry;
  
  trampoline_type exit;
  
  // ids in parents vector are ordered from outermost to innermost
  std::vector<parallel_loop_id_type> parents;
  
  std::function<void(par&, parallel_loop_activation_record*)> initializer;
  
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
  // loop_of[i] yields the id of the parallel loop that most immediately
  // encloses the basic block labeled i
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
  
class shared_activation_record {
public:
  
  virtual std::pair<stack_type, int> run(stack_type, int) const = 0;
  
  virtual void promote_mark(interpreter*, private_activation_record*) = 0;
  
  virtual sched::outset* get_dependency_of_join_minus(stack_type stack) = 0;
  
};
  
sched::vertex* dummy_join = nullptr;
  
std::unique_ptr<children_record> dummy_children = nullptr;
  
private_activation_record* dummy_destination = nullptr;
  
class private_activation_record {
public:
  
  trampoline_type trampoline = { .pred = entry_block_label, .succ = entry_block_label };
  
  virtual int nb_strands() {
    return 1;
  }
  
  virtual std::pair<sched::vertex*, sched::vertex*> split(interpreter*, int) {
    assert(false); // impossible
    return std::make_pair(nullptr, nullptr);
  }
  
  virtual sched::vertex*& get_join(parallel_loop_id_type) {
    assert(false); // impossible
    return dummy_join;
  }
  
  virtual parallel_loop_activation_record* loop_activation_record_of(parallel_loop_id_type id) {
    assert(false); // impossible
    return nullptr;
  }
  
};
  
template <class Shared_activation_record>
Shared_activation_record* get_shared_frame_pointer(char* _ar) {
  return (Shared_activation_record*)(_ar + sizeof(size_t));
}
  
template <class Private_activation_record>
Private_activation_record* get_private_frame_pointer(char* _ar) {
  size_t shared_szb = *((size_t*)_ar);
  return (Private_activation_record*)(_ar + sizeof(size_t) + shared_szb);
}
  
bool is_splittable(char* _ar) {
  return (get_private_frame_pointer<private_activation_record>(_ar)->nb_strands() >= 2);
}
  
template <class Shared_activation_record, class Private_activation_record>
stack_type create_stack(Shared_activation_record* sar, Private_activation_record* par) {
  static constexpr size_t shared_szb = sizeof(Shared_activation_record);
  static constexpr size_t frame_szb = sizeof(size_t) + shared_szb + sizeof(Private_activation_record);
  return cactus::create_stack<frame_szb>(cactus::Parent_link_sync, [&] (char* _ar) {
    new ((size_t*)_ar) size_t(sizeof(Shared_activation_record*));
    new (get_shared_frame_pointer<Shared_activation_record*>(_ar)) Shared_activation_record*(sar);
    new (get_private_frame_pointer<Private_activation_record>(_ar)) Private_activation_record(*par);
  }, [&] (char* _ar) {
    return is_splittable(_ar);
  });
}

bool empty_stack(stack_type s) {
  return cactus::empty(s);
}

bool empty_mark_stack(stack_type s) {
  return cactus::empty_mark(s);
}

template <class Shared_activation_record, class ...Args>
stack_type push_call(stack_type s, cactus::parent_link_type plt, Args... args) {
  // The line below is commented out to avoid a GCC bug whereby GCC segfaults.
  // using private_activation_record = private_activation_record_of<Shared_activation_record>;
  using private_activation_record = typename Shared_activation_record::private_activation_record;
  static constexpr size_t shared_szb = sizeof(Shared_activation_record);
  static constexpr size_t frame_szb = sizeof(size_t) + shared_szb + sizeof(private_activation_record);
  return cactus::push_back<frame_szb>(s, plt, [&] (char* _ar)  {
    new ((size_t*)_ar) size_t(shared_szb);
    new (get_shared_frame_pointer<Shared_activation_record>(_ar)) Shared_activation_record(args...);
    new (get_private_frame_pointer<private_activation_record>(_ar)) private_activation_record;
  }, [&] (char* _ar) {
    return is_splittable(_ar);
  });
}
  
template <class Shared_activation_record>
stack_type pop_call(stack_type s) {
  using private_activation_record = typename Shared_activation_record::private_activation_record;
  return cactus::pop_back(s, [&] (char* _ar, cactus::shared_frame_type sft) {
    if (sft == cactus::Shared_frame_direct) {
      get_shared_frame_pointer<Shared_activation_record>(_ar)->~Shared_activation_record();
    } else {
      *get_shared_frame_pointer<Shared_activation_record*>(_ar) = nullptr;
    }
    get_private_frame_pointer<private_activation_record>(_ar)->~private_activation_record();
  });
}
  
template <class Shared_activation_record>
Shared_activation_record* resolve_shared_frame_pointer(char* _ar, cactus::shared_frame_type sft) {
  Shared_activation_record* sar;
  if (sft == cactus::Shared_frame_indirect) {
    Shared_activation_record** p = get_shared_frame_pointer<Shared_activation_record*>(_ar);
    assert(p != nullptr);
    sar = *p;
  } else {
    sar = get_shared_frame_pointer<Shared_activation_record>(_ar);
  }
  return sar;
}

template <class Shared_activation_record>
Shared_activation_record& peek_newest_shared_frame(stack_type s) {
  Shared_activation_record* sar;
  cactus::peek_back(s, [&] (cactus::shared_frame_type sft, cactus::call_link_type, char* _ar) {
    sar = resolve_shared_frame_pointer<Shared_activation_record>(_ar, sft);
  });
  return *sar;
}

template <class Private_activation_record>
Private_activation_record& peek_newest_private_frame(stack_type s) {
  Private_activation_record* par;
  cactus::peek_back(s, [&] (cactus::shared_frame_type, cactus::call_link_type, char* _ar) {
    par = get_private_frame_pointer<Private_activation_record>(_ar);
  });
  return *par;
}
  
using peek_mark_result_tag = enum {
  Peek_mark_none, Peek_mark_fork, Peek_mark_loop_split
};
  
using peek_mark_result_type = struct {
  peek_mark_result_tag tag;
  shared_activation_record* sar;
  private_activation_record* par;
};

peek_mark_result_type peek_mark(stack_type s) {
  peek_mark_result_type r;
  if (empty_mark_stack(s)) {
    r.tag = Peek_mark_none;
    return r;
  }
  cactus::peek_mark(s, [&] (cactus::shared_frame_type sft,
                            cactus::call_link_type clt,
                            char* _ar,
                            cactus::shared_frame_type pred_sft,
                            char* _pred_ar) {
    r.par = get_private_frame_pointer<private_activation_record>(_ar);
    if ((clt == cactus::Call_link_async) && (_pred_ar != nullptr)) {
      r.tag = Peek_mark_fork;
      r.sar = resolve_shared_frame_pointer<shared_activation_record>(_pred_ar, pred_sft);
      r.par = get_private_frame_pointer<private_activation_record>(_pred_ar);
    } else if (r.par->nb_strands() >= 2) {
      r.tag = Peek_mark_loop_split;
      r.sar = resolve_shared_frame_pointer<shared_activation_record>(_ar, sft);
    } else {
      r.tag = Peek_mark_none;
    }
  });
  return r;
}

template <class Shared_activation_record>
Shared_activation_record& peek_marked_shared_frame(stack_type s) {
  Shared_activation_record* sar;
  cactus::peek_mark(s, [&] (cactus::shared_frame_type sft,
                            cactus::call_link_type,
                            char* _ar,
                            cactus::shared_frame_type,
                            char*) {
    sar = resolve_shared_frame_pointer<Shared_activation_record>(_ar, sft);
  });
  return *sar;
}

template <class Private_activation_record>
Private_activation_record& peek_marked_private_frame(stack_type s) {
  Private_activation_record* par;
  cactus::peek_mark(s, [&] (cactus::shared_frame_type sft,
                            cactus::call_link_type,
                            char* _ar,
                            cactus::shared_frame_type,
                            char*) {
    par = get_private_frame_pointer<Private_activation_record>(_ar);
  });
  return *par;
}
  
std::pair<stack_type, stack_type> split_stack(stack_type s) {
  return cactus::split_mark(s, [&] (char* _ar) {
    return is_splittable(_ar);
  });
}

std::pair<stack_type, stack_type> fork_stack(stack_type s) {
  return cactus::fork_mark(s, [&] (char* _ar) {
    return is_splittable(_ar);
  });
}
  
class interpreter : public sched::vertex {
public:

  stack_type stack;
  
  interpreter() {
    stack = cactus::create_stack();
  }
  
  interpreter(stack_type stack)
  : stack(stack) { }
  
  int nb_strands() {
    if (empty_stack(stack)) {
      return 0;
    } else if (empty_mark_stack(stack)) {
      return 1;
    } else {
      return peek_marked_private_frame<private_activation_record>(stack).nb_strands();
    }
  }
  
  int run(int fuel) {
    stack_type s = stack;
    while (! empty_stack(s) && (fuel >= 1)) {
      auto r = peek_newest_shared_frame<shared_activation_record>(s).run(s, fuel);
      s = r.first;
      fuel = r.second;
    }
    stack = s;
    if (fuel == suspend_tag) {
      fuel = 0;
      is_suspended = true;
    }
    if (nb_strands() == 0) {
      assert(! is_suspended);
      return fuel;
    }
    if (fuel > 0) {
      return fuel;
    }
    assert(fuel == 0);
    auto r = peek_mark(stack);
    switch (r.tag) {
      case Peek_mark_none: {
        if (is_suspended) {
          is_suspended = false;
          auto& par = peek_newest_shared_frame<shared_activation_record>(stack);
          auto dep = par.get_dependency_of_join_minus(stack);
          sched::new_edge(dep, this);
        } else {
          schedule(this);
        }
        break;
      }
      case Peek_mark_fork: {
        r.sar->promote_mark(this, r.par);
        break;
      }
      case Peek_mark_loop_split: {
        auto r = split(nb_strands() / 2);
        schedule(r.second);
        schedule(r.first);
        stats::on_promotion();
        break;
      }
      default: {
        assert(false);
        break;
      }
    }
    return fuel;
  }
  
  std::pair<vertex*, vertex*> split(int nb) {
    return peek_marked_private_frame<private_activation_record>(stack).split(this, nb);
  }
  
};

template <class Shared_activation_record>
std::pair<stack_type, int> step(cfg_type<Shared_activation_record>& cfg, stack_type stack, int fuel) {
  using private_activation_record = private_activation_record_of<Shared_activation_record>;
  assert(! empty_stack(stack));
  fuel--;
  auto& sar = peek_newest_shared_frame<Shared_activation_record>(stack);
  auto& par = peek_newest_private_frame<private_activation_record>(stack);
  basic_block_label_type pred = par.trampoline.succ;
  basic_block_label_type succ;
  if (pred == exit_block_label) {
    stack = pop_call<Shared_activation_record>(stack);
    return std::make_pair(stack, fuel);
  }
  assert(pred >= 0 && pred < cfg.nb_basic_blocks());
  auto& block = cfg.basic_blocks[pred];
  switch (block.tag) {
    case tag_unconditional_jump: {
      block.variant_unconditional_jump.code(sar, par);
      succ = block.variant_unconditional_jump.next;
      break;
    }
    case tag_conditional_jump: {
      succ = block.variant_conditional_jump.code(sar, par);
      break;
    }
    case tag_spawn_join: {
      stack = block.variant_spawn_join.code(sar, par, cactus::Parent_link_sync, stack);
      succ = block.variant_spawn_join.next;
      break;
    }
    case tag_spawn2_join: {
      stack = block.variant_spawn2_join.code1(sar, par, cactus::Parent_link_async, stack);
      succ = block.variant_spawn2_join.next;
      break;
    }
    case tag_tail: {
      stack = pop_call<Shared_activation_record>(stack);
      stack = block.variant_tail.code(sar, par, cactus::Parent_link_sync, stack);
      succ = block.variant_tail.next;
      break;
    }
    case tag_join_plus: {
      *block.variant_join_plus.getter(sar, par) = nullptr;
      stack = block.variant_join_plus.code(sar, par, cactus::Parent_link_async, stack);
      succ = block.variant_join_plus.next;
      break;
    }
    case tag_spawn_minus: {
      stack = block.variant_spawn_minus.code(sar, par, cactus::Parent_link_async, stack);
      succ = block.variant_spawn_minus.next;
      break;
    }
    case tag_spawn_plus: {
      *block.variant_spawn_plus.getter(sar, par) = nullptr;
      stack = block.variant_spawn_plus.code(sar, par, cactus::Parent_link_async, stack);
      succ = block.variant_spawn_plus.next;
      break;
    }
    case tag_join_minus: {
      sched::outset* outset = *block.variant_join_minus.getter(sar, par);
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
  par.trampoline.pred = pred;
  par.trampoline.succ = succ;
  return std::make_pair(stack, fuel);
}

template <class Shared_activation_record, class Private_activation_record>
void promote_mark(cfg_type<Shared_activation_record>& cfg, interpreter* interp,
                  Shared_activation_record* sar, Private_activation_record* par) {
  basic_block_label_type pred = par->trampoline.pred;
  assert(pred != exit_block_label);
  assert(pred >= 0 && pred < cfg.nb_basic_blocks());
  auto& block = cfg.basic_blocks[pred];
  switch (block.tag) {
    case tag_spawn2_join: {
      interpreter* join = interp;
      auto stacks = fork_mark(interp->stack, [&] (char* _ar) {
        return is_splittable(_ar);
      });
      join->stack = stacks.first;
      interpreter* branch1 = new interpreter(stacks.second);
      interpreter* branch2 = new interpreter;
      basic_block_label_type pred = par->trampoline.succ;
      auto& spawn_join_block = cfg.basic_blocks[pred];
      assert(spawn_join_block.tag == tag_spawn_join);
      par->trampoline.pred = pred;
      par->trampoline.succ = spawn_join_block.variant_spawn_join.next;
      branch2->stack = spawn_join_block.variant_spawn_join.code(*sar, *par, cactus::Parent_link_sync, branch2->stack);
      new_edge(branch2, join);
      new_edge(branch1, join);
      release(branch2);
      release(branch1);
      break;
    }
    case tag_spawn_minus: {
      interpreter* continuation = interp;
      auto stacks = fork_mark(interp->stack, [&] (char* _ar) {
        return is_splittable(_ar);
      });
      continuation->stack = stacks.first;
      interpreter* branch = new interpreter(stacks.second);
      sched::incounter* incounter = *block.variant_spawn_minus.getter(*sar, *par);
      assert(incounter != nullptr);
      new_edge(branch, incounter);
      schedule(continuation);
      release(branch);
      break;
    }
    case tag_spawn_plus: {
      interpreter* continuation = interp;
      auto stacks = fork_mark(interp->stack, [&] (char* _ar) {
        return is_splittable(_ar);
      });
      continuation->stack = stacks.first;
      interpreter* branch = new interpreter(stacks.second);
      branch->enable_future();
      assert(*block.variant_spawn_plus.getter(*sar, *par) == nullptr);
      *block.variant_spawn_plus.getter(*sar, *par) = branch->get_outset();
      schedule(continuation);
      release(branch);
      break;
    }
    case tag_join_plus: {
      interpreter* continuation = interp;
      auto stacks = fork_mark(interp->stack, [&] (char* _ar) {
        return is_splittable(_ar);
      });
      continuation->stack = stacks.first;
      interpreter* branch = new interpreter(stacks.second);
      assert(*block.variant_join_plus.getter(*sar, *par) == nullptr);
      *block.variant_join_plus.getter(*sar, *par) = continuation->get_incounter();
      new_edge(branch, continuation);
      release(branch);
      stats::on_promotion();
      break;
    }
    default: {
      assert(false);
      return;
    }
  }
  stats::on_promotion();
}
  
template <class Shared_activation_record>
sched::outset* get_dependency_of_join_minus(cfg_type<Shared_activation_record>& cfg, stack_type stack) {
  using private_activation_record = private_activation_record_of<Shared_activation_record>;
  auto& sar = peek_newest_shared_frame<Shared_activation_record>(stack);
  auto& par = peek_newest_private_frame<private_activation_record>(stack);
  auto& block = cfg.basic_blocks.at(par.trampoline.pred);
  assert(block.tag == tag_join_minus);
  sched::outset* r = *block.variant_join_minus.getter(sar, par);
  assert(r != nullptr);
  return r;
}
  
static constexpr parallel_loop_id_type not_a_parallel_loop_id = -1;
 
template <class Shared_activation_record, class Private_activation_record>
class parallel_loop_private_activation_record : public private_activation_record {
public:
  
  using sar_type = Shared_activation_record;
  using par_type = Private_activation_record;
  
  parallel_loop_activation_record* loop_activation_record_of(parallel_loop_id_type id) {
    auto p = (par_type*)this;
    return p->_encore_loop_activation_record_of(id);
  }
  
  void initialize_descriptors() {
    for (parallel_loop_id_type id = 0; id < sar_type::cfg.nb_loops(); id++) {
      sar_type::cfg.loop_descriptors[id].initializer(*(par_type*)this, loop_activation_record_of(id));
    }
  }
  
  parallel_loop_id_type get_id_of_current_parallel_loop() {
    return sar_type::cfg.loop_of[trampoline.pred];
  }
  
  parallel_loop_id_type get_id_of_oldest_nonempty() {
    parallel_loop_id_type current = get_id_of_current_parallel_loop();
    if (current == not_a_parallel_loop_id) {
      return not_a_parallel_loop_id;
    }
    for (parallel_loop_id_type id : sar_type::cfg.loop_descriptors[current].parents) {
      assert(id != not_a_parallel_loop_id);
      if (loop_activation_record_of(id)->nb_strands() >= 2) {
        return id;
      }
    }
    if (loop_activation_record_of(current)->nb_strands() >= 2) {
      return current;
    } else {
      return not_a_parallel_loop_id;
    }
  }
  
  parallel_loop_activation_record* get_oldest_nonempty() {
    auto id = get_id_of_oldest_nonempty();
    if (id == not_a_parallel_loop_id) {
      return nullptr;
    } else {
      return loop_activation_record_of(id);
    }
  }
  
  sched::vertex*& get_join(parallel_loop_id_type id) {
    return loop_activation_record_of(id)->get_join();
  }
  
  sched::vertex*& get_join() {
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
    return std::max(1, d->nb_strands());
  }
  
  using split_result = std::pair<sched::vertex*, sched::vertex*>;
  
  split_result split_join_trivial(interpreter* interp0, par_type* par0, sar_type* sar0,
                                  parallel_loop_id_type id, int nb) {
    assert(nb < interp0->nb_strands());
    assert(nb > 0);
    parallel_loop_descriptor_type<par_type>& lpdescr = sar_type::cfg.loop_descriptors[id];
    interpreter* interp1 = nullptr;
    interpreter* interp2 = nullptr;
    auto lpar0 = par0->loop_activation_record_of(id);
    par_type* par1 = par0;
    sched::vertex* join = lpar0->get_join();
    if (join == nullptr) {
      join = interp0;
      auto stacks = split_stack(interp0->stack);
      interp0->stack = stacks.first;
      interpreter* interp01 = new interpreter(create_stack(sar0, par0));
      par_type* par01 = &peek_newest_private_frame<par_type>(interp01->stack);
      par01->initialize_descriptors();
      auto lpar01 = par01->loop_activation_record_of(id);
      lpar0->split(lpar01, lpar0->nb_strands());
      interp1 = new interpreter(create_stack(sar0, par01));
      par1 = &peek_newest_private_frame<par_type>(interp1->stack);
      par1->initialize_descriptors();
      auto lpar1 = par1->loop_activation_record_of(id);
      lpar01->split(lpar1, lpar01->nb_strands() - 1);
      interp0->stack = cactus::update_mark_stack(interp0->stack, [&] (char* _ar) {
        return pcfg::is_splittable(_ar);
      });
      interp01->stack = cactus::update_mark_stack(interp01->stack, [&] (char* _ar) {
        return pcfg::is_splittable(_ar);
      });
      nb--;
      std::swap(interp0->is_suspended, interp01->is_suspended);
      par0->trampoline = lpdescr.exit;
      par1->trampoline = lpdescr.entry;
      lpar1->get_join() = join;
      lpar01->get_join() = join;
      lpar0->get_join() = nullptr;
      sched::new_edge(interp01, join);
      sched::new_edge(interp1, join);
      interpreter* interp00 = new interpreter(stacks.second);
      sched::new_edge(interp00, interp01);
      release(interp01);
      release(interp00);
      interp1->release_handle->decrement();
    } else {
      interp1 = interp0;
    }
    interp2 = new interpreter(create_stack(sar0, par1));
    par_type* par2 = &peek_newest_private_frame<par_type>(interp2->stack);
    par2->initialize_descriptors();
    auto lpar2 = par2->loop_activation_record_of(id);
    par1->loop_activation_record_of(id)->split(lpar2, nb);
    lpar2->get_join() = join;
    par2->trampoline = lpdescr.entry;
    interp1->stack = cactus::update_mark_stack(interp1->stack, [&] (char* _ar) {
      return pcfg::is_splittable(_ar);
    });
    interp2->stack = cactus::update_mark_stack(interp2->stack, [&] (char* _ar) {
      return pcfg::is_splittable(_ar);
    });
    sched::new_edge(interp2, join);
    interp2->release_handle->decrement();
    return std::make_pair(interp1, interp2);
  }
  
  split_result split_join_associative_combine(interpreter* interp0, par_type* par0, sar_type* sar0,
                                              parallel_loop_id_type id, int nb) {
    parallel_loop_descriptor_type<par_type>& lpdescr = sar_type::cfg.loop_descriptors[id];
    interpreter* interp1 = interp0;
    interpreter* interp2 = nullptr;
    par_type* par1 = par0;
    sar_type* sar1 = sar0;
    auto lpar1 = par1->loop_activation_record_of(id);
    interp2 = new interpreter(create_stack(sar1, par1));
    interp2->enable_future();
    par_type* par2 = &peek_newest_private_frame<par_type>(interp2->stack);
    par2->initialize_descriptors();
    auto lpar2 = par2->loop_activation_record_of(id);
    lpar1->split(lpar2, nb);
    par2->trampoline = lpdescr.entry;
    auto& children = lpar1->get_children();
    if (! children) {
      children.reset(new children_record);
    }
    private_activation_record* destination = new par_type;
    children->futures.push_back(std::make_pair(interp2->get_outset(), destination));
    lpar2->get_destination() = destination;
    interp1->stack = cactus::update_mark_stack(interp1->stack, [&] (char* _ar) {
      return pcfg::is_splittable(_ar);
    });
    interp2->stack = cactus::update_mark_stack(interp2->stack, [&] (char* _ar) {
      return pcfg::is_splittable(_ar);
    });
    interp2->release_handle->decrement();
    return std::make_pair(interp1, interp2);
  }
  
  split_result split(interpreter* interp0, int nb) {
    split_result result;
    par_type* par0 = &peek_marked_private_frame<par_type>(interp0->stack);
    sar_type* sar0 = &peek_marked_shared_frame<sar_type>(interp0->stack);
    parallel_loop_id_type id = par0->get_id_of_oldest_nonempty();
    switch (sar_type::cfg.loop_descriptors[id].join) {
      case parallel_loop_descriptor_type<par_type>::join_trivial: {
        result = split_join_trivial(interp0, par0, sar0, id, nb);
        break;
      }
      case parallel_loop_descriptor_type<par_type>::join_binary_associative_combine: {
        result = split_join_associative_combine(interp0, par0, sar0, id, nb);
        break;
      }
      default: {
        assert(false);
      }
    }
    return result;
  }
  
};

class parallel_for_activation_record : public parallel_loop_activation_record {
public:
  
  parallel_for_activation_record()
  : lo(nullptr), hi(nullptr), join(nullptr) { }
  
  parallel_for_activation_record(int& lo, int& hi)
  : lo(&lo), hi(&hi), join(nullptr) { }
  
  int* lo;
  
  int* hi;
  
  sched::vertex* join = nullptr;
  
  int nb_strands() {
    return *hi - *lo;
  }
  
  void split(parallel_loop_activation_record* _dest, int nb) {
    parallel_for_activation_record* dest = (parallel_for_activation_record*)_dest;
    int orig = nb_strands();
    assert(nb >= 0 && nb <= orig);
    int mid = (orig - nb) + *lo;
    *(dest->hi) = *hi;
    *hi = mid;
    *(dest->lo) = mid;
    assert((dest->nb_strands() == nb) && (nb_strands() + nb == orig));
  }
  
  sched::vertex*& get_join() {
    return join;
  }
  
  std::unique_ptr<children_record>& get_children() {
    assert(false); // impossible
    return dummy_children;
  }

  private_activation_record*& get_destination() {
    assert(false); // impossible
    return dummy_destination;
  }
  
};

class parallel_combine_activation_record : public parallel_loop_activation_record {
public:
  
  parallel_combine_activation_record()
  : lo(nullptr), hi(nullptr), destination(nullptr) { }
  
  parallel_combine_activation_record(int& lo, int& hi)
  : lo(&lo), hi(&hi), destination(nullptr) { }
  
  parallel_combine_activation_record(const parallel_combine_activation_record& other)
  : lo(other.lo), hi(other.hi), destination(nullptr) { }
  
  int* lo;
  
  int* hi;
  
  std::unique_ptr<children_record> children;
  
  private_activation_record* destination;
 
  int nb_strands() {
    return *hi - *lo;
  }
  
  void split(parallel_loop_activation_record* _dest, int nb) {
    parallel_combine_activation_record* dest = (parallel_combine_activation_record*)_dest;
    int orig = nb_strands();
    assert(nb >= 0 && nb <= orig);
    int mid = (orig - nb) + *lo;
    *(dest->hi) = *hi;
    *hi = mid;
    *(dest->lo) = mid;
    assert((dest->nb_strands() == nb) && (nb_strands() + nb == orig));
  }
  
  sched::vertex*& get_join() {
    assert(false); // impossible
    return dummy_join;
  }
  
  std::unique_ptr<children_record>& get_children() {
    return children;
  }
  
  private_activation_record*& get_destination() {
    return destination;
  }
  
};
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Dag Calculus */

namespace dc {

int loop_threshold = 1024;
  
using stmt_tag_type = enum {
  tag_stmt, tag_stmts, tag_cond, tag_exit,
  tag_sequential_loop, tag_parallel_for_loop, tag_parallel_combine_loop,
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
  using procedure_call_code_type = std::function<pcfg::stack_type(sar_type&, par_type&, pcfg::cactus::parent_link_type, pcfg::stack_type)>;
  using incounter_getter_code_type = std::function<sched::incounter**(sar_type&, par_type&)>;
  using outset_getter_code_type = std::function<sched::outset**(sar_type&, par_type&)>;
  using conds_type = std::vector<std::pair<predicate_code_type, stmt_type>>;
  using parallel_loop_range_getter_type = std::function<std::pair<int*, int*>(par_type&)>;
  using parallel_loop_combine_initializer_type = std::function<void(sar_type&, par_type&)>;
  using parallel_combining_operator_type = std::function<void(sar_type&, par_type&, par_type&)>;
  using loop_range_getter_type = std::function<std::pair<int*, int*>(sar_type&, par_type&)>;
  using leaf_loop_body_type = std::function<void(sar_type&, par_type&, int, int)>;
  
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
      parallel_loop_range_getter_type getter;
      std::unique_ptr<stmt_type> body;
    } variant_parallel_for_loop;
    struct {
      predicate_code_type predicate;
      parallel_loop_range_getter_type getter;
      parallel_loop_combine_initializer_type initialize;
      parallel_combining_operator_type combine;
      std::unique_ptr<stmt_type> body;
    } variant_parallel_combine_loop;
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
        new (&variant_parallel_for_loop.getter) parallel_loop_range_getter_type(other.variant_parallel_for_loop.getter);
        auto p = new stmt_type;
        p->copy_constructor(*other.variant_parallel_for_loop.body);
        new (&variant_parallel_for_loop.body) std::unique_ptr<stmt_type>(p);
        break;
      }
      case tag_parallel_combine_loop: {
        new (&variant_parallel_combine_loop.predicate) predicate_code_type(other.variant_parallel_combine_loop.predicate);
        new (&variant_parallel_combine_loop.getter) parallel_loop_range_getter_type(other.variant_parallel_combine_loop.getter);
        new (&variant_parallel_combine_loop.initialize) parallel_loop_combine_initializer_type(other.variant_parallel_combine_loop.initialize);
        new (&variant_parallel_combine_loop.combine) parallel_combining_operator_type(other.variant_parallel_combine_loop.combine);
        auto p = new stmt_type;
        p->copy_constructor(*other.variant_parallel_combine_loop.body);
        new (&variant_parallel_combine_loop.body) std::unique_ptr<stmt_type>(p);
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
        new (&variant_parallel_for_loop.getter) parallel_loop_range_getter_type;
        variant_parallel_for_loop.getter = std::move(other.variant_parallel_for_loop.getter);
        new (&variant_parallel_for_loop.body) std::unique_ptr<stmt_type>;
        variant_parallel_for_loop.body = std::move(other.variant_parallel_for_loop.body);
        break;
      }
      case tag_parallel_combine_loop: {
        new (&variant_parallel_combine_loop.predicate) predicate_code_type;
        new (&variant_parallel_combine_loop.getter) parallel_loop_range_getter_type;
        variant_parallel_combine_loop.getter = std::move(other.variant_parallel_combine_loop.getter);
        new (&variant_parallel_combine_loop.initialize) parallel_loop_combine_initializer_type;
        variant_parallel_combine_loop.initialize = std::move(other.variant_parallel_combine_loop.initialize);
        new (&variant_parallel_combine_loop.combine) parallel_combining_operator_type;
        variant_parallel_combine_loop.combine = std::move(other.variant_parallel_combine_loop.combine);
        new (&variant_parallel_combine_loop.body) std::unique_ptr<stmt_type>;
        variant_parallel_combine_loop.body = std::move(other.variant_parallel_combine_loop.body);
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
        variant_parallel_for_loop.getter.~parallel_loop_range_getter_type();
        using st = std::unique_ptr<stmt_type>;
        variant_parallel_for_loop.body.~st();
        break;
      }
      case tag_parallel_combine_loop: {
        variant_parallel_combine_loop.predicate.~predicate_code_type();
        variant_parallel_combine_loop.getter.~parallel_loop_range_getter_type();
        variant_parallel_combine_loop.initialize.~parallel_loop_combine_initializer_type();
        variant_parallel_combine_loop.combine.~parallel_combining_operator_type();
        using st = std::unique_ptr<stmt_type>;
        variant_parallel_combine_loop.body.~st();
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
  stmt_type parallel_for_loop(predicate_code_type predicate, parallel_loop_range_getter_type getter, stmt_type body) {
    stmt_type s;
    s.tag = tag_parallel_for_loop;
    new (&s.variant_parallel_for_loop.predicate) predicate_code_type(predicate);
    new (&s.variant_parallel_for_loop.getter) parallel_loop_range_getter_type(getter);
    new (&s.variant_parallel_for_loop.body) std::unique_ptr<stmt_type>(new stmt_type(body));
    return s;
  }
  
  static
  stmt_type parallel_combine_loop(predicate_code_type predicate,
                                  parallel_loop_range_getter_type getter,
                                  parallel_loop_combine_initializer_type initialize,
                                  parallel_combining_operator_type combine,
                                  stmt_type body) {
    stmt_type s;
    s.tag = tag_parallel_combine_loop;
    new (&s.variant_parallel_combine_loop.predicate) predicate_code_type(predicate);
    new (&s.variant_parallel_combine_loop.getter) parallel_loop_range_getter_type(getter);
    new (&s.variant_parallel_combine_loop.initialize) parallel_loop_combine_initializer_type(initialize);
    new (&s.variant_parallel_combine_loop.combine) parallel_combining_operator_type(combine);
    new (&s.variant_parallel_combine_loop.body) std::unique_ptr<stmt_type>(new stmt_type(body));
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

  using loop_direction_type = enum { forward_loop, backward_loop };

  static
  stmt_type sequential_loop(unconditional_jump_code_type initializer,
                            loop_range_getter_type getter,
                            leaf_loop_body_type body,
                            loop_direction_type direction = forward_loop) {
    return stmts({
      stmt(initializer),
      sequential_loop([=] (sar_type& s, par_type& p) {
        auto rng = getter(s, p);
        return *rng.first != *rng.second;
      }, stmt([=, &loop_threshold] (sar_type& s, par_type& p) {
        auto rng = getter(s, p);
        auto lo = *rng.first;
        auto hi = *rng.second;
        auto lo2 = 0;
        auto hi2 = 0;
        if (direction == forward_loop) {
          auto mid = std::min(lo + loop_threshold, hi);
          *rng.first = mid;
          lo2 = lo;
          hi2 = mid;
        } else { // backward_loop
          auto mid = std::max(lo, hi - loop_threshold);
          *rng.second = mid;
          lo2 = mid;
          hi2 = hi;
        }
        body(s, p, lo2, hi2);
      }))
    });
  }

  static
  stmt_type parallel_for_loop(parallel_loop_range_getter_type getter, stmt_type body) {
    return parallel_for_loop([=] (sar_type& s, par_type& p) {
      auto rng = getter(p);
      return *rng.first != *rng.second;
    }, getter, body);
  }
  
  static
  stmt_type parallel_for_loop(unconditional_jump_code_type initializer,
                              parallel_loop_range_getter_type getter,
                              leaf_loop_body_type body) {
    return stmts({
      stmt(initializer),
      parallel_for_loop(getter, stmt([=, &loop_threshold] (sar_type& s, par_type& p) {
        auto rng = getter(p);
        auto lo = *rng.first;
        auto mid = std::min(lo + loop_threshold, *rng.second);
        *rng.first = mid;
        body(s, p, lo, mid);
      }))
    });
  }

  static
  stmt_type parallel_combine_loop(parallel_loop_range_getter_type getter,
                                  parallel_loop_combine_initializer_type initialize,
                                  parallel_combining_operator_type combine,
                                  stmt_type body) {
    return parallel_combine_loop([=] (sar_type& s, par_type& p) {
      auto rng = getter(p);
      return *rng.first != *rng.second;
    }, getter, initialize, combine, body);
  }

  static
  stmt_type parallel_combine_loop(unconditional_jump_code_type initializer,
                                  parallel_loop_range_getter_type getter,
                                  parallel_loop_combine_initializer_type combine_initializer,
                                  parallel_combining_operator_type combine,
                                  leaf_loop_body_type body) {
    return stmts({
      stmt(initializer),
        parallel_combine_loop(getter, combine_initializer, combine,
                              stmt([=, &loop_threshold] (sar_type& s, par_type& p) {
        auto rng = getter(p);
        auto lo = *rng.first;
        auto mid = std::min(lo + loop_threshold, *rng.second);
        *rng.first = mid;
        body(s, p, lo, mid);
      }))
    });    
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
        auto selector = [predicates, entries] (sar& s, par& p) {
          int i = 0;
          for (auto& pred : predicates) {
            if (pred(s, p)) {
              break;
            }
            i++;
          }
          return entries[i];
        };
        add_block(entry, bbt::conditional_jump(selector));
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
        auto selector = [predicate, body_label, exit] (sar& s, par& p) {
          return predicate(s, p) ? body_label : exit;
        };
        add_block(header_label, bbt::conditional_jump(selector));
        result = transform(*stmt.variant_sequential_loop.body, body_label, header_label, loop_scope, result);
        break;
      }
      case tag_parallel_for_loop: {
        parallel_loop_id_type loop_label = new_parallel_loop_label();
        loop_scope.push_back(loop_label);
        parallel_loop_descriptor_type descriptor;
        descriptor.join = parallel_loop_descriptor_type::join_trivial;
        descriptor.entry = { .pred=entry, .succ=entry };
        descriptor.exit = { .pred=exit, .succ=exit };
        descriptor.parents = loop_scope;
        auto getter = stmt.variant_parallel_for_loop.getter;
        descriptor.initializer = [getter] (Private_activation_record& p, pcfg::parallel_loop_activation_record* _ar) {
          pcfg::parallel_for_activation_record& ar = *((pcfg::parallel_for_activation_record*)_ar);
          new (&ar) pcfg::parallel_for_activation_record;
          std::pair<int*, int*> range = getter(p);
          ar.lo = range.first;
          ar.hi = range.second;
        };
        add_parallel_loop(loop_label, descriptor);
        auto header_label = entry;
        auto body_label = new_label();
        auto footer_label = new_label();
        auto update_label = new_label();
        add_block(update_label, bbt::spawn_join([&] (sar&, par&, pcfg::cactus::parent_link_type, pcfg::stack_type st) {
          return pcfg::cactus::update_mark_stack(st, [&] (char* _ar) {
            return pcfg::is_splittable(_ar);
          });
        }, header_label));
        auto predicate = stmt.variant_parallel_for_loop.predicate;
        auto selector = [predicate, body_label, footer_label] (sar& s, par& p) {
          return predicate(s, p) ? body_label : footer_label;
        };
        add_block(header_label, bbt::conditional_jump(selector));
        auto footer_selector = [loop_label, exit] (sar&, par& p) {
          return p.get_join(loop_label) == nullptr ? exit : pcfg::exit_block_label;
        };
        add_block(footer_label, bbt::conditional_jump(footer_selector));
        result = transform(*stmt.variant_parallel_for_loop.body, body_label, update_label, loop_scope, result);
        break;
      }
      case tag_parallel_combine_loop: {
        parallel_loop_id_type loop_label = new_parallel_loop_label();
        loop_scope.push_back(loop_label);
        parallel_loop_descriptor_type descriptor;
        descriptor.join = parallel_loop_descriptor_type::join_binary_associative_combine;
        descriptor.entry = { .pred=entry, .succ=entry };
        descriptor.exit = { .pred=exit, .succ=exit };
        descriptor.parents = loop_scope;
        auto getter = stmt.variant_parallel_combine_loop.getter;
        descriptor.initializer = [getter] (Private_activation_record& p, pcfg::parallel_loop_activation_record* _ar) {
          pcfg::parallel_combine_activation_record& ar = *((pcfg::parallel_combine_activation_record*)_ar);
          new (&ar) pcfg::parallel_combine_activation_record;
          std::pair<int*, int*> range = getter(p);
          ar.lo = range.first;
          ar.hi = range.second;
        };
        add_parallel_loop(loop_label, descriptor);
        auto initialize_label = entry;
        auto header_label = new_label();
        auto body_label = new_label();
        auto children_loop_finalize_label = new_label();
        auto children_loop_header_label = new_label();
        auto children_loop_body0_label = new_label();
        auto children_loop_body1_label = new_label();
        auto parent_check_label = new_label();
        auto update_label = new_label();
        add_block(update_label, bbt::spawn_join([&] (sar&, par&, pcfg::cactus::parent_link_type, pcfg::stack_type st) {
          return pcfg::cactus::update_mark_stack(st, [&] (char* _ar) {
            return pcfg::is_splittable(_ar);
          });
        }, header_label));
        add_block(initialize_label, bbt::unconditional_jump(stmt.variant_parallel_combine_loop.initialize, header_label));
        auto predicate = stmt.variant_parallel_combine_loop.predicate;
        auto selector = [predicate, body_label, children_loop_finalize_label] (sar& s, par& p) {
          return predicate(s, p) ? body_label : children_loop_finalize_label;
        };
        add_block(header_label, bbt::conditional_jump(selector));
        add_block(children_loop_finalize_label, bbt::unconditional_jump([loop_label] (sar&, par& p) {
          auto& children = p.loop_activation_record_of(loop_label)->get_children();
          if (children) {
            children->nb = (int)children->futures.size();
          }
        }, children_loop_header_label));
        auto children_loop_header = [loop_label, parent_check_label, children_loop_body0_label] (sar&, par& p) {
          auto& children = p.loop_activation_record_of(loop_label)->get_children();
          if (children) {
            if (children->nb <= 0) {
              children.reset();
              return parent_check_label;
            }
            return children_loop_body0_label;
          } else {
            return parent_check_label;
          }
        };
        add_block(children_loop_header_label, bbt::conditional_jump(children_loop_header));
        auto children_loop_body0 = [loop_label] (sar&, par& p) {
          auto& children = p.loop_activation_record_of(loop_label)->get_children();
          assert(children);
          auto i = children->nb - 1;
          return &(children->futures.at(i).first);
        };
        add_block(children_loop_body0_label, bbt::join_minus(children_loop_body0, children_loop_body1_label));
        auto combine = stmt.variant_parallel_combine_loop.combine;
        auto children_loop_body1 = [loop_label, combine] (sar& s, par& p) {
          auto& children = p.loop_activation_record_of(loop_label)->get_children();
          assert(children);
          auto i = children->nb - 1;
          par* c = (par*)children->futures.at(i).second;
          combine(s, *c, p);
          delete c;
          children->futures.at(i).second = nullptr; // to avoid the dangling pointer
          children->nb--;
        };
        add_block(children_loop_body1_label, bbt::unconditional_jump(children_loop_body1, children_loop_header_label));
        auto parent_check = [loop_label, exit] (sar& s, par& p) {
          auto destination = (par*)p.loop_activation_record_of(loop_label)->get_destination();
          if (destination != nullptr) {
            new (destination) par(p);
            return pcfg::exit_block_label;
          }
          return exit;
        };
        add_block(parent_check_label, bbt::conditional_jump(parent_check));
        result = transform(*stmt.variant_parallel_combine_loop.body, body_label, update_label, loop_scope, result);
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

#define encore_pcfg_default_private_activation_record(edsl) \
class private_activation_record : public edsl::private_activation_record { };

#define encore_pcfg_declare(edsl, name, sar, par, bb) \
std::pair<edsl::pcfg::stack_type, int> run(edsl::pcfg::stack_type stack, int fuel) const { \
  return edsl::pcfg::step(cfg, stack, fuel); \
} \
\
void promote_mark(edsl::pcfg::interpreter* interp, edsl::pcfg::private_activation_record* p) { \
  edsl::pcfg::promote_mark(cfg, interp, this, (par*)p); \
} \
\
encore::sched::outset* get_dependency_of_join_minus(edsl::pcfg::stack_type  stack) { \
  return edsl::pcfg::get_dependency_of_join_minus(cfg, stack); \
} \
\
using cfg_type = edsl::pcfg::cfg_type<name>; \
using sar = name; \
using par = private_activation_record; \
using stt = edsl::pcfg::stack_type; \
using bb = typename cfg_type::basic_block_type; \
using plt = edsl::pcfg::cactus::parent_link_type; \
\
template <class Activation_record, class ...Args> \
static stt encore_call(stt s, plt p, Args... args) { \
  return edsl::pcfg::push_call<Activation_record>(s, p, args...); \
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

#define encore_parallel_loop_alloc_default(edsl, nb_loops) \
private_activation_record() { \
  private_activation_record::initialize_descriptors(); \
} \
\
std::array<edsl::pcfg::parallel_for_activation_record, nb_loops> _ars1; \
edsl::pcfg::parallel_loop_activation_record* _encore_loop_activation_record_of(edsl::pcfg::parallel_loop_id_type id) { \
  return &_ars1[id]; \
}

#define encore_private_activation_record_begin(edsl, name, nb_loops) \
class private_activation_record \
: public edsl::pcfg::parallel_loop_private_activation_record<name,private_activation_record> { \
public: \
  encore_parallel_loop_alloc_default(edsl, nb_loops)

#define encore_private_activation_record_end(edsl, name, sar, par, dc, get_dc) \
}; \
encore_dc_loop_declare(edsl, name, sar, par, dc, get_dc)

#endif /*! _ENCORE_EDSL_H_ */
