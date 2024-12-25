#include "generator/generator.hpp"

#include "debug.hpp"
#include "error/error.hpp"
#include "resolver/resolver.hpp"

// #include <llvm/IR/DerivedTypes.h>
// #include <llvm/IRReader/IRReader.h>
// #include <llvm/Support/SourceMgr.h>
// #include <llvm/Support/TargetSelect.h>
// #include <llvm/Support/raw_ostream.h>
// #include <llvm/Target/TargetMachine.h>
// #include <llvm/Target/TargetOptions.h>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>    // A basic function like in c
#include <llvm/IR/IRBuilder.h>   // Utility to generate instructions
#include <llvm/IR/LLVMContext.h> // Manages types and global states
#include <llvm/IR/Module.h>      // Container for the IR code
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/Error.h>

#include <llvm/Transforms/Utils/Cloning.h>

#include <llvm/Support/raw_ostream.h>

#include <memory>
#include <string>

/// generate_program_ir
///     Generates the llvm IR code for a complete program
std::unique_ptr<llvm::Module> Generator::generate_program_ir(const std::string &program_name, llvm::LLVMContext *context) {
    auto builder = std::make_unique<llvm::IRBuilder<>>(*context);
    auto module = std::make_unique<llvm::Module>(program_name, *context);

    llvm::Linker linker(*module);

    for (const auto &file : Resolver::get_file_map()) {
        // Generate the IR for a single file
        std::unique_ptr<llvm::Module> file_module = generate_file_ir(file.second, file.first, context, builder.get());

        // TODO DONE:   This results in a segmentation fault when the context goes out of scope / memory, because then the module will have
        //              dangling references to the context. All modules in the Resolver must be cleared and deleted before the context goes
        //              oom! Its the reason to why Resolver::clear() was implemented!
        // Store the generated module in the resolver
        std::unique_ptr<const llvm::Module> file_module_copy = llvm::CloneModule(*file_module);
        Resolver::add_ir(file.first, file_module_copy);

        if (linker.linkInModule(std::move(file_module))) {
            throw_err(ERR_LINKING);
        }
    }

    return module;
}

/// generate_file_ir
///     Generates the llvm IR code from a given AST FileNode for a given file
std::unique_ptr<llvm::Module> Generator::generate_file_ir(const FileNode &file, const std::string &file_name, llvm::LLVMContext *context,
    llvm::IRBuilder<> *builder) {
    Debug::AST::print_file(file);

    std::unique_ptr<llvm::Module> module = std::make_unique<llvm::Module>(file_name, *context);

    // // Create the main function signature
    llvm::FunctionType *mainFuncType = llvm::FunctionType::get( //
        llvm::Type::getInt32Ty(*context),                       // Return type: int
        false                                                   // No arguments
    );

    // Create the main function itself
    llvm::Function *mainFunc = llvm::Function::Create( //
        mainFuncType,                                  //
        llvm::Function::ExternalLinkage,               //
        "main",                                        //
        module.get()                                   //
    );

    // Create the entry block for main function
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(*context, "entry", mainFunc);
    builder->SetInsertPoint(entry);

    // Create your own printf. As llvm does not have internal IO, the printf function from the c std lib will be used instead
    llvm::FunctionType *printfType = llvm::FunctionType::get(          //
        llvm::Type::getInt32Ty(*context),                              // Return type: int
        llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context)), // Takes char*
        true                                                           // Variadic arguments
    );
    llvm::Function *printfFunc = llvm::Function::Create( //
        printfType,                                      //
        llvm::Function::ExternalLinkage,                 //
        "printf",                                        //
        module.get()                                     //
    );

    // Create the "Hello, World!\n" string
    llvm::Value *helloWorld = builder->CreateGlobalStringPtr("Hello, World!\n");

    // Generate the prinf call
    builder->CreateCall(printfFunc, helloWorld);

    // Add return statement
    builder->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0));

    // Verify and emit the module
    llvm::verifyModule(*module, &llvm::errs());
    // module->print(llvm::outs(), nullptr);
    return module;
}

/// get_module_ir_string
///     Returns the module string
std::string Generator::get_module_ir_string(const llvm::Module *module) {
    std::string ir_string;
    llvm::raw_string_ostream stream(ir_string);
    module->print(stream, nullptr);
    stream.flush();
    return ir_string;
}
