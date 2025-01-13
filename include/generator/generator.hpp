#ifndef __GENERATOR_HPP__
#define __GENERATOR_HPP__

#include "../parser/ast/file_node.hpp"
#include "lexer/lexer.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/ast/expressions/binary_op_node.hpp"
#include "parser/ast/expressions/call_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/expressions/literal_node.hpp"
#include "parser/ast/expressions/unary_op_node.hpp"
#include "parser/ast/expressions/variable_node.hpp"
#include "parser/ast/scope.hpp"
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
#include <unordered_map>
#include <utility>
#include <vector>

/// Class which is responsible for the IR code generation
class Generator {
  public:
    Generator() = delete;
    static std::unique_ptr<llvm::Module> generate_program_ir(const std::string &program_name, llvm::LLVMContext &context);

    static std::unique_ptr<llvm::Module> generate_file_ir(const FileNode &file, const std::string &file_name, llvm::LLVMContext &context,
        llvm::IRBuilder<> *builder);

    static std::string get_module_ir_string(const llvm::Module *module);
    static std::string resolve_ir_comments(const std::string &ir_string);

  private:
    static inline std::unordered_map<BuiltinFunctions, llvm::Function *> builtins = {
        {BuiltinFunctions::PRINT, nullptr},
        {BuiltinFunctions::PRINT_ERR, nullptr},
        {BuiltinFunctions::ASSERT, nullptr},
        {BuiltinFunctions::ASSERT_ARG, nullptr},
        {BuiltinFunctions::RUN_ON_ALL, nullptr},
        {BuiltinFunctions::MAP_ON_ALL, nullptr},
        {BuiltinFunctions::FILTER_ON_ALL, nullptr},
        {BuiltinFunctions::REDUCE_ON_ALL, nullptr},
        {BuiltinFunctions::REDUCE_ON_PAIRS, nullptr},
        {BuiltinFunctions::PARTITION_ON_ALL, nullptr},
        {BuiltinFunctions::SPLIT_ON_ALL, nullptr},
    };
    static inline std::unordered_map<std::string, llvm::Function *> print_functions = {
        {"int", nullptr},
        {"flint", nullptr},
        {"char", nullptr},
        {"str", nullptr},
        {"bool", nullptr},
        {"byte", nullptr},
    };
    static std::unordered_map<std::string, llvm::StructType *> type_map;
    static llvm::StructType *add_and_or_get_type(llvm::LLVMContext *context, const FunctionNode *function_node);

    static std::unordered_map<std::string, std::vector<llvm::CallInst *>> unresolved_functions;
    static std::unordered_map<std::string, unsigned int> function_mangle_ids;

    static void generate_builtin_print(llvm::IRBuilder<> *builder, llvm::Module *module);
    static void generate_builtin_print_int(llvm::IRBuilder<> *builder, llvm::Module *module);
    static void generate_builtin_print_flint(llvm::IRBuilder<> *builder, llvm::Module *module);
    static void generate_builtin_print_char(llvm::IRBuilder<> *builder, llvm::Module *module);
    static void generate_builtin_print_str(llvm::IRBuilder<> *builder, llvm::Module *module);
    static void generate_builtin_print_bool(llvm::IRBuilder<> *builder, llvm::Module *module);
    static void generate_builtin_print_byte(llvm::IRBuilder<> *builder, llvm::Module *module);

    static llvm::Value *generate_pow_instruction(llvm::IRBuilder<> &builder, llvm::Function *parent, llvm::Value *lhs, llvm::Value *rhs);

    static bool function_has_return(llvm::Function *function);
    static void generate_phi_calls(                                                                            //
        llvm::IRBuilder<> &builder,                                                                            //
        std::unordered_map<std::string, std::vector<std::pair<llvm::BasicBlock *, llvm::Value *>>> &phi_lookup //
    );
    static void generate_allocation(                                           //
        llvm::IRBuilder<> &builder,                                            //
        Scope *scope,                                                          //
        std::unordered_map<std::string, llvm::AllocaInst *const> &allocations, //
        const std::string &alloca_name,                                        //
        llvm::Type *type,                                                      //
        const std::string &ir_name,                                            //
        const std::string &ir_comment                                          //
    );
    static void generate_allocations(                                         //
        llvm::IRBuilder<> &builder,                                           //
        llvm::Function *parent,                                               //
        Scope *scope,                                                         //
        std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
    );
    static llvm::Type *get_type_from_str(llvm::LLVMContext &context, const std::string &str);

    static llvm::FunctionType *generate_function_type(llvm::LLVMContext &context, FunctionNode *function_node);
    static llvm::Function *generate_function(llvm::Module *module, FunctionNode *function_node);
    static void generate_body(                                                                                  //
        llvm::IRBuilder<> &builder,                                                                             //
        llvm::Function *parent,                                                                                 //
        Scope *scope,                                                                                           //
        std::unordered_map<std::string, std::vector<std::pair<llvm::BasicBlock *, llvm::Value *>>> &phi_lookup, //
        std::unordered_map<std::string, llvm::AllocaInst *const> &allocations                                   //
    );

    static void generate_statement(                                                                             //
        llvm::IRBuilder<> &builder,                                                                             //
        llvm::Function *parent,                                                                                 //
        Scope *scope,                                                                                           //
        const body_statement &statement,                                                                        //
        std::unordered_map<std::string, std::vector<std::pair<llvm::BasicBlock *, llvm::Value *>>> &phi_lookup, //
        std::unordered_map<std::string, llvm::AllocaInst *const> &allocations                                   //
    );
    static void generate_return_statement(                                    //
        llvm::IRBuilder<> &builder,                                           //
        llvm::Function *parent,                                               //
        Scope *scope,                                                         //
        const ReturnNode *return_node,                                        //
        std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
    );
    static void generate_if_statement(                                                                          //
        llvm::IRBuilder<> &builder,                                                                             //
        llvm::Function *parent,                                                                                 //
        const IfNode *if_node,                                                                                  //
        unsigned int nesting_level,                                                                             //
        const std::vector<llvm::BasicBlock *> &blocks,                                                          //
        std::unordered_map<std::string, std::vector<std::pair<llvm::BasicBlock *, llvm::Value *>>> &phi_lookup, //
        std::unordered_map<std::string, llvm::AllocaInst *const> &allocations                                   //
    );
    static void generate_while_loop(                                          //
        llvm::IRBuilder<> &builder,                                           //
        llvm::Function *parent,                                               //
        const WhileNode *while_node,                                          //
        std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
    );
    static void generate_for_loop(llvm::IRBuilder<> &builder, llvm::Function *parent, const ForLoopNode *for_node);
    static void generate_assignment(                                                                            //
        llvm::IRBuilder<> &builder,                                                                             //
        llvm::Function *parent,                                                                                 //
        Scope *scope,                                                                                           //
        const AssignmentNode *assignment_node,                                                                  //
        std::unordered_map<std::string, std::vector<std::pair<llvm::BasicBlock *, llvm::Value *>>> &phi_lookup, //
        std::unordered_map<std::string, llvm::AllocaInst *const> &allocations                                   //
    );
    static void generate_declaration(                                         //
        llvm::IRBuilder<> &builder,                                           //
        llvm::Function *parent,                                               //
        Scope *scope,                                                         //
        const DeclarationNode *declaration_node,                              //
        std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
    );

    static llvm::Value *generate_expression(                                  //
        llvm::IRBuilder<> &builder,                                           //
        llvm::Function *parent,                                               //
        Scope *scope,                                                         //
        const ExpressionNode *expression_node,                                //
        std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
    );
    static llvm::Value *generate_variable(                                    //
        llvm::IRBuilder<> &builder,                                           //
        llvm::Function *parent,                                               //
        Scope *scope,                                                         //
        const VariableNode *variable_node,                                    //
        std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
    );
    static llvm::Value *generate_unary_op(llvm::IRBuilder<> &builder, llvm::Function *parent, const UnaryOpNode *unary_op_node);
    static llvm::Value *generate_literal(llvm::IRBuilder<> &builder, llvm::Function *parent, const LiteralNode *literal_node);
    static llvm::Value *generate_call(                                        //
        llvm::IRBuilder<> &builder,                                           //
        llvm::Function *parent,                                               //
        Scope *scope,                                                         //
        const CallNode *call_node,                                            //
        std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
    );
    static llvm::Value *generate_binary_op(                                   //
        llvm::IRBuilder<> &builder,                                           //
        llvm::Function *parent,                                               //
        Scope *scope,                                                         //
        const BinaryOpNode *bin_op_node,                                      //
        std::unordered_map<std::string, llvm::AllocaInst *const> &allocations //
    );
};

#endif
