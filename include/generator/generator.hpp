#ifndef __GENERATOR_HPP__
#define __GENERATOR_HPP__

#include "../parser/ast/program_node.hpp"

#include <llvm/IR/LLVMContext.h>

/// Class which is responsible for the IR code generation
class Generator {
  public:
    Generator() = delete;

    static void generate_ir(const ProgramNode &program);

  private:
};

#endif
