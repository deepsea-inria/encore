#include <iostream>
#include "edsl.hpp"

namespace dsl = pasl::sched::edsl::pcfg;

void* the_cfg;

class fib : public dsl::activation_record {
public:
  
  int n; int* dp;
  int d1; int d2;
  
  fib() { }
  fib(int n, int* dp) : n(n), dp(dp) { }
  
  using env = fib;
  using cfg_type = dsl::cfg_type<env>;
  using bty = dsl::basic_block_type<env>;
  
  static
  cfg_type get_cfg() {
    cfg_type cfg;
    // 0
    cfg.push_back(bty::conditional_jump([] (env& e) {
      if (e.n <= 1) {
        *(e.dp) = e.n;
        return 0;
      }
      return 1;
    }, { dsl::exit_block_label, 1 }));
    // 1
    cfg.push_back(bty::fork2([&] (env& e, dsl::deque& dq) {
      dq.push_back<fib>(e.n - 1, &e.d1);
    }, 2));
    // 2
    cfg.push_back(bty::fork1([] (env& e, dsl::deque& dq) {
      dq.push_back<fib>(e.n - 2, &e.d2);
    }, 3));
    // 3
    cfg.push_back(bty::unconditional_jump([&] (env& e) {
      *(e.dp) = e.d1 + e.d2;
    }, -1));
    return cfg;
  }
  
  void run(dsl::deque& dq) {
    dsl::step(*((cfg_type*)the_cfg), dq);
  }
  
};

//fib::cfg_type fib::cfg = fib::get_cfg();

int main(int argc, const char * argv[]) {
  the_cfg = new fib::cfg_type(fib::get_cfg());
  int result = -1;
  dsl::interpreter interp;
  dsl::deque& dq = interp.get_deque();
  dq.push_back<fib>(1, &result);
  interp.seq(1000);
  std::cout << "result = " << result << std::endl;
  return 0;
}
