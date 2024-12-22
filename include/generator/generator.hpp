#ifndef __GENERATOR_HPP__
#define __GENERATOR_HPP__

#include "../parser/ast/file_node.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <string>

/// Class which is responsible for the IR code generation
class Generator {
  public:
    Generator() = delete;

    /// init
    ///     Initializes the generator (Creates the context, module ang generator pointers)
    static void init(const std::string &name) {
        // Assert that the pointer are uninitialized
        assert(!context);
        assert(!module);
        assert(!builder);

        context = std::make_unique<llvm::LLVMContext>();
        module = std::make_unique<llvm::Module>(name, *context);
        builder = std::make_unique<llvm::IRBuilder<>>(*context);
    }

    static void generate_ir(const FileNode &file);

  private:
    static std::unique_ptr<llvm::LLVMContext> context;
    static std::unique_ptr<llvm::Module> module;
    static std::unique_ptr<llvm::IRBuilder<>> builder;
};

#endif
