#include "error/error.hpp"
#include "generator/generator.hpp"

llvm::FunctionType *Generator::Function::generate_function_type(FunctionNode *function_node) {
    llvm::Type *return_types = nullptr;
    if (function_node->return_types.size() == 1) {
        return_types = IR::add_and_or_get_type(function_node->return_types.front());
    } else {
        std::shared_ptr<Type> group_type = std::make_shared<GroupType>(function_node->return_types);
        if (!Type::add_type(group_type)) {
            group_type = Type::get_type_from_str(group_type->to_string()).value();
        }
        return_types = IR::add_and_or_get_type(group_type);
    }

    // Get the parameter types
    std::vector<llvm::Type *> param_types_vec;
    param_types_vec.reserve(function_node->parameters.size());
    for (const auto &param : function_node->parameters) {
        auto param_type = IR::get_type(std::get<0>(param));
        if (param_type.second) {
            // Complex type, passed by reference
            param_types_vec.emplace_back(param_type.first->getPointerTo());
        } else {
            // Primitive type, passed by copy
            param_types_vec.emplace_back(param_type.first);
        }
    }
    llvm::ArrayRef<llvm::Type *> param_types(param_types_vec);

    // Complete the functions definition
    llvm::FunctionType *function_type = llvm::FunctionType::get( //
        return_types,                                            //
        param_types,                                             //
        false                                                    //
    );
    return function_type;
}

bool Generator::Function::generate_function(                                        //
    llvm::Module *module,                                                           //
    FunctionNode *function_node,                                                    //
    const std::unordered_map<std::string, ImportNode *const> &imported_core_modules //
) {
    llvm::FunctionType *function_type = generate_function_type(function_node);

    // Creating the function itself
    llvm::Function *function = llvm::Function::Create( //
        function_type,                                 //
        llvm::Function::ExternalLinkage,               //
        function_node->name,                           //
        module                                         //
    );

    // Assign names to function arguments and add them to the function's body
    size_t paramIndex = 0;
    for (auto &arg : function->args()) {
        arg.setName(std::get<1>(function_node->parameters.at(paramIndex)));
        ++paramIndex;
    }

    // Create the functions entry block
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create( //
        context,                                              //
        "entry",                                              //
        function                                              //
    );
    llvm::IRBuilder<> builder(entry_block);

    // Create all the functions allocations (declarations, etc.) at the beginning, before the actual function body
    // The key is a combination of the scope id and the variable name, e.g. 1::var1, 2::var2
    std::unordered_map<std::string, llvm::Value *const> allocations;
    Allocation::generate_function_allocations(builder, function, allocations, function_node);
    if (!Allocation::generate_allocations(builder, function, function_node->scope.get(), allocations, imported_core_modules)) {
        return false;
    }

    // Generate all instructions of the functions body
    GenerationContext ctx{function, function_node->scope.get(), allocations, imported_core_modules};
    if (!Statement::generate_body(builder, ctx)) {
        return false;
    }

    // Check if the function has a terminator, if not add an "empty" return (only the error return)
    if (!function->empty() && function->back().getTerminator() == nullptr) {
        Expression::garbage_type garbage;
        if (!Statement::generate_return_statement(builder, ctx, nullptr)) {
            return false;
        }
    }

    return true;
}

std::optional<llvm::Function *> Generator::Function::generate_test_function(        //
    llvm::Module *module,                                                           //
    const TestNode *test_node,                                                      //
    const std::unordered_map<std::string, ImportNode *const> &imported_core_modules //
) {
    llvm::StructType *void_type = IR::add_and_or_get_type(Type::get_primitive_type("void"));

    // Create the function type
    llvm::FunctionType *test_type = llvm::FunctionType::get( //
        void_type,                                           // return { i32 }
        {},                                                  // takes nothing
        false                                                // no vararg
    );
    // Create the test function itself
    llvm::Function *test_function = llvm::Function::Create( //
        test_type,                                          //
        llvm::Function::ExternalLinkage,                    //
        "___test_" + std::to_string(test_node->test_id),    //
        module                                              //
    );

    // Create the entry block
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create( //
        context,                                              //
        "entry",                                              //
        test_function                                         //
    );

    // The test function has no parameters when called, it just returns whether it has succeeded through the error value
    llvm::IRBuilder<> builder(entry_block);
    std::unordered_map<std::string, llvm::Value *const> allocations;
    if (!Allocation::generate_allocations(builder, test_function, test_node->scope.get(), allocations, imported_core_modules)) {
        return std::nullopt;
    }
    // Normally generate the tests body
    GenerationContext ctx{test_function, test_node->scope.get(), allocations, imported_core_modules};
    if (!Statement::generate_body(builder, ctx)) {
        return std::nullopt;
    }

    // Check if the function has a terminator, if not add an "empty" return (only the error return)
    if (!test_function->empty() && test_function->back().getTerminator() == nullptr) {
        if (!Statement::generate_return_statement(builder, ctx, nullptr)) {
            return std::nullopt;
        }
    }

    return test_function;
}

std::pair<std::optional<llvm::Function *>, bool> Generator::Function::get_function_definition( //
    llvm::Function *parent,                                                                    //
    const CallNodeBase *call_node                                                              //
) {
    llvm::Function *func_decl = parent->getParent()->getFunction(call_node->function_name);
    if (func_decl != nullptr) {
        if (std::find(function_names.begin(), function_names.end(), call_node->function_name) == function_names.end()) {
            // Function is defined in another module
            return {func_decl, true};
        }
        // Function is defined in the current module
        return {func_decl, false};
    }
    // Get the mangle id by looking for the function's name
    std::optional<unsigned int> call_mangle_id = std::nullopt;
    for (const auto &file_mangle_map : file_function_mangle_ids) {
        for (const auto &[function_name, mangle_id] : file_mangle_map.second) {
            if (function_name == call_node->function_name) {
                call_mangle_id = mangle_id;
            }
        }
    }

    if (call_mangle_id.has_value()) {
        // Function has mangle id, for example a function call from another module
        func_decl = main_module[0]->getFunction(call_node->function_name + "." + std::to_string(call_mangle_id.value()));
    } else {
        // Function has no mangle id, for example a builtin function
        func_decl = main_module[0]->getFunction(call_node->function_name);
    }

    if (func_decl == nullptr) {
        // Use of undeclared function
        THROW_BASIC_ERR(ERR_GENERATING);
        return {std::nullopt, false};
    }

    return {func_decl, true};
}

bool Generator::Function::function_has_return(llvm::Function *function) {
    for (const llvm::BasicBlock &block : *function) {
        for (const llvm::Instruction &instr : block) {
            if (llvm::isa<llvm::ReturnInst>(&instr)) {
                return true;
            }
        }
    }
    return false;
}
