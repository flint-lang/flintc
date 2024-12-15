#include "generator/generator.hpp"

#include "debug.hpp"

/// generate_ir
///     Generates the llvm IR code from a given AST ProgramNode
void Generator::generate_ir(const ProgramNode &program) {
    Debug::AST::print_program(program);
}
