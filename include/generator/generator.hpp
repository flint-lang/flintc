#ifndef __GENERATOR_HPP__
#define __GENERATOR_HPP__

#include "../parser/ast/file_node.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/ast/expressions/call_node.hpp"
#include "parser/ast/statements/assignment_node.hpp"
#include "parser/ast/statements/declaration_node.hpp"
#include "parser/ast/statements/for_loop_node.hpp"
#include "parser/ast/statements/if_node.hpp"
#include "parser/ast/statements/return_node.hpp"
#include "parser/ast/statements/while_node.hpp"
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
    static void generate_return_statement(llvm::IRBuilder<> &builder, const ReturnNode *return_node);
    static void generate_if_statement(llvm::IRBuilder<> &builder, const IfNode *if_node);
    static void generate_while_loop(llvm::IRBuilder<> &builder, const WhileNode *while_node);
    static void generate_for_loop(llvm::IRBuilder<> &builder, const ForLoopNode *for_node);
    static void generate_assignment(llvm::IRBuilder<> &builder, const AssignmentNode *assignment_node);
    static void generate_declaration(llvm::IRBuilder<> &builder, const DeclarationNode *declaration_node);

    static void generate_call(llvm::IRBuilder<> &builder, const CallNode *call_node);

    static llvm::Type *get_type_from_str(llvm::Module *module, const std::string &str);
};

#endif
