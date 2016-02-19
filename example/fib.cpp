#include <iostream>
#include "scheduler.hpp"
#include "edsl.hpp"

namespace dsl = pasl::edsl::pcfg;

int fib(int n) {
  if (n <= 1) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}

class fib_manual : public pasl::sched::vertex {
public:
  
  enum { entry, join, exit };
  int trampoline = entry;
  
  void yield_with(int target) {
    trampoline = target;
  }
  
  int n; int* dp;
  int d1, d2;
  
  fib_manual(int n, int* dp)
  : vertex(), n(n), dp(dp) { }
  
  int nb_strands() {
    if (trampoline == exit) {
      return 0;
    }
    return 1;
  }
  
  int run(int fuel) {
    switch (trampoline) {
      case entry: {
        if (n <= 1) {
          *dp = n;
          yield_with(exit);
          break;
        }
        fib_manual* b1 = new fib_manual(n - 1, &d1);
        fib_manual* b2 = new fib_manual(n - 2, &d2);
        yield_with(join);
        pasl::sched::new_edge(b2, this);
        pasl::sched::new_edge(b1, this);
        pasl::sched::release(b2);
        pasl::sched::release(b1);
        break;
      }
      case join: {
        *dp = d1 + d2;
        yield_with(exit);
        break;
      }
      default:
        assert(false);
    }
    return fuel - 1;
  }
  
  vertex* split(int nb) {
    assert(false); // impossible
  }
  
};

class fib_cfg : public dsl::activation_record {
public:
  
  int n; int* dp;
  int d1; int d2;
  
  fib_cfg() { }
  
  fib_cfg(int n, int* dp)
  : n(n), dp(dp) { }
  
  using ar = fib_cfg;
  using cfg_type = dsl::cfg_type<ar>;
  using bb = dsl::basic_block_type<ar>;
  
  static
  cfg_type get_cfg() {
    cfg_type cfg;
    // 0
    cfg.push_back(bb::conditional_jump([] (ar& a) {
      if (a.n <= 1) {
        *a.dp = a.n;
        return 0;
      }
      return 1;
    }, { dsl::exit_block_label, 1 }));
    // 1
    cfg.push_back(bb::fork2([&] (ar& a, dsl::stack_type st) {
      return dsl::procedure_call<ar>(st, a.n - 1, &a.d1);
    }, 2));
    // 2
    cfg.push_back(bb::fork1([] (ar& a, dsl::stack_type st) {
      return dsl::procedure_call<ar>(st, a.n - 2, &a.d2);
    }, 3));
    // 3
    cfg.push_back(bb::unconditional_jump([&] (ar& a) {
      *a.dp = a.d1 + a.d2;
    }, -1));
    return cfg;
  }
  
  static
  cfg_type cfg;
  
  dsl::stack_type run(dsl::stack_type stack) {
    return dsl::step(cfg, stack);
  }
  
  void promote(dsl::interpreter* interp) {
     dsl::promote(cfg, interp);
  }
  
};

fib_cfg::cfg_type fib_cfg::cfg = fib_cfg::get_cfg();

namespace pasl {
namespace cactus {
  
class test_frame {
public:
  static constexpr int stack_szb = pasl::cactus::K - sizeof(pasl::cactus::descriptor_type);
  static constexpr int target_nb_frames = 3;
  static constexpr int target_frame_szb = stack_szb / target_nb_frames;
  static constexpr int nb_ints = target_frame_szb / sizeof(int);
  
  char f[target_frame_szb];
  test_frame() {
    int* ints = (int*)f;
    for (int i = 0; i < nb_ints; i++) {
      ints[i] = 0xdeadbeef;
    }
  }
  void check() {
    int* ints = (int*)f;
    for (int i = 0; i < nb_ints; i++) {
      assert(ints[i] == 0xdeadbeef);
    }
  }
};

void test_stack1() {
  stack_type stack = new_stack();
  chunk_type* c = (chunk_type*)stack.first;
  char* n = stack.last + sizeof(test_frame) + 4;
  chunk_type* c2 = chunk_of(n);
  assert(c2 == c);

  assert(empty(stack));
  stack = push_back<test_frame>(stack);
  peek_back<test_frame>(stack).check();
  stack = push_back<test_frame>(stack);
  peek_back<test_frame>(stack).check();
  
  stack = push_back<test_frame>(stack);
  peek_back<test_frame>(stack).check();
  stack = push_back<test_frame>(stack);
  peek_back<test_frame>(stack).check();
  
  auto stacks = cactus::split_front<test_frame>(stack);
  stack_type stack1 = stacks.first;
  stack_type stack2 = stacks.second;
  
  stack1 = push_back<test_frame>(stack1);
  peek_back<test_frame>(stack1).check();
  stack1 = push_back<test_frame>(stack1);
  peek_back<test_frame>(stack1).check();
  
  stack1 = pop_back<test_frame>(stack1);
  peek_back<test_frame>(stack1).check();
  stack1 = pop_back<test_frame>(stack1);
  peek_back<test_frame>(stack1).check();
  
  assert(! empty(stack1));
  stack1 = pop_back<test_frame>(stack1);
  assert(empty(stack1));
  
  stack2 = pop_back<test_frame>(stack2);
  peek_back<test_frame>(stack2).check();
  stack2 = pop_back<test_frame>(stack2);
  peek_back<test_frame>(stack2).check();
  
  stack2 = pop_back<test_frame>(stack2);
  assert(empty(stack2));
  
  delete_stack(stack2);
  delete_stack(stack1);
}

}
}

void test_fib_cfg() {
  int n = 15;
  int d = -1;
  dsl::interpreter* interp = new dsl::interpreter;
  interp->stack = dsl::procedure_call<fib_cfg>(interp->stack, n, &d);
  pasl::sched::release(interp);
  pasl::sched::uniprocessor::scheduler_loop();
  int m = fib(n);
  assert(d == m);
}

/*
void test2() {
  int n = 10;
  int d = -1;
  pasl::sched::release(new fib_manual(n, &d));
  pasl::sched::uniprocessor::scheduler_loop();
  assert(d == fib(n));
}
*/

int main(int argc, const char * argv[]) {
  //pasl::cactus::test_stack1();
  test_fib_cfg();
  return 0;
}
