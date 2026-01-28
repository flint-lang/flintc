#include "generator/generator.hpp"

#include "parser/parser.hpp"

const std::string prefix = "flint.";

void Generator::Memory::generate_memory_functions(llvm::IRBuilder<> *builder, llvm::Module *module, const bool only_declarations) {
    generate_free_function(builder, module, only_declarations);
    generate_clone_function(builder, module, only_declarations);
}

void Generator::Memory::generate_free_value( //
    llvm::IRBuilder<> *const builder,        //
    llvm::Module *const module,              //
    llvm::Value *const value,                //
    const std::shared_ptr<Type> &type        //
) {
    switch (type->get_variation()) {
        default:
            break;
        case Type::Variation::ARRAY: {
            const auto *array_type = type->as<ArrayType>();
            if (!array_type->type->is_freeable()) {
                // Just call free on the value and done, as the value stored in the array is not freeable
                llvm::Function *free_fn = c_functions.at(FREE);
                builder->CreateCall(free_fn, {value});
                break;
            }
            // If the element of the array is freeable then we need to generate the freeing code for every single value of the array
            // This means we need to generate a loop and shit like that
            llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("type.flint.str")).first;
            llvm::Value *dim_ptr = builder->CreateStructGEP(str_type, value, 0, "dim_ptr");
            llvm::Value *dimensionality = IR::aligned_load(*builder, builder->getInt64Ty(), dim_ptr, "dimensionality");
            llvm::Value *length = builder->getInt64(1);
            llvm::Value *len_ptr = builder->CreateStructGEP(str_type, value, 1, "len_ptr");
            for (size_t i = 0; i < array_type->dimensionality; i++) {
                llvm::Value *single_len_ptr = builder->CreateGEP(builder->getInt64Ty(), len_ptr, builder->getInt64(i));
                llvm::Value *single_len = IR::aligned_load(*builder, builder->getInt64Ty(), single_len_ptr, "len_" + std::to_string(i));
                length = builder->CreateMul(length, single_len);
            }
            // The values start right after the lengths
            llvm::Value *value_ptr = builder->CreateGEP(builder->getInt64Ty(), len_ptr, dimensionality);
            llvm::AllocaInst *idx = builder->CreateAlloca(builder->getInt64Ty(), 0, nullptr, "idx");
            IR::aligned_store(*builder, builder->getInt64(0), idx);
            // Now we iterate through the whole array and generate the free falue for each element, we only do this once of course
            llvm::BasicBlock *current_block = builder->GetInsertBlock();
            llvm::BasicBlock *loop_cond_block = llvm::BasicBlock::Create(             //
                context, type->to_string() + "_loop_cond", current_block->getParent() //
            );
            llvm::BasicBlock *loop_body_block = llvm::BasicBlock::Create(             //
                context, type->to_string() + "_loop_body", current_block->getParent() //
            );
            llvm::BasicBlock *loop_merge_block = llvm::BasicBlock::Create(             //
                context, type->to_string() + "_loop_merge", current_block->getParent() //
            );
            builder->SetInsertPoint(current_block);
            builder->CreateBr(loop_cond_block);

            builder->SetInsertPoint(loop_cond_block);
            llvm::Value *idx_value = IR::aligned_load(*builder, builder->getInt64Ty(), idx, "idx_value");
            llvm::Value *idx_lt_length = builder->CreateICmpULT(idx_value, length, "idx_lt_length");
            IR::generate_debug_print(builder, module, "free_" + type->to_string() + "_cond: (idx, length) = (%lu, %lu)",
                {idx_value, length});
            builder->CreateCondBr(idx_lt_length, loop_body_block, loop_merge_block);

            builder->SetInsertPoint(loop_body_block);
            IR::generate_debug_print(builder, module, "free_" + type->to_string() + "_body: (idx, length) = (%lu, %lu)",
                {idx_value, length});
            auto element_type_pair = IR::get_type(module, array_type->type);
            llvm::Type *element_type = element_type_pair.second.first ? element_type_pair.first->getPointerTo() : element_type_pair.first;
            llvm::Value *arr_value_ptr = builder->CreateGEP(element_type, value_ptr, idx_value, "arr_value_ptr");
            llvm::Value *arr_value = arr_value_ptr;
            const bool base_is_array = array_type->type->get_variation() == Type::Variation::ARRAY;
            const bool base_is_str = array_type->type->to_string() == "str";
            if (element_type_pair.second.first || base_is_array || base_is_str) {
                arr_value = IR::aligned_load(*builder, element_type, arr_value_ptr, "arr_value");
            }
            if (array_type->type->get_variation() == Type::Variation::DATA) {
                // Data is released in DIMA. If the ARC falls to 0 then DIMA will call the free function of the data
                llvm::Value *dima_head = Module::DIMA::get_head(array_type->type);
                llvm::Function *dima_release_fn = Module::DIMA::dima_functions.at("release");
                builder->CreateCall(dima_release_fn, {dima_head, value});
            } else {
                llvm::Function *free_fn = memory_functions.at("free");
                builder->CreateCall(free_fn, {arr_value, builder->getInt32(array_type->type->get_id())});
            }
            builder->CreateBr(loop_cond_block);

            builder->SetInsertPoint(loop_merge_block);
            break;
        }
        case Type::Variation::DATA: {
            // The 'value' is the data which needs to be freed itself. This means that we first need to go through all fields of the data
            // first, check if they need to be freed too, emit the freeing code for those and *then* at the end we call `dima.release` on
            // *this* data value
            const auto *data_node = type->as<DataType>()->data_node;
            llvm::Type *data_type = IR::get_type(module, type).first;
            for (size_t i = 0; i < data_node->fields.size(); i++) {
                const auto &[field_name, field_type] = data_node->fields.at(i);
                if (!field_type->is_freeable()) {
                    continue;
                }
                llvm::Value *data_field_ptr = builder->CreateStructGEP(data_type, value, i, "data_field_ptr_" + field_name);
                auto field_type_pair = IR::get_type(module, field_type);
                llvm::Value *data_field = data_field_ptr;
                const bool field_is_array = field_type->get_variation() == Type::Variation::ARRAY;
                const bool field_is_str = field_type->to_string() == "str";
                if (field_type_pair.second.first || field_is_array || field_is_str) {
                    llvm::Type *field_type_ptr = field_type_pair.first->getPointerTo();
                    data_field = IR::aligned_load(                                           //
                        *builder, field_type_ptr, data_field_ptr, "data_field_" + field_name //
                    );
                }
                if (field_type->get_variation() == Type::Variation::DATA) {
                    // Data is released in DIMA. If the ARC falls to 0 then DIMA will call the free function of the data
                    llvm::Value *dima_head = Module::DIMA::get_head(field_type);
                    llvm::Function *dima_release_fn = Module::DIMA::dima_functions.at("release");
                    builder->CreateCall(dima_release_fn, {dima_head, data_field});
                } else {
                    llvm::Function *free_fn = memory_functions.at("free");
                    builder->CreateCall(free_fn, {data_field, builder->getInt32(field_type->get_id())});
                }
            }
            break;
        }
        case Type::Variation::ENTITY: {
            const auto *entity_type = type->as<EntityType>();
            llvm::Type *struct_type = IR::get_type(module, type).first;
            for (size_t i = 0; i < entity_type->entity_node->data_modules.size(); i++) {
                const auto &data_node = entity_type->entity_node->data_modules.at(i);
                const Namespace *data_namespace = Resolver::get_namespace_from_hash(data_node->file_hash);
                const std::shared_ptr<Type> data_type = data_namespace->get_type_from_str(data_node->name).value();
                const std::string data_type_str = data_type->to_string();
                llvm::Value *field_ptr = builder->CreateStructGEP(struct_type, value, i, "field_" + data_type_str + "_ptr");
                llvm::Type *base_type = IR::get_type(module, data_type).first;
                llvm::Value *data_value = IR::aligned_load(*builder, base_type->getPointerTo(), field_ptr, "data_value");
                llvm::Function *release_fn = Module::DIMA::dima_functions.at("release");
                builder->CreateCall(release_fn, {Module::DIMA::get_head(data_type), data_value});
            }
            break;
        }
        case Type::Variation::ERROR_SET: {
            llvm::StructType *error_type = type_map.at("type.flint.err");
            llvm::Value *err_message_ptr = builder->CreateStructGEP(error_type, value, 2, "err_message_ptr");
            llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("str")).first;
            llvm::Value *err_message = IR::aligned_load(*builder, str_type, err_message_ptr, "err_message");
            builder->CreateCall(c_functions.at(FREE), {err_message});
            break;
        }
        case Type::Variation::FUNC:
            // TODO: Implement freeing loginc for func modules once func-modules as-interfaces work
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            break;
        case Type::Variation::PRIMITIVE: {
            assert(type->to_string() == "str");
            llvm::Function *free_fn = c_functions.at(FREE);
            builder->CreateCall(free_fn, {value});
            break;
        }
        case Type::Variation::OPTIONAL:
            // TODO: Implement freeing of optional logic
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            break;
        case Type::Variation::TUPLE:
            // TODO: Implement freeing of tuple logic
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            break;
        case Type::Variation::VARIANT:
            const auto *variant_type = type->as<VariantType>();
            if (variant_type->is_err_variant) {
                llvm::StructType *error_type = type_map.at("type.flint.err");
                llvm::Value *err_message_ptr = builder->CreateStructGEP(error_type, value, 2, "err_message_ptr");
                llvm::Type *str_type = IR::get_type(module, Type::get_primitive_type("str")).first;
                llvm::Value *err_message = IR::aligned_load(*builder, str_type, err_message_ptr, "err_message");
                builder->CreateCall(c_functions.at(FREE), {err_message});
            } else {
                llvm::BasicBlock *prev_block = builder->GetInsertBlock();
                std::map<size_t, llvm::BasicBlock *> possible_value_blocks;
                const auto &possible_types = variant_type->get_possible_types();
                for (size_t i = 0; i < possible_types.size(); i++) {
                    // Check if the type is freeable
                    const auto &[tag, possible_type] = possible_types[i];
                    if (!possible_type->is_freeable()) {
                        continue;
                    }
                    possible_value_blocks[i] = llvm::BasicBlock::Create(                   //
                        context, type->to_string() + "_free_" + possible_type->to_string() //
                    );
                }
                if (possible_value_blocks.empty()) {
                    // If there are no complex values inside the variant then we do not need to free anything
                    break;
                }
                // Create the merge block of the variant free and create the switch statement at the end of the current block to branch
                // to each type.
                llvm::BasicBlock *variant_free_merge_block = llvm::BasicBlock::Create(context, type->to_string() + "_free_merge");
                llvm::StructType *variant_struct_type = IR::add_and_or_get_type(module, type);
                llvm::Value *variant_active_value_ptr = builder->CreateStructGEP( //
                    variant_struct_type, value, 0, "variant_active_value_ptr"     //
                );
                llvm::Value *variant_active_value = IR::aligned_load(                                //
                    *builder, builder->getInt8Ty(), variant_active_value_ptr, "variant_active_value" //
                );
                const size_t block_count = possible_value_blocks.size();
                llvm::SwitchInst *active_value_switch = builder->CreateSwitch(variant_active_value, variant_free_merge_block, block_count);

                // Now generate the content of each block, get the value of the variant and free the value in it
                for (auto &[value_id, value_block] : possible_value_blocks) {
                    active_value_switch->addCase(builder->getInt8(value_id), value_block);
                    value_block->insertInto(prev_block->getParent());
                    builder->SetInsertPoint(value_block);
                    llvm::Value *variant_value_ptr = builder->CreateStructGEP(variant_struct_type, value, 1, "variant_value_ptr");
                    const auto &variant_type_ptr = possible_types.at(value_id).second;
                    auto value_type = IR::get_type(module, variant_type_ptr);
                    llvm::Value *variant_value = variant_value_ptr;
                    const bool value_is_array = variant_type_ptr->get_variation() == Type::Variation::ARRAY;
                    const bool value_is_str = variant_type_ptr->to_string() == "str";
                    if (value_type.second.first || value_is_array || value_is_str) {
                        variant_value = IR::aligned_load(                                                  //
                            *builder, value_type.first->getPointerTo(), variant_value_ptr, "variant_value" //
                        );
                    }
                    if (variant_type_ptr->get_variation() == Type::Variation::DATA) {
                        // Data is released in DIMA. If the ARC falls to 0 then DIMA will call the free function of the data
                        llvm::Value *dima_head = Module::DIMA::get_head(variant_type_ptr);
                        llvm::Function *dima_release_fn = Module::DIMA::dima_functions.at("release");
                        builder->CreateCall(dima_release_fn, {dima_head, value});
                    } else {
                        builder->CreateCall(memory_functions.at("free"), {variant_value, builder->getInt32(variant_type_ptr->get_id())});
                        builder->CreateBr(variant_free_merge_block);
                    }
                }

                variant_free_merge_block->insertInto(prev_block->getParent());
                builder->SetInsertPoint(variant_free_merge_block);
            }
            break;
    }
}

void Generator::Memory::generate_free_function( //
    llvm::IRBuilder<> *builder,                 //
    llvm::Module *module,                       //
    const bool only_declarations                //
) {
    llvm::FunctionType *free_value_type = llvm::FunctionType::get( //
        llvm::Type::getVoidTy(context),                            // Returns void
        {
            llvm::Type::getInt8Ty(context)->getPointerTo(), // void* value_ptr
            llvm::Type::getInt32Ty(context)                 // u32 type_id
        },
        false // No vaargs
    );
    llvm::Function *free_value_fn = llvm::Function::Create( //
        free_value_type,                                    //
        llvm::Function::ExternalLinkage,                    //
        prefix + "free",                                    //
        module                                              //
    );
    memory_functions["free"] = free_value_fn;
    if (only_declarations) {
        return;
    }

    llvm::BasicBlock *entry_block = llvm::BasicBlock::Create(context, "entry", free_value_fn);
    llvm::BasicBlock *default_block = llvm::BasicBlock::Create(context, "default", free_value_fn);

    // Get the parameters
    llvm::Argument *arg_value_ptr = free_value_fn->arg_begin();
    arg_value_ptr->setName("value_ptr");
    llvm::Argument *arg_type_id = free_value_fn->arg_begin() + 1;
    arg_type_id->setName("type_id");

    // Set insertion point to entry block
    builder->SetInsertPoint(entry_block);

    // Get all freeable types
    std::vector<std::shared_ptr<Type>> freeable_types = Parser::get_all_freeable_types();

    // Create the switch instruction (we'll add cases to it)
    // Number of cases = number of freeable types
    llvm::SwitchInst *switch_inst = builder->CreateSwitch(arg_type_id, default_block, freeable_types.size());

    // Add cases for each data type
    for (const auto &type : freeable_types) {
        llvm::BasicBlock *case_block = llvm::BasicBlock::Create(context, "case_" + type->to_string(), free_value_fn);
        switch_inst->addCase(builder->getInt32(type->get_id()), case_block);

        builder->SetInsertPoint(case_block);
        IR::generate_debug_print(builder, module, "memory.free.in(" + type->to_string() + ")", {});
        generate_free_value(builder, module, arg_value_ptr, type);
        IR::generate_debug_print(builder, module, "memory.free.out(" + type->to_string() + ")", {});
        builder->CreateRetVoid();
    }

    // Default case: print error message and abort
    builder->SetInsertPoint(default_block);
    llvm::Value *unknown_err_msg = IR::generate_const_string(module, "Unknown type id for 'dima.free_value': %u\n");
    builder->CreateCall(c_functions.at(PRINTF), {unknown_err_msg, arg_type_id});
    builder->CreateCall(c_functions.at(ABORT), {});
    builder->CreateUnreachable();
}

void Generator::Memory::generate_clone_function(  //
    [[maybe_unused]] llvm::IRBuilder<> *builder,  //
    [[maybe_unused]] llvm::Module *module,        //
    [[maybe_unused]] const bool only_declarations //
) {
    // THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
}
