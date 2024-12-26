#ifndef __GENERATOR_HPP__
#define __GENERATOR_HPP__

#include "../parser/ast/file_node.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>

#include <string>

/// Class which is responsible for the IR code generation
class Generator {
  public:
    Generator() = delete;
    static std::unique_ptr<llvm::Module> generate_program_ir(const std::string &program_name, llvm::LLVMContext *context);

  private:
    static std::unique_ptr<llvm::Module> generate_file_ir(const FileNode &file, const std::string &file_name, llvm::LLVMContext *context,
        llvm::IRBuilder<> *builder);

    static std::string get_module_ir_string(const llvm::Module *module);
};

#endif
