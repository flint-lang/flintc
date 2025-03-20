#include "error/error.hpp"
#include "generator/generator.hpp"

llvm::FunctionType *Generator::Function::generate_function_type(llvm::LLVMContext &context, FunctionNode *function_node) {
    llvm::Type *return_types = IR::add_and_or_get_type(&context, function_node->return_types);

    // Get the parameter types
    std::vector<llvm::Type *> param_types_vec;
    param_types_vec.reserve(function_node->parameters.size());
    for (const auto &param : function_node->parameters) {
        auto param_type = IR::get_type_from_str(context, std::get<0>(param));
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

llvm::Function *Generator::Function::generate_function(llvm::Module *module, FunctionNode *function_node) {
    llvm::FunctionType *function_type = generate_function_type(module->getContext(), function_node);

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
        module->getContext(),                                 //
        "entry",                                              //
        function                                              //
    );
    llvm::IRBuilder<> builder(entry_block);

    // Create all the functions allocations (declarations, etc.) at the beginning, before the actual function body
    // The key is a combination of the scope id and the variable name, e.g. 1::var1, 2::var2
    std::unordered_map<std::string, llvm::Value *const> allocations;
    Allocation::generate_function_allocations(function, allocations, function_node);
    Allocation::generate_allocations(builder, function, function_node->scope.get(), allocations);

    // Generate all instructions of the functions body
    Statement::generate_body(builder, function, function_node->scope.get(), allocations);

    // Check if the function has a terminator, if not add an "empty" return (only the error return)
    if (!function->empty() && function->back().getTerminator() == nullptr) {
        Statement::generate_return_statement(builder, function, function_node->scope.get(), allocations, {});
    }

    return function;
}

std::pair<std::optional<llvm::Function *>, bool> Generator::Function::get_function_definition( //
    llvm::Function *parent,                                                                    //
    const CallNodeBase *call_node                                                              //
) {
    llvm::Function *func_decl = parent->getParent()->getFunction(call_node->function_name);
    // Check if the call is to a builtin function
    if (func_decl == nullptr && builtin_functions.find(call_node->function_name) != builtin_functions.end()) {
        if (builtin_functions.at(call_node->function_name) == PRINT) {
            // Print functions dont return anything, this no allocations have to be made
            return {nullptr, false};
        }
    }
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
