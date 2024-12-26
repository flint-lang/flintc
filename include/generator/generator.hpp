#ifndef __GENERATOR_HPP__
#define __GENERATOR_HPP__

#include "../parser/ast/file_node.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "types.hpp"

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

    static std::unique_ptr<llvm::Module> generate_file_ir(const FileNode &file, const std::string &file_name, llvm::LLVMContext *context,
        llvm::IRBuilder<> *builder);

    static std::string get_module_ir_string(const llvm::Module *module);

  private:
    static llvm::Function *generate_builtin_main(llvm::Module *module);
    static llvm::Function *generate_builtin_print(llvm::Module *module);
    static llvm::Function *generate_function(llvm::Module *module, FunctionNode *function_node);
    static void generate_body(llvm::Module *module, llvm::Function *function, std::vector<body_statement> &body);
    static void generate_statement(llvm::IRBuilder<> &builder, const body_statement &statement);

    static llvm::Type *get_type_from_str(llvm::Module *module, const std::string &str);
};

#endif
