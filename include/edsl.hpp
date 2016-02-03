
#include <functional>
#include <vector>
#include <algorithm>

#include "vertex.hpp"

#ifndef _PASL_SCHED_EDSL_H_
#define _PASL_SCHED_EDSL_H_

namespace pasl {
namespace sched {
namespace edsl {
  
/*---------------------------------------------------------------------*/
/* Parallel procedural language */
  
namespace procedural {
  
template <class Env>
class stmt_type {
public:
  
  using tag_type = enum {
    tag_pstmt,
    tag_pblock,
    tag_pcond,
    tag_pwhile,
    tag_preturn,
    tag_pbreak,
    tag_pcontinue,
    tag_pcall,
    tag_pasync,
    tag_pfinish,
    tag_pfuture,
    tag_pforce
  };
  
  tag_type tag;
  
  // later: consider making this struct a union, as it should be
  // in order to do so, you need to replace all the std::function
  // fields with ones that have default constructors
  struct {
    
    struct {
      std::function<void(Env&)> f;
    } cons_pstmt;
    
    struct {
      std::vector<stmt_type*> stmts;
    } cons_pblock;
    
    struct {
      std::vector<std::tuple<std::function<bool(Env&)>, stmt_type*>> conds;
      stmt_type* cond;
    } cons_pcond;
    
    struct {
      stmt_type* cond_stmt;
      std::function<bool(Env&)> cond;
      stmt_type* body_stmt;
    } cons_pwhile;
    
    struct {
      
    } cons_preturn;
    
    struct {
      
    } cons_pbreak;
    
    struct {
      
    } cons_pcontinue;
    
    struct {
      std::function<vertex*(Env&)> f;
    } cons_pcall;
    
    struct {
      std::function<std::pair<vertex*, vertex*>(Env&)> f;
    } cons_pasync;
    
    struct {
      std::function<vertex*(Env&)> f;
    } cons_pfinish;
    
    struct {
      std::function<vertex*(Env&)> f;
    } cons_pfuture;
    
    struct {
      std::function<outset*(Env&)> f;
    } cons_pforce;
    
  } u;
  
  static
  stmt_type* pstmt(std::function<void(Env&)> f) {
    stmt_type* stmt = new stmt_type(tag_pstmt);
    stmt->u.cons_pstmt.f = f;
    return stmt;
  }
  
  static
  stmt_type* pblock(std::vector<stmt_type*> stmts) {
    stmt_type* stmt = new stmt_type(tag_pblock);
    stmt->u.cons_pblock.stmts = stmts;
    return stmt;
  }
  
  using branch_type = std::tuple<std::function<bool(Env&)>, stmt_type*>;
  
  static
  stmt_type* pcond(std::vector<branch_type> conds) {
    stmt_type* stmt = new stmt_type(tag_pcond);
    stmt->u.cons_pcond.conds = conds;
    return stmt;
  }
  
  static
  stmt_type* pnoop() {
    return pstmt([] (Env&) { });
  }
  
  static
  branch_type pbranch(std::function<bool(Env&)> cond, stmt_type* body) {
    return std::make_tuple(cond, body);
  }
  
  static
  branch_type pbranch(stmt_type* body) {
    return pbranch([] (Env&) { return true; }, body);
  }
  
  static
  stmt_type* pwhile(stmt_type* cond_stmt, std::function<bool(Env&)> cond, stmt_type* body_stmt) {
    stmt_type* stmt = new stmt_type(tag_pwhile);
    stmt->u.cons_pwhile.cond_stmt = cond_stmt;
    stmt->u.cons_pwhile.cond = cond;
    stmt->u.cons_pwhile.body_stmt = body_stmt;
    return stmt;
  }
  
  static
  stmt_type* preturn() {
    stmt_type* stmt = new stmt_type(tag_preturn);
    return stmt;
  }
  
  static
  stmt_type* pbreak() {
    stmt_type* stmt = new stmt_type(tag_pbreak);
    return stmt;
  }
  
  static
  stmt_type* pcontinue() {
    stmt_type* stmt = new stmt_type(tag_pcontinue);
    return stmt;
  }
  
  static
  stmt_type* pcall(std::function<vertex*(Env&)> f) {
    stmt_type* stmt = new stmt_type(tag_pcall);
    stmt->u.cons_pcall.f = f;
    return stmt;
  }
  
  static
  stmt_type* pasync(std::function<std::pair<vertex*, vertex*>(Env&)> f) {
    stmt_type* stmt = new stmt_type(tag_pasync);
    stmt->u.cons_pasync.f = f;
    return stmt;
  }
  
  static
  stmt_type* pfinish(std::function<vertex*(Env&)> f) {
    stmt_type* stmt = new stmt_type(tag_pfinish);
    stmt->u.cons_pfinish.f = f;
    return stmt;
  }
  
  static
  stmt_type* pfuture(std::function<vertex*(Env&)> f) {
    stmt_type* stmt = new stmt_type(tag_pfuture);
    stmt->u.cons_pfuture.f = f;
    return stmt;
  }
  
  static
  stmt_type* pforce(std::function<outset*(Env&)> f) {
    stmt_type* stmt = new stmt_type(tag_pforce);
    stmt->u.cons_pforce.f = f;
    return stmt;
  }
  
  stmt_type(tag_type tag)
  : tag(tag) { }
  
};
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Parallel Control-Flow Graph */
  
namespace pcfg {

class deque {
public:
  
  static constexpr int capacity_szb = 4096;
  
  class Deleter {
  public:
    void operator()(void* items) {
      free(items);
    }
  };
  
  std::unique_ptr<char[], Deleter> frames;
  int head;
  
  deque() : head(0) {
    frames.reset((char*)malloc(capacity_szb));
  }
  
  bool empty() const {
    return head == 0;
  }
  
  template <class Frame, class ...Args>
  void push_back(Args... args) {
    size_t szb = sizeof(Frame);
    Frame* destination = (Frame*)&frames[head];
    new (destination) Frame(args...);
    head += szb;
    size_t* header = (size_t*)&frames[head];
    *header = szb;
    head += sizeof(size_t);
  }
  
  template <class Frame>
  Frame& peek_back() {
    size_t header = head - sizeof(size_t);
    size_t szb = *((size_t*)&frames[header]);
    size_t start = header - szb;
    Frame* frame = (Frame*)&frames[start];
    return *frame;
  }
  
  template <class Frame>
  Frame& peek_front() {
    assert(false); // todo
    Frame* tmp = nullptr;
    return *tmp;
  }
  
  void pop_back() {
    size_t header = head - sizeof(size_t);
    size_t szb = *((size_t*)&frames[header]);
    head -= sizeof(size_t) + szb;
  }
  
  void pop_front() {
    assert(false); // todo
  }
  
};
  
using basic_block_label_type = int;

static constexpr basic_block_label_type entry_block_label = 0;
static constexpr basic_block_label_type exit_block_label = -1;

using tag = enum {
  tag_unconditional_jump, tag_conditional_jump,
  tag_fork1, tag_fork2,
  tag_demand, tag_none
};

template <class Env>
class basic_block_type {
public:
  
  basic_block_type() : t(tag_none) { }

  ~basic_block_type() {
    switch (t) {
      case tag_unconditional_jump: {
        using fct = std::function<void(Env&)>;
        variant_unconditional_jump.code.~fct();
        break;
      }
      case tag_conditional_jump: {
        using fct = std::function<int(Env&)>;
        variant_conditional_jump.code.~fct();
        using blks = std::vector<basic_block_label_type>;
        variant_conditional_jump.targets.~blks();
        break;
      }
      case tag_fork1: {
        using fct = std::function<void(Env&, deque&)>;
        variant_fork1.code.~fct();
        break;
      }
      case tag_fork2: {
        using fct = std::function<void(Env&, deque&)>;
        variant_fork2.code1.~fct();
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
        new (&variant_unconditional_jump.code) std::function<void(Env&)>(other.variant_unconditional_jump.code);
        variant_unconditional_jump.next = other.variant_unconditional_jump.next;
        break;
      }
      case tag_conditional_jump: {
        new (&variant_conditional_jump.code) std::function<int(Env&)>(other.variant_conditional_jump.code);
        new (&variant_conditional_jump.targets) std::vector<basic_block_label_type>(other.variant_conditional_jump.targets);
        break;
      }
      case tag_fork1: {
        new (&variant_fork1.code) std::function<void(Env&, deque&)>(other.variant_fork1.code);
        variant_fork1.next = other.variant_fork1.next;
        break;
      }
      case tag_fork2: {
        new (&variant_fork2.code1) std::function<void(Env&, deque&)>(other.variant_fork2.code1);
        variant_fork2.next = other.variant_fork2.next;
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
        new (&variant_unconditional_jump.code) std::function<void(Env&)>();
        variant_unconditional_jump.code = std::move(other.variant_unconditional_jump.code);
        variant_unconditional_jump.next = std::move(other.variant_unconditional_jump.next);
        break;
      }
      case tag_conditional_jump: {
        new (&variant_conditional_jump.code) std::function<int(Env&)>();
        variant_conditional_jump.code = std::move(other.variant_conditional_jump.code);
        new (&variant_conditional_jump.targets) std::vector<basic_block_label_type>();
        variant_conditional_jump.targets = std::move(other.variant_conditional_jump.targets);
        break;
      }
      case tag_fork1: {
        new (&variant_fork1.code) std::function<void(Env&, deque&)>();
        variant_fork1.code = std::move(other.variant_fork1.code);
        variant_fork1.next = std::move(other.variant_fork1.next);
        break;
      }
      case tag_fork2: {
        new (&variant_fork2.code1) std::function<void(Env&, deque&)>();
        variant_fork2.code1 = std::move(other.variant_fork2.code1);
        variant_fork2.next = std::move(other.variant_fork2.next);
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
      std::function<void(Env&)> code;
      basic_block_label_type next;
    } variant_unconditional_jump;
    struct {
      std::function<int(Env&)> code;
      std::vector<basic_block_label_type> targets;
    } variant_conditional_jump;
    struct {
      std::function<void(Env&, deque&)> code;
      basic_block_label_type next;
    } variant_fork1;
    struct {
      std::function<void(Env&, deque&)> code1;
      basic_block_label_type next; // must be the label of a fork1 block
    } variant_fork2;
    struct {
      basic_block_label_type next;
    } variant_demand;
  };
  

  static
  basic_block_type unconditional_jump(std::function<void(Env&)> code,
                                      basic_block_label_type next) {
    basic_block_type b;
    b.t = tag_unconditional_jump;
    new (&b.variant_unconditional_jump.code) std::function<void(Env&)>(code);
    b.variant_unconditional_jump.next = next;
    return b;
  }
  
  static
  basic_block_type conditional_jump(std::function<int(Env&)> code,
                                    std::vector<basic_block_label_type> targets) {
    basic_block_type b;
    b.t = tag_conditional_jump;
    new (&b.variant_conditional_jump.code) std::function<int(Env&)>(code);
    b.variant_conditional_jump.targets = targets;
    return b;
  }
  
  static
  basic_block_type fork1(std::function<void(Env&, deque&)> code,
                         basic_block_label_type next) {
    basic_block_type b;
    b.t = tag_fork1;
    new (&b.variant_fork1.code) std::function<void(Env&, deque&)>(code);
    b.variant_fork1.next = next;
    return b;
  }
  
  static
  basic_block_type fork2(std::function<void(Env&, deque&)> code1,
                         basic_block_label_type next) {
    basic_block_type b;
    b.t = tag_fork2;
    new (&b.variant_fork2.code1) std::function<void(Env&, deque&)>(code1);
    b.variant_fork2.next = next;
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
  
template <class Env>
using cfg_type = std::vector<basic_block_type<Env>>;
  
class activation_record {
public:
  
  basic_block_label_type trampoline;
  
  virtual void run(deque&) = 0;
  
};
  
template <class Env>
void step(cfg_type<Env>& cfg, deque& dq) {
  assert(! dq.empty());
  Env& newest = dq.peek_back<Env>();
  basic_block_type<Env>& block = cfg.at(newest.trampoline);
  switch (block.t) {
    case tag_unconditional_jump: {
      block.variant_unconditional_jump.code(newest);
      newest.trampoline = block.variant_unconditional_jump.next;
      break;
    }
    case tag_conditional_jump: {
      int target = block.variant_conditional_jump.code(newest);
      newest.trampoline = block.variant_conditional_jump.targets[target];
      break;
    }
    case tag_fork1: {
      block.variant_fork1.code(newest, dq);
      newest.trampoline = block.variant_fork1.next;
      break;
    }
    case tag_fork2: {
      block.variant_fork2.code1(newest, dq);
      newest.trampoline = block.variant_fork2.next;
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
  if (newest.trampoline == exit_block_label) {
    dq.pop_back();
  }
}
  
class interpreter : public vertex {
public:
  
  std::unique_ptr<deque> dg;
  
  deque& get_deque() {
    deque* dq = dg.get();
    if (dq == nullptr) {
      dq = new deque;
      dg.reset(dq);
    }
    return *dq;
  }
  
  int seq(int fuel) {
    deque& dq = get_deque();
    while (fuel > 0 && ! dq.empty()) {
      activation_record& ar = dq.peek_back<activation_record>();
      ar.run(dq);
      fuel--;
    }
    if (! dq.empty()) {
      assert(fuel == 0);
      // later: promote the oldest frame
    }
    return fuel;
  }
  
  int nb_strands() {
    return 1;
  }
  
  int run(int fuel) {
    return fuel - 1;
  }
  
  vertex* split(int nb) {
    return nullptr;
  }
  
};
  
} // end namespace
  
} // end namespace
} // end namespace
} // end namespace

#endif /*! _PASL_SCHED_EDSL_H_ */