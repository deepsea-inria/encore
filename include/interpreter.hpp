
#include <functional>
#include <vector>
#include <map>
#include <algorithm>
#include <memory>
#include <array>

#include "vertex.hpp"
#include "cactus-plus.hpp"
#include "stats.hpp"
#include "scheduler.hpp"
#include "pcfg.hpp"

#ifndef _ENCORE_INTERPRETER_H_
#define _ENCORE_INTERPRETER_H_

namespace encore {
namespace edsl {
namespace pcfg {

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
  
bool never_promote = false;
  
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
    stack = cactus::update_mark_stack(s, [&] (char* _ar) {
      return pcfg::is_splittable(_ar);
    });
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
    if (never_promote) {
      schedule(this);
      return sched::promotion_threshold;
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
      sched::new_edge(branch2, join);
      sched::new_edge(branch1, join);
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
      sched::new_edge(branch, incounter);
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
      sched::new_edge(branch, continuation);
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
} // end namespace
} // end namespace

#endif /*! _ENCORE_INTERPRETER_H_ */
