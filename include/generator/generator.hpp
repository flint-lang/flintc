#ifndef __GENERATOR_HPP__
#define __GENERATOR_HPP__

#include "../parser/ast/file_node.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>

#include <string>

/// Class which is responsible for the IR code generation
namespace Generator {
    std::unique_ptr<llvm::Module> generate_program_ir(const std::string &program_name);
    std::unique_ptr<llvm::Module> generate_file_ir(const FileNode &file, const std::string &file_name, llvm::LLVMContext *context,
        llvm::IRBuilder<> *builder);

    std::string get_module_ir_string(const llvm::Module *module);
}; // namespace Generator

#endif
