
#ifndef _ENCORE_CONTEXT_H_
#define _ENCORE_CONTEXT_H_

using register_set_type = struct {
  uint64_t rsp;
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t rbx;
  uint64_t rbp;
};

extern "C" {
void encore_context_save(register_set_type*);
void encore_context_switch(register_set_type*);
void encore_context_swap(register_set_type*, register_set_type*);
}

namespace encore {
  
static
void impossible() {
  std::cerr << "encore::context: impossible" << std::endl;
}

class context_type {
public:
  
  register_set_type registers;
  
  bool status = false;
  
  inline
  bool capture() {
    status = true;
    encore_context_save(&registers);
    return status;
  }
  
  inline
  void switch_to() {
    status = false;
    encore_context_switch(&registers);
  }
  
  inline
  void swap(context_type& target) {
    target.status = true;
    encore_context_swap(&registers, &target.registers);
  }
  
  inline
  void create(char* stack, int stack_szb, void* f) {
    *(uint64_t *)&stack[stack_szb -  8] = (uint64_t)impossible;
    *(uint64_t *)&stack[stack_szb - 16] = (uint64_t)f;
    registers.rsp = (uint64_t)&stack[stack_szb - 16];
  }
  
};
  
} // end namespace

#endif /*! _ENCORE_CONTEXT_H_ */