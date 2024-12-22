#include "generator/generator.hpp"

#include "debug.hpp"

#include <cassert>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>    // A basic function like in c
#include <llvm/IR/IRBuilder.h>   // Utility to generate instructions
#include <llvm/IR/LLVMContext.h> // Manages types and global states
#include <llvm/IR/Module.h>      // Container for the IR code
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

std::unique_ptr<llvm::LLVMContext> Generator::context;
std::unique_ptr<llvm::Module> Generator::module;
std::unique_ptr<llvm::IRBuilder<>> Generator::builder;

/// generate_ir
///     Generates the llvm IR code from a given AST ProgramNode
void Generator::generate_ir(const FileNode &file) {
    Debug::AST::print_file(file);

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
    module->print(llvm::outs(), nullptr);
}
