#include "error/error.hpp"
#include "generator/generator.hpp"
#include "parser/type/multi_type.hpp"

llvm::FunctionType *Generator::Function::generate_function_type(llvm::Module *module, FunctionNode *function_node) {
    llvm::Type *return_types = nullptr;
    const bool is_extern = function_node->extern_name_alias.has_value();
    if (is_extern && function_node->return_types.empty()) {
        // If it's extern and empty it's a void return type
        return_types = llvm::Type::getVoidTy(context);
    } else if (function_node->return_types.size() == 1) {
        if (is_extern) {
            return_types = IR::get_type(module, function_node->return_types.front(), true).first;
        } else {
            return_types = IR::add_and_or_get_type(module, function_node->return_types.front(), !is_extern, is_extern);
        }
    } else {
        std::shared_ptr<Type> group_type = std::make_shared<GroupType>(function_node->return_types);
        if (!Type::add_type(group_type)) {
            group_type = Type::get_type_from_str(group_type->to_string()).value();
        }
        return_types = IR::add_and_or_get_type(module, group_type, !is_extern, is_extern);
    }

    // Get the parameter types
    std::vector<llvm::Type *> param_types_vec;
    if (is_extern) {
        for (const auto &param : function_node->parameters) {
            const std::shared_ptr<Type> &param_type = std::get<0>(param);
            if (const MultiType *multi_type = dynamic_cast<const MultiType *>(param_type.get())) {
                llvm::Type *element_type = IR::get_type(module, multi_type->base_type).first;
                const std::string base_type_str = multi_type->base_type->to_string();
                if (base_type_str == "f64" || base_type_str == "i64") {
                    for (size_t i = 0; i < multi_type->width; i++) {
                        param_types_vec.emplace_back(element_type);
                    }
                    continue;
                }
                if (multi_type->width == 2) {
                    param_types_vec.emplace_back(IR::get_type(module, param_type).first);
                } else if (multi_type->width == 3) {
                    llvm::VectorType *vec2_type = llvm::VectorType::get(element_type, 2, false);
                    param_types_vec.emplace_back(vec2_type);
                    param_types_vec.emplace_back(element_type);
                } else {
                    llvm::VectorType *vec2_type = llvm::VectorType::get(element_type, 2, false);
                    for (size_t i = 0; i < multi_type->width; i += 2) {
                        param_types_vec.emplace_back(vec2_type);
                    }
                }
                continue;
            }
            param_types_vec.emplace_back(IR::get_type(module, std::get<0>(param), true).first);
        }
    } else {
        param_types_vec.reserve(function_node->parameters.size());
        for (const auto &param : function_node->parameters) {
            auto param_type = IR::get_type(module, std::get<0>(param));
            if (param_type.second) {
                // Complex type, passed by reference
                param_types_vec.emplace_back(param_type.first->getPointerTo());
            } else {
                // Primitive type, passed by copy
                param_types_vec.emplace_back(param_type.first);
            }
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
    llvm::FunctionType *function_type = generate_function_type(module, function_node);

    // Creating the function itself
    const bool is_extern = function_node->extern_name_alias.has_value();
    const std::string fn_name = is_extern          //
        ? function_node->extern_name_alias.value() //
        : function_node->name;
    llvm::Function *function = llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, fn_name, module);

    if (!function_node->scope.has_value()) {
        // It's only a declaration, not an implementation
        return true;
    }

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
    if (!Allocation::generate_allocations(builder, function, function_node->scope.value(), allocations, imported_core_modules)) {
        return false;
    }

    // Generate all instructions of the functions body
    GenerationContext ctx{function, function_node->scope.value(), allocations, imported_core_modules};
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
    llvm::StructType *void_type = IR::add_and_or_get_type(module, Type::get_primitive_type("void"));

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
    if (!Allocation::generate_allocations(builder, test_function, test_node->scope, allocations, imported_core_modules)) {
        return std::nullopt;
    }
    // Normally generate the tests body
    GenerationContext ctx{test_function, test_node->scope, allocations, imported_core_modules};
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
