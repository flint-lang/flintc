#include "error/error.hpp"
#include "generator/generator.hpp"

llvm::FunctionType *Generator::Function::generate_function_type(llvm::Module *module, const FunctionNode *function_node) {
    llvm::Type *return_types = nullptr;
    llvm::Type *sret_param_type = nullptr;
    const bool is_extern = function_node->is_extern;
    if (!is_extern) {
        return llvm::FunctionType::get(     //
            llvm::Type::getInt1Ty(context), //
            PTR_TY,                         //
            false                           //
        );
    }
    ASSERT(is_extern);
    if (function_node->return_types.empty()) {
        // If it's extern and empty it's a void return type
        return_types = llvm::Type::getVoidTy(context);
    } else {
        std::shared_ptr<Type> ret_type = function_node->return_types.front();
        if (function_node->return_types.size() > 1) {
            ret_type = std::make_shared<GroupType>(function_node->return_types);
            Namespace *file_namespace = Resolver::get_namespace_from_hash(function_node->file_hash);
            if (!file_namespace->add_type(ret_type)) {
                ret_type = file_namespace->get_type_from_str(ret_type->to_string()).value();
            }
        }

        // Check if return type is > 16 bytes
        llvm::Type *actual_return_type = IR::get_type(module, ret_type, false).first;
        size_t return_size = Allocation::get_type_size(module, actual_return_type);
        if (return_size > 16) {
            // Return type becomes void
            return_types = llvm::Type::getVoidTy(context);
            // First parameter becomes sret pointer
            sret_param_type = PTR_TY;
        } else {
            // Existing logic for <= 16 bytes
            return_types = IR::get_type(module, ret_type, true).first;
        }
    }

    // Get the parameter types
    std::vector<llvm::Type *> param_types_vec;
    if (sret_param_type != nullptr) {
        // Add the sret parameter as the first parameter
        param_types_vec.emplace_back(sret_param_type);
    }

    for (const auto &param : function_node->parameters) {
        llvm::Type *param_type = IR::get_type(module, std::get<0>(param), true).first;
        if (param_type->isStructTy()) {
            llvm::StructType *struct_type = llvm::cast<llvm::StructType>(param_type);
            for (const auto &element_type : struct_type->elements()) {
                param_types_vec.emplace_back(element_type);
            }
        } else {
            param_types_vec.emplace_back(param_type);
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

bool Generator::Function::generate_function_setup(llvm::Module *module, const FunctionNode *function_node) {
    if (function_node->is_extern) {
        // Do nothing as the "declaration" of the extern function actually already happened inside the forward-declaration of the module
        return true;
    }
    llvm::FunctionType *function_type = generate_function_type(module, function_node);

    // Creating the function itself only if it's the main function. All other functions should have been forward-declared already so we
    // should be able to just get them from the module itself
    llvm::Function *function = nullptr;
    if (function_node->name == "_main") {
        function = llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, "_main", module);
    } else {
        std::string function_name = function_node->file_hash.to_string() + "." + function_node->name;
        if (function_node->mangle_id.has_value()) {
            ASSERT(!function_node->is_extern);
            function_name += "." + std::to_string(function_node->mangle_id.value());
        }
        function = module->getFunction(function_name);
        if (function == nullptr) {
            THROW_BASIC_ERR(ERR_GENERATING);
            return false;
        }
    }
    if (OPTIMIZE_MODE != OptimizeMode::DEBUG) {
#ifndef __WIN32__
        // Add 'tailcc' to every user-defined function
        function->setCallingConv(llvm::CallingConv::Tail);
#endif
    }

    if (!function_node->scope.has_value()) {
        // It's only a declaration, not an implementation
        return true;
    }

    ASSERT(function->arg_size() == 1);
    llvm::Argument *const stack_arg = function->args().begin();
    stack_arg->setName("stack");
#ifndef __WIN32__
    stack_arg->addAttr(llvm::Attribute::InReg);
#endif

    // Create the functions setup block
    llvm::BasicBlock *setup_block = llvm::BasicBlock::Create( //
        context,                                              //
        "setup",                                              //
        function                                              //
    );
    llvm::IRBuilder<> builder(setup_block);

    // Create all the functions allocations (declarations, etc.) at the beginning, before the actual function body
    // The key is a combination of the scope id and the variable name, e.g. 1::var1, 2::var2
    // Because of the thread stack, these "allocations" are now fixed pointer offsets (GEPs) into the function structure, nothing more and
    // nothing less
    std::unordered_map<std::string, llvm::Value *const> allocations;
    const auto fn_ty = Allocation::generate_function_allocations(builder, function, function_node, allocations);
    if (!fn_ty.has_value()) {
        return false;
    }

    // Store the function and allocations in the function context to be used later in the `generate_function_body` function
    const size_t fn_id = function_node->get_id();
    ASSERT(function_contexts.find(fn_id) == function_contexts.end());
    function_contexts[fn_id] = FunctionContext{
        .function = function,
        .function_type = fn_ty.value(),
        .allocations = allocations,
    };
    return true;
}

bool Generator::Function::generate_function_body(                                   //
    FunctionNode *function_node,                                                    //
    const std::unordered_map<std::string, ImportNode *const> &imported_core_modules //
) {
    if (function_node->is_extern) {
        // Do nothing as the "declaration" of the extern function actually already happened inside the forward-declaration of the module
        return true;
    }
    if (!function_node->scope.has_value()) {
        // It's only a declaration, not an implementation
        return true;
    }

    const size_t fn_id = function_node->get_id();
    FunctionContext fn_ctx = function_contexts.at(fn_id);
    llvm::Function *const function = fn_ctx.function;

    // Create the functions entry block
    llvm::BasicBlock *setup_block = &function->getEntryBlock();
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create( //
        context,                                              //
        "entry",                                              //
        function                                              //
    );
    llvm::IRBuilder<> builder(entry_block);
    builder.SetInsertPoint(setup_block);
    builder.CreateBr(entry_block);
    builder.SetInsertPoint(entry_block);

    // Generate all instructions of the functions body
    GenerationContext ctx{fn_ctx.function_type, function, function_node->scope.value(), 0, fn_ctx.allocations, imported_core_modules};
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
    // Create the function type
    llvm::FunctionType *test_type = llvm::FunctionType::get( //
        llvm::Type::getInt1Ty(context),                      // return i1
        {PTR_TY},                                            // ptr stack
        false                                                // no vararg
    );
    // Create the test function itself
    const std::string test_name = "___test_" + std::to_string(test_node->test_id);
    llvm::Function *const test_function = llvm::Function::Create(test_type, llvm::Function::ExternalLinkage, test_name, module);
    llvm::Argument *const stack_arg = test_function->args().begin();
    stack_arg->setName("stack");
#ifndef __WIN32__
    stack_arg->addAttr(llvm::Attribute::InReg);
#endif

    // Create the entry block
    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create( //
        context,                                              //
        "entry",                                              //
        test_function                                         //
    );

    // The test function has no parameters when called, it just returns whether it has succeeded through the error value
    llvm::IRBuilder<> builder(entry_block);
    std::vector<std::tuple<std::shared_ptr<Type>, std::string, bool>> fake_fn_parameters;
    std::vector<std::shared_ptr<Type>> fake_fn_return_types;
    std::vector<std::shared_ptr<Type>> fake_fn_error_types;
    const std::optional<size_t> fake_fn_mangle_id;
    std::optional<std::shared_ptr<Scope>> fake_fn_scope = std::make_optional(test_node->scope);
    FunctionNode fake_fn = FunctionNode( //
        test_node->file_hash,            //
        test_node->line,                 //
        test_node->column,               //
        test_node->length,               //
        false,                           //
        false,                           //
        false,                           //
        test_name,                       //
        fake_fn_parameters,              //
        fake_fn_return_types,            //
        fake_fn_error_types,             //
        fake_fn_scope,                   //
        fake_fn_mangle_id                //
    );
    std::unordered_map<std::string, llvm::Value *const> allocations;
    const auto fn_ty = Allocation::generate_function_allocations(builder, test_function, &fake_fn, allocations);
    if (!fn_ty.has_value()) {
        return std::nullopt;
    }
    // Normally generate the tests body
    GenerationContext ctx{fn_ty.value(), test_function, test_node->scope, 0, allocations, imported_core_modules};
    if (!Statement::generate_body(builder, ctx)) {
        return std::nullopt;
    }

    // Check if the function has a terminator, if not return false (no error)
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
    std::string function_name = call_node->function->name;
    if (!call_node->function->is_extern) {
        function_name = call_node->function->file_hash.to_string() + "." + function_name;
    }
    if (call_node->function->mangle_id.has_value()) {
        ASSERT(!call_node->function->is_extern);
        function_name += "." + std::to_string(call_node->function->mangle_id.value());
    }
    llvm::Function *func_decl = parent->getParent()->getFunction(function_name);
    if (func_decl != nullptr) {
        if (std::find(function_names.begin(), function_names.end(), function_name) == function_names.end() //
            && !call_node->function->mangle_id.has_value()                                                 //
        ) {
            // Function is defined in another module
            return {func_decl, true};
        }
        // Function is defined in the current module
        return {func_decl, false};
    }

    if (call_node->function->mangle_id.has_value()) {
        ASSERT(!call_node->function->is_extern);
        // Function has mangle id, for example a function call from another module
        // Externally defined functions are not mangled, this is why we do not need mangling at all for them
        func_decl = main_module[0]->getFunction(function_name);
    } else {
        // Function has no mangle id, for example a builtin function or external functions
        func_decl = main_module[0]->getFunction(call_node->function->name);
    }

    if (func_decl == nullptr) {
        // Let's print all function names we are aware of
        std::cout << "All known functions:" << std::endl;
        for (const auto &fn : main_module[0]->functions()) {
            std::cout << "  " << fn.getName().str() << std::endl;
        }
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
