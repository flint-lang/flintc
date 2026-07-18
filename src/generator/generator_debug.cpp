#include "generator/generator.hpp"

#include "parser/parser.hpp"
#include "parser/type/alias_type.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/func_type.hpp"
#include "parser/type/interface_type.hpp"
#include "parser/type/multi_type.hpp"
#include "parser/type/object_type.hpp"
#include "parser/type/variant_type.hpp"

#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/IR/DebugInfoMetadata.h>

llvm::DIType *Generator::Debug::create_debug_type_array(llvm::Module *const module, const std::shared_ptr<Type> &type) {
    const auto *array_type = type->as<ArrayType>();

    if (array_type->sizes.has_value()) {
        // Static array: comptime-known sizes
        llvm::DIType *const elem_dt = get_or_create_debug_type(module, array_type->type);
        const auto elem_llvm_pair = IR::get_type(module, array_type->type);
        llvm::Type *const elem_llvm = elem_llvm_pair.second.first ? PTR_TY : elem_llvm_pair.first;

        size_t total_elems = 1;
        for (const size_t len : array_type->sizes.value()) {
            total_elems *= len;
        }

        llvm::ArrayType *const arr_llvm = llvm::ArrayType::get(elem_llvm, total_elems);
        const uint64_t size_bits = Allocation::get_type_size(module, arr_llvm) * 8;
        const uint32_t align_bits = Allocation::calculate_type_alignment(arr_llvm) * 8;

        std::vector<llvm::Metadata *> subs;
        for (const size_t len : array_type->sizes.value()) {
            subs.push_back(DIB->getOrCreateSubrange(0, static_cast<int64_t>(len)));
        }

        return DIB->createArrayType(size_bits, align_bits, elem_dt, DIB->getOrCreateArray(subs));
    }

    // Dynamic array: pointer → { dimensionality: u64, lengths: u64[N], data: T[0] }
    ASSERT(DCU != nullptr);
    llvm::DIFile *const file = DCU->getFile();

    const size_t dimensionality = array_type->dimensionality;
    llvm::DIType *const u64_dt = get_or_create_debug_type(module, Type::get_primitive_type("u64"));
    llvm::DIType *const elem_dt = get_or_create_debug_type(module, array_type->type);

    const auto elem_llvm_pair = IR::get_type(module, array_type->type);
    llvm::Type *const elem_llvm = elem_llvm_pair.second.first ? PTR_TY : elem_llvm_pair.first;
    const uint32_t elem_align = Allocation::calculate_type_alignment(elem_llvm) * 8;
    llvm::Type *const i64_llvm = llvm::Type::getInt64Ty(context);
    const uint32_t i64_align = Allocation::calculate_type_alignment(i64_llvm) * 8;

    std::vector<llvm::Metadata *> members;
    uint64_t offset = 0;

    members.push_back(DIB->createMemberType(file, "dimensionality", file, 0, 64, i64_align, offset, llvm::DINode::FlagZero, u64_dt));
    offset += 64;

    llvm::DISubrange *const len_sub = DIB->getOrCreateSubrange(0, static_cast<int64_t>(dimensionality));
    const uint64_t len_size = dimensionality * 64;
    llvm::DICompositeType *const len_arr = DIB->createArrayType(len_size, i64_align, u64_dt, DIB->getOrCreateArray({len_sub}));
    if (offset % i64_align != 0) {
        offset += i64_align - (offset % i64_align);
    }
    members.push_back(DIB->createMemberType(file, "lengths", file, 0, len_size, i64_align, offset, llvm::DINode::FlagZero, len_arr));
    offset += len_size;

    // Do a small debugging runtime-expression to be able to display exactly the number of elements stored in the (multi-dimensional) array
    std::vector<uint64_t> count_ops;
    for (size_t i = 0; i < dimensionality; i++) {
        count_ops.push_back(llvm::dwarf::DW_OP_push_object_address);
        count_ops.push_back(llvm::dwarf::DW_OP_constu);
        count_ops.push_back((dimensionality - i) * 8);
        count_ops.push_back(llvm::dwarf::DW_OP_minus);
        count_ops.push_back(llvm::dwarf::DW_OP_deref_size);
        count_ops.push_back(8);
        if (i > 0) {
            count_ops.push_back(llvm::dwarf::DW_OP_mul);
        }
    }
    llvm::DIExpression *const count_expr = llvm::DIExpression::get(context, count_ops);
    llvm::DISubrange *const data_sub = DIB->getOrCreateSubrange(0, count_expr);
    llvm::DICompositeType *const data_arr = DIB->createArrayType(0, elem_align, elem_dt, DIB->getOrCreateArray({data_sub}));
    if (offset % elem_align != 0) {
        offset += elem_align - (offset % elem_align);
    }
    members.push_back(DIB->createMemberType(file, "data", file, 0, 0, elem_align, offset, llvm::DINode::FlagZero, data_arr));

    const uint32_t struct_align = std::max(i64_align, elem_align);
    uint64_t struct_size = offset;
    if (struct_size % struct_align != 0) {
        struct_size += struct_align - (struct_size % struct_align);
    }

    llvm::DICompositeType *const struct_type = DIB->createStructType(file, type->to_string(), file, 0, struct_size, struct_align,
        llvm::DINode::FlagZero, nullptr, DIB->getOrCreateArray(members), 0, nullptr, type->get_type_string());

    const size_t PTR_SIZE = module->getDataLayout().getPointerSizeInBits();
    return DIB->createPointerType(struct_type, PTR_SIZE, 0);
}

llvm::DIType *Generator::Debug::create_debug_type_data(llvm::Module *const module, const std::shared_ptr<Type> &type) {
    const auto *node = type->as<DataType>()->data_node;
    if (debug_files.find(node->file_hash) == debug_files.end()) {
        const auto &path = node->file_hash.path;
        if (!path.empty()) {
            debug_files[node->file_hash] = DIB->createFile(path.filename().string(), path.parent_path().string());
        }
    }
    llvm::DIFile *const file_meta = debug_files.at(node->file_hash);

    const std::string llvm_type_str = type->get_type_string();
    ASSERT(type_map.find(llvm_type_str) != type_map.end());
    llvm::StructType *const llvm_struct = type_map.at(llvm_type_str);

    const uint64_t struct_size_bits = Allocation::get_type_size(module, llvm_struct) * 8;
    const uint32_t struct_align_bits = Allocation::calculate_type_alignment(llvm_struct) * 8;

    std::vector<llvm::Metadata *> member_types;
    uint64_t offset_bits = 0;

    for (size_t i = 0; i < node->fields.size(); ++i) {
        const auto &field = node->fields[i];
        llvm::DIType *const field_debug_type = get_or_create_debug_type(module, field.type);

        llvm::Type *const field_llvm_type = llvm_struct->getElementType(i);
        uint64_t const field_size_bits = Allocation::get_type_size(module, field_llvm_type) * 8;
        uint32_t const field_align_bits = Allocation::calculate_type_alignment(field_llvm_type) * 8;

        if (offset_bits % field_align_bits != 0) {
            offset_bits += field_align_bits - (offset_bits % field_align_bits);
        }

        llvm::DIDerivedType *const member = DIB->createMemberType(                           //
            file_meta, field.name, file_meta, node->line, field_size_bits, field_align_bits, //
            offset_bits, llvm::DINode::FlagZero, field_debug_type                            //
        );
        member_types.push_back(member);
        offset_bits += field_size_bits;
    }

    const llvm::DINodeArray elements = DIB->getOrCreateArray(member_types);
    llvm::DICompositeType *const struct_type = DIB->createStructType(                      //
        file_meta, node->name, file_meta, node->line, struct_size_bits, struct_align_bits, //
        llvm::DINode::FlagZero, nullptr, elements, 0, nullptr, llvm_type_str               //
    );
    // Wrap the struct in a pointer for correct data type encoding
    const size_t PTR_SIZE = module->getDataLayout().getPointerSizeInBits();
    return DIB->createPointerType(struct_type, PTR_SIZE, 0);
}

llvm::DIType *Generator::Debug::create_debug_type_object(llvm::Module *const module, const std::shared_ptr<Type> &type) {
    const auto *object_node = type->as<ObjectType>()->object_node;
    if (debug_files.find(object_node->file_hash) == debug_files.end()) {
        const auto &path = object_node->file_hash.path;
        if (!path.empty()) {
            debug_files[object_node->file_hash] = DIB->createFile(path.filename().string(), path.parent_path().string());
        }
    }
    llvm::DIFile *const file_meta = debug_files.at(object_node->file_hash);

    const std::string llvm_type_str = type->get_type_string();
    ASSERT(type_map.find(llvm_type_str) != type_map.end());
    llvm::StructType *const llvm_struct = type_map.at(llvm_type_str);

    const uint64_t struct_size_bits = Allocation::get_type_size(module, llvm_struct) * 8;
    const uint32_t struct_align_bits = Allocation::calculate_type_alignment(llvm_struct) * 8;

    const auto &constructor_order = object_node->constructor_order;
    const size_t PTR_SIZE = module->getDataLayout().getPointerSizeInBits();
    std::vector<llvm::Metadata *> member_types;
    for (size_t i = 0; i < object_node->data_modules.size(); ++i) {
        const auto &[data_node, accessor_name] = object_node->data_modules.at(constructor_order.at(i));
        const auto data_type = Resolver::get_namespace_from_hash(data_node->file_hash)->get_type_from_str(data_node->name).value();
        llvm::DIType *const data_debug_type = get_or_create_debug_type(module, data_type);

        llvm::Type *const field_llvm_type = llvm_struct->getElementType(i);
        const size_t field_size_bits = Allocation::get_type_size(module, field_llvm_type) * 8;
        const uint32_t field_align_bits = Allocation::calculate_type_alignment(field_llvm_type) * 8;

        const size_t offset = i * PTR_SIZE;
        llvm::DIDerivedType *const member = DIB->createMemberType(                            //
            file_meta, data_type->to_string(), file_meta, object_node->line, field_size_bits, //
            field_align_bits, offset, llvm::DINode::FlagZero, data_debug_type                 //
        );
        member_types.push_back(member);
    }

    const llvm::DINodeArray elements = DIB->getOrCreateArray(member_types);
    llvm::DICompositeType *const struct_type = DIB->createStructType(                           //
        file_meta, object_node->name, file_meta, object_node->line, struct_size_bits,           //
        struct_align_bits, llvm::DINode::FlagZero, nullptr, elements, 0, nullptr, llvm_type_str //
    );

    return DIB->createPointerType(struct_type, PTR_SIZE, 0);
}

llvm::DIType *Generator::Debug::create_debug_type_enum(llvm::Module *const module, const std::shared_ptr<Type> &type) {
    const auto *enum_node = type->as<EnumType>()->enum_node;
    if (debug_files.find(enum_node->file_hash) == debug_files.end()) {
        const auto &path = enum_node->file_hash.path;
        if (!path.empty()) {
            debug_files[enum_node->file_hash] = DIB->createFile(path.filename().string(), path.parent_path().string());
        }
    }
    llvm::DIFile *const file_meta = debug_files.at(enum_node->file_hash);

    llvm::DIType *const underlying_type = get_or_create_debug_type(module, Type::get_primitive_type("i32"));
    llvm::Type *const i32_llvm = llvm::Type::getInt32Ty(context);
    const uint64_t size_bits = Allocation::get_type_size(module, i32_llvm) * 8;
    const uint32_t align_bits = Allocation::calculate_type_alignment(i32_llvm) * 8;

    std::vector<llvm::Metadata *> enumerators;
    for (const auto &[name, value] : enum_node->values) {
        enumerators.push_back(DIB->createEnumerator(name, value));
    }

    const llvm::DINodeArray elements = DIB->getOrCreateArray(enumerators);
    return DIB->createEnumerationType(                                     //
        file_meta, enum_node->name, file_meta, enum_node->line, size_bits, //
        align_bits, elements, underlying_type, 0, type->get_type_string()  //
    );
}

llvm::DIType *Generator::Debug::create_debug_type_error(llvm::Module *const module) {
    static llvm::DIType *error_type = nullptr;
    if (error_type != nullptr) {
        return error_type;
    }

    std::vector<const ErrorNode *> errors = Parser::get_all_errors();
    ASSERT(!errors.empty());

    const uint64_t u32_size_bits = 32;
    const uint32_t u32_align_bits = 32;
    const uint64_t ptr_size_bits = Allocation::get_type_size(module, PTR_TY) * 8;
    const uint32_t ptr_align_bits = Allocation::calculate_type_alignment(PTR_TY) * 8;
    const uint64_t struct_size_bits = ptr_size_bits + u32_size_bits + u32_size_bits;
    const uint32_t struct_align_bits = static_cast<uint32_t>(ptr_align_bits);

    llvm::DIFile *const file = DCU->getFile();
    llvm::DIType *const u32_type = DIB->createBasicType("u32", u32_size_bits, llvm::dwarf::DW_ATE_unsigned);
    llvm::DIType *const str_debug_type = get_or_create_debug_type(module, Type::get_primitive_type("str"));

    std::vector<llvm::Metadata *> error_types;
    for (const auto &error : errors) {
        error_types.push_back(DIB->createEnumerator(error->name, error->error_id));
    }
    const llvm::DINodeArray enum_elements = DIB->getOrCreateArray(error_types);
    llvm::DICompositeType *const value_type = DIB->createEnumerationType(    //
        file, "type.flint.err.enum", file, 0, u32_size_bits, u32_align_bits, //
        enum_elements, u32_type, 0, "type.flint.err.enum"                    //
    );
    llvm::DIDerivedType *const type_field = DIB->createMemberType( //
        file, "type", file, 0, u32_size_bits, u32_align_bits, 0,   //
        llvm::DINode::FlagZero, value_type                         //
    );
    llvm::DIDerivedType *const value_field = DIB->createMemberType( //
        file, "value", file, 0, u32_size_bits, u32_align_bits,      //
        u32_size_bits, llvm::DINode::FlagZero, u32_type             //
    );
    llvm::DIDerivedType *const msg_field = DIB->createMemberType(             //
        file, "msg", file, 0, ptr_size_bits, ptr_align_bits,                  //
        u32_size_bits + u32_size_bits, llvm::DINode::FlagZero, str_debug_type //
    );

    error_type = DIB->createStructType(                                                          //
        file, "flint.err", file, 0, struct_size_bits, struct_align_bits, llvm::DINode::FlagZero, //
        nullptr, DIB->getOrCreateArray({type_field, value_field, msg_field}), 0, nullptr,        //
        "type.flint.err"                                                                         //
    );

    return error_type;
}

llvm::DIType *Generator::Debug::create_debug_type_func(llvm::Module *const module, const std::shared_ptr<Type> &type) {
    const FuncNode *func_node = type->as<FuncType>()->func_node;
    if (debug_files.find(func_node->file_hash) == debug_files.end()) {
        const auto &path = func_node->file_hash.path;
        if (!path.empty()) {
            debug_files[func_node->file_hash] = DIB->createFile(path.filename().string(), path.parent_path().string());
        }
    }
    llvm::DIFile *const file_meta = debug_files.at(func_node->file_hash);

    std::vector<const ObjectNode *> objects = Parser::get_all_objects();
    for (auto it = objects.begin(); it != objects.end();) {
        const auto &func_modules = (*it)->func_modules;
        if (std::find(func_modules.begin(), func_modules.end(), func_node) == func_modules.end()) {
            it = objects.erase(it);
        } else {
            ++it;
        }
    }

    struct ObjectInfo {
        std::string name;
        std::shared_ptr<Type> type_ptr;
        uint32_t type_id;
    };
    std::vector<ObjectInfo> object_infos;
    for (const ObjectNode *node : objects) {
        const Namespace *ns = Resolver::get_namespace_from_hash(node->file_hash);
        const std::shared_ptr<Type> object_type = ns->get_type_from_str(node->name).value();
        object_infos.push_back({node->name, object_type, object_type->get_id()});
    }

    const std::string llvm_type_str = type->get_type_string();
    ASSERT(type_map.find(llvm_type_str) != type_map.end());
    llvm::StructType *const llvm_struct = type_map.at(llvm_type_str);
    const uint64_t struct_size_bits = Allocation::get_type_size(module, llvm_struct) * 8;
    const uint32_t struct_align_bits = Allocation::calculate_type_alignment(llvm_struct) * 8;

    llvm::Type *const ptr_llvm = PTR_TY;
    const uint64_t ptr_size_bits = Allocation::get_type_size(module, ptr_llvm) * 8;
    const uint32_t ptr_align_bits = Allocation::calculate_type_alignment(ptr_llvm) * 8;

    llvm::Type *const i32_llvm = llvm::Type::getInt32Ty(context);
    const uint64_t i32_size_bits = Allocation::get_type_size(module, i32_llvm) * 8;
    const uint32_t i32_align_bits = Allocation::calculate_type_alignment(i32_llvm) * 8;

    llvm::DIType *const u32_type = DIB->createBasicType("u32", 32, llvm::dwarf::DW_ATE_unsigned);

    std::vector<llvm::Metadata *> enumerators;
    for (const ObjectInfo &info : object_infos) {
        enumerators.push_back(DIB->createEnumerator(info.name, static_cast<int64_t>(info.type_id)));
    }
    const llvm::DINodeArray enum_elements = DIB->getOrCreateArray(enumerators);
    llvm::DIType *const object_enum = DIB->createEnumerationType(                                //
        file_meta, func_node->name + ".object_types", file_meta, func_node->line, i32_size_bits, //
        i32_align_bits, enum_elements, u32_type, 0, type->get_type_string() + ".object_types"    //
    );

    llvm::DICompositeType *const dima_type = DIB->createStructType(                                                              //
        file_meta, func_node->name + ".dima_head", file_meta, func_node->line, ptr_size_bits + i32_size_bits, ptr_align_bits,    //
        llvm::DINode::FlagZero, nullptr,                                                                                         //
        DIB->getOrCreateArray({DIB->createMemberType(                                                                            //
            file_meta, "type", file_meta, func_node->line, i32_size_bits, i32_align_bits, ptr_size_bits, llvm::DINode::FlagZero, //
            object_enum)}),                                                                                                      //
        0, nullptr, type->get_type_string() + ".dima_head"                                                                       //
    );
    // The func struct stores &global_var in its dima_head field (offset 16).
    // The global var *contains* the actual dima_head address (e.g. after init_heads).
    // To reach type_id = *(*(func+16) + 8), we chain two references:
    //   outer ref → wrapper struct at &global_var → inner ref → dima type at dima_head_addr
    llvm::DIDerivedType *const inner_ref = DIB->createReferenceType(                 //
        llvm::dwarf::DW_TAG_reference_type, dima_type, ptr_size_bits, ptr_align_bits //
    );
    llvm::DICompositeType *const wrapper_type = DIB->createStructType(                                                          //
        file_meta, func_node->name + ".dima_wrapper", file_meta, func_node->line, ptr_size_bits, ptr_align_bits,                //
        llvm::DINode::FlagZero, nullptr,                                                                                        //
        DIB->getOrCreateArray({DIB->createMemberType(                                                                           //
            file_meta, "ptr", file_meta, func_node->line, ptr_size_bits, ptr_align_bits, 0, llvm::DINode::FlagZero, inner_ref)} //
            ),                                                                                                                  //
        0, nullptr, type->get_type_string() + ".dima_wrapper"                                                                   //
    );
    llvm::DIDerivedType *const outer_ref = DIB->createReferenceType(                    //
        llvm::dwarf::DW_TAG_reference_type, wrapper_type, ptr_size_bits, ptr_align_bits //
    );

    std::vector<llvm::Metadata *> union_members;
    for (const ObjectInfo &info : object_infos) {
        llvm::DIType *const object_dt = get_or_create_debug_type(module, info.type_ptr);
        union_members.push_back(DIB->createMemberType(                                                                  //
            file_meta, info.name, file_meta, func_node->line, ptr_size_bits, ptr_align_bits, 0, llvm::DINode::FlagZero, //
            object_dt)                                                                                                  //
        );
    }
    const llvm::DINodeArray union_elements = DIB->getOrCreateArray(union_members);
    llvm::DICompositeType *const object_union = DIB->createUnionType(                  //
        file_meta, "union", file_meta, func_node->line, ptr_size_bits, ptr_align_bits, //
        llvm::DINode::FlagZero, union_elements, 0, type->get_type_string() + ".object" //
    );

    std::vector<llvm::Metadata *> func_members;
    func_members.push_back(DIB->createMemberType(                             //
        file_meta, "active_type", file_meta, func_node->line, ptr_size_bits,  //
        ptr_align_bits, ptr_size_bits * 2, llvm::DINode::FlagZero, outer_ref) //
    );
    func_members.push_back(DIB->createMemberType(                                                                                //
        file_meta, "object", file_meta, func_node->line, ptr_size_bits, ptr_align_bits, 0, llvm::DINode::FlagZero, object_union) //
    );
    const llvm::DINodeArray func_elements = DIB->getOrCreateArray(func_members);
    return DIB->createStructType(                                                                    //
        file_meta, func_node->name, file_meta, func_node->line, struct_size_bits, struct_align_bits, //
        llvm::DINode::FlagZero, nullptr, func_elements, 0, nullptr, type->get_type_string()          //
    );
}

llvm::DIType *Generator::Debug::create_debug_type_fn(llvm::Module *const module) {
    static llvm::DIType *callable_type = nullptr;
    if (callable_type != nullptr) {
        return callable_type;
    }

    std::vector<const FunctionNode *> functions = Parser::get_all_functions();
    for (auto it = functions.begin(); it != functions.end();) {
        if ((*it)->scope.has_value()) {
            ++it;
        } else {
            it = functions.erase(it);
        }
    }
    ASSERT(!functions.empty());

    const uint64_t ptr_size_bits = Allocation::get_type_size(module, PTR_TY) * 8;
    const uint32_t ptr_align_bits = Allocation::calculate_type_alignment(PTR_TY) * 8;
    const uint64_t u64_size_bits = 64;
    const uint32_t u64_align_bits = 64;
    const uint64_t error_size_bits = 128;
    // Layout: [0] fn_ptr, [8] thread_stack, [16] fn_id, [24] err (16B), [40] rets, ...
    const uint64_t header_size_bits = ptr_size_bits + ptr_size_bits + u64_size_bits + error_size_bits;

    const FunctionNode *first_fn = functions.front();
    if (debug_files.find(first_fn->file_hash) == debug_files.end()) {
        const auto &path = first_fn->file_hash.path;
        if (!path.empty()) {
            debug_files[first_fn->file_hash] = DIB->createFile(path.filename().string(), path.parent_path().string());
        }
    }
    llvm::DIFile *const file_meta = debug_files.at(first_fn->file_hash);

    llvm::DIType *const u64_type = DIB->createBasicType("u64", 64, llvm::dwarf::DW_ATE_unsigned);
    llvm::DIDerivedType *const discriminator = DIB->createMemberType(                 //
        file_meta, "fn_id", file_meta, first_fn->line, u64_size_bits, u64_align_bits, //
        ptr_size_bits * 2, llvm::DINode::FlagArtificial, u64_type                     //
    );

    std::vector<llvm::Metadata *> variants;
    uint64_t max_frame_size_bits = header_size_bits;

    for (const FunctionNode *fn : functions) {
        const size_t fn_id = fn->get_id();
        if (debug_files.find(fn->file_hash) == debug_files.end()) {
            const auto &path = fn->file_hash.path;
            if (!path.empty()) {
                debug_files[fn->file_hash] = DIB->createFile(path.filename().string(), path.parent_path().string());
            }
        }
        llvm::DIFile *const fn_file_meta = debug_files.at(fn->file_hash);
        llvm::ConstantInt *const fn_id_value = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), fn_id);
        std::vector<llvm::Metadata *> members;

        std::vector<llvm::Metadata *> return_members;
        uint64_t return_size = 0;
        for (const auto &return_type : fn->return_types) {
            llvm::DIType *const return_debug_type = get_or_create_debug_type(module, return_type);
            const size_t size_bits = return_debug_type->getSizeInBits();
            const size_t align_bits = return_debug_type->getAlignInBits();
            llvm::DIDerivedType *const return_member = DIB->createMemberType(              //
                fn_file_meta, return_type->to_string(), fn_file_meta, fn->line, size_bits, //
                align_bits, return_size, llvm::DINode::FlagZero, return_debug_type         //
            );
            return_members.emplace_back(return_member);
            return_size += size_bits;
        }
        if (!return_members.empty()) {
            llvm::DICompositeType *const return_struct = DIB->createStructType(                             //
                fn_file_meta, "", fn_file_meta, fn->line, return_size, 64, llvm::DINode::FlagZero, nullptr, //
                DIB->getOrCreateArray(return_members), 0, nullptr,                                          //
                fn->file_hash.to_string() + "." + fn->name + ".rets"                                        //
            );
            llvm::DIDerivedType *const return_member = DIB->createMemberType(                    //
                fn_file_meta, "rets", fn_file_meta, fn->line, return_size, 64, header_size_bits, //
                llvm::DINode::FlagZero, return_struct                                            //
            );
            members.emplace_back(return_member);
        }

        std::vector<llvm::Metadata *> parameter_members;
        uint64_t parameter_size = 0;
        for (const auto &[param_type, param_name, param_mutable] : fn->parameters) {
            llvm::DIType *const param_debug_type = get_or_create_debug_type(module, param_type);
            const size_t size_bits = param_debug_type->getSizeInBits();
            const size_t align_bits = param_debug_type->getAlignInBits();
            llvm::DIDerivedType *const param_member = DIB->createMemberType(         //
                fn_file_meta, param_name, fn_file_meta, fn->line, size_bits,         //
                align_bits, parameter_size, llvm::DINode::FlagZero, param_debug_type //
            );
            parameter_members.emplace_back(param_member);
            parameter_size += size_bits;
        }
        if (!parameter_members.empty()) {
            llvm::DICompositeType *const parameter_struct = DIB->createStructType(                             //
                fn_file_meta, "", fn_file_meta, fn->line, parameter_size, 64, llvm::DINode::FlagZero, nullptr, //
                DIB->getOrCreateArray(parameter_members), 0, nullptr,                                          //
                fn->file_hash.to_string() + "." + fn->name + ".args"                                           //
            );
            llvm::DIDerivedType *const parameter_member = DIB->createMemberType(         //
                fn_file_meta, "args", fn_file_meta, fn->line, parameter_size, 64,        //
                header_size_bits + return_size, llvm::DINode::FlagZero, parameter_struct //
            );
            members.emplace_back(parameter_member);
        }

        std::vector<llvm::Metadata *> local_members;
        uint64_t local_size = 0;
        for (const auto &[variable_name, variable] : fn->scope.value()->get_all_variables()) {
            if (!variable.is_persistent) {
                continue;
            }
            llvm::DIType *const local_debug_type = get_or_create_debug_type(module, variable.type);
            const size_t size_bits = local_debug_type->getSizeInBits();
            const size_t align_bits = local_debug_type->getAlignInBits();
            llvm::DIDerivedType *const local_member = DIB->createMemberType(     //
                fn_file_meta, variable_name, fn_file_meta, fn->line, size_bits,  //
                align_bits, local_size, llvm::DINode::FlagZero, local_debug_type //
            );
            local_members.emplace_back(local_member);
            local_size += size_bits;
        }
        if (!local_members.empty()) {
            llvm::DICompositeType *const local_struct = DIB->createStructType(                             //
                fn_file_meta, "", fn_file_meta, fn->line, local_size, 64, llvm::DINode::FlagZero, nullptr, //
                DIB->getOrCreateArray(local_members), 0, nullptr,                                          //
                fn->file_hash.to_string() + "." + fn->name + ".locals"                                     //
            );
            llvm::DIDerivedType *const local_member = DIB->createMemberType(                          //
                fn_file_meta, "locals", fn_file_meta, fn->line, local_size, 64,                       //
                header_size_bits + return_size + parameter_size, llvm::DINode::FlagZero, local_struct //
            );
            members.emplace_back(local_member);
        }

        const uint64_t frame_size = header_size_bits + return_size + parameter_size + local_size;
        max_frame_size_bits = std::max(max_frame_size_bits, frame_size);
        const llvm::DINodeArray members_array = DIB->getOrCreateArray(members);
        llvm::DICompositeType *const frame_type = DIB->createStructType(                              //
            fn_file_meta, "", fn_file_meta, fn->line, frame_size, 64, llvm::DINode::FlagZero,         //
            nullptr, members_array, 0, nullptr, fn->file_hash.to_string() + "." + fn->name + ".frame" //
        );
        llvm::DIDerivedType *const fn_variation = DIB->createVariantMemberType(                                                //
            fn_file_meta, fn->name, fn_file_meta, fn->line, frame_size, 64, 0, fn_id_value, llvm::DINode::FlagZero, frame_type //
        );
        variants.emplace_back(fn_variation);
    }

    llvm::DICompositeType *const variant_part_type = DIB->createVariantPart(                         //
        file_meta, "fn", file_meta, first_fn->line, max_frame_size_bits, 64, llvm::DINode::FlagZero, //
        discriminator, DIB->getOrCreateArray(variants), "fn.callable"                                //
    );
    llvm::DICompositeType *const callable_struct_type = DIB->createStructType(                       //
        file_meta, "fn", file_meta, first_fn->line, max_frame_size_bits, 64, llvm::DINode::FlagZero, //
        nullptr, DIB->getOrCreateArray({variant_part_type}), 0, nullptr, "fn.callable"               //
    );
    callable_type = DIB->createReferenceType(                                                   //
        llvm::dwarf::DW_TAG_reference_type, callable_struct_type, ptr_size_bits, ptr_align_bits //
    );
    return callable_type;
}

llvm::DIType *Generator::Debug::create_debug_type_interface(llvm::Module *const module, const std::shared_ptr<Type> &type) {
    const InterfaceNode *interface_node = type->as<InterfaceType>()->interface_node;
    if (debug_files.find(interface_node->file_hash) == debug_files.end()) {
        const auto &path = interface_node->file_hash.path;
        if (!path.empty()) {
            debug_files[interface_node->file_hash] = DIB->createFile(path.filename().string(), path.parent_path().string());
        }
    }
    llvm::DIFile *const file_meta = debug_files.at(interface_node->file_hash);

    std::vector<const ObjectNode *> objects = Parser::get_all_objects();
    for (auto it = objects.begin(); it != objects.end();) {
        const auto &interfaces = (*it)->interfaces;
        if (interfaces.find(interface_node->name) == interfaces.end()) {
            it = objects.erase(it);
        } else {
            ++it;
        }
    }

    struct ObjectInfo {
        std::string name;
        std::shared_ptr<Type> type_ptr;
        uint32_t type_id;
    };
    std::vector<ObjectInfo> object_infos;
    for (const ObjectNode *node : objects) {
        const Namespace *ns = Resolver::get_namespace_from_hash(node->file_hash);
        const std::shared_ptr<Type> object_type = ns->get_type_from_str(node->name).value();
        object_infos.push_back({node->name, object_type, object_type->get_id()});
    }

    const std::string llvm_type_str = type->get_type_string();
    ASSERT(type_map.find(llvm_type_str) != type_map.end());
    llvm::StructType *const llvm_struct = type_map.at(llvm_type_str);
    const uint64_t struct_size_bits = Allocation::get_type_size(module, llvm_struct) * 8;
    const uint32_t struct_align_bits = Allocation::calculate_type_alignment(llvm_struct) * 8;

    llvm::Type *const ptr_llvm = PTR_TY;
    const uint64_t ptr_size_bits = Allocation::get_type_size(module, ptr_llvm) * 8;
    const uint32_t ptr_align_bits = Allocation::calculate_type_alignment(ptr_llvm) * 8;

    llvm::Type *const i32_llvm = llvm::Type::getInt32Ty(context);
    const uint64_t i32_size_bits = Allocation::get_type_size(module, i32_llvm) * 8;
    const uint32_t i32_align_bits = Allocation::calculate_type_alignment(i32_llvm) * 8;

    llvm::DIType *const u32_type = DIB->createBasicType("u32", 32, llvm::dwarf::DW_ATE_unsigned);

    std::vector<llvm::Metadata *> enumerators;
    for (const ObjectInfo &info : object_infos) {
        enumerators.push_back(DIB->createEnumerator(info.name, static_cast<int64_t>(info.type_id)));
    }
    const llvm::DINodeArray enum_elements = DIB->getOrCreateArray(enumerators);
    llvm::DIType *const object_enum = DIB->createEnumerationType(                                          //
        file_meta, interface_node->name + ".object_types", file_meta, interface_node->line, i32_size_bits, //
        i32_align_bits, enum_elements, u32_type, 0, type->get_type_string() + ".object_types"              //
    );

    llvm::DICompositeType *const dima_type = DIB->createStructType(                                                                     //
        file_meta, interface_node->name + ".dima_head", file_meta, interface_node->line, ptr_size_bits + i32_size_bits, ptr_align_bits, //
        llvm::DINode::FlagZero, nullptr,                                                                                                //
        DIB->getOrCreateArray({DIB->createMemberType(                                                                                   //
            file_meta, "type", file_meta, interface_node->line, i32_size_bits, i32_align_bits, ptr_size_bits, llvm::DINode::FlagZero,   //
            object_enum)}),                                                                                                             //
        0, nullptr, type->get_type_string() + ".dima_head"                                                                              //
    );
    // The interface struct stores &global_var in its dima_head field (offset 16).
    // The global var *contains* the actual dima_head address (e.g. after init_heads).
    // To reach type_id = *(*(interface+16) + 8), we chain two references:
    //   outer ref → wrapper struct at &global_var → inner ref → dima type at dima_head_addr
    llvm::DIDerivedType *const inner_ref = DIB->createReferenceType(                 //
        llvm::dwarf::DW_TAG_reference_type, dima_type, ptr_size_bits, ptr_align_bits //
    );
    llvm::DICompositeType *const wrapper_type = DIB->createStructType(                                                               //
        file_meta, interface_node->name + ".dima_wrapper", file_meta, interface_node->line, ptr_size_bits, ptr_align_bits,           //
        llvm::DINode::FlagZero, nullptr,                                                                                             //
        DIB->getOrCreateArray({DIB->createMemberType(                                                                                //
            file_meta, "ptr", file_meta, interface_node->line, ptr_size_bits, ptr_align_bits, 0, llvm::DINode::FlagZero, inner_ref)} //
            ),                                                                                                                       //
        0, nullptr, type->get_type_string() + ".dima_wrapper"                                                                        //
    );
    llvm::DIDerivedType *const outer_ref = DIB->createReferenceType(                    //
        llvm::dwarf::DW_TAG_reference_type, wrapper_type, ptr_size_bits, ptr_align_bits //
    );

    std::vector<llvm::Metadata *> union_members;
    for (const ObjectInfo &info : object_infos) {
        llvm::DIType *const object_dt = get_or_create_debug_type(module, info.type_ptr);
        union_members.push_back(DIB->createMemberType(                                                                       //
            file_meta, info.name, file_meta, interface_node->line, ptr_size_bits, ptr_align_bits, 0, llvm::DINode::FlagZero, //
            object_dt)                                                                                                       //
        );
    }
    const llvm::DINodeArray union_elements = DIB->getOrCreateArray(union_members);
    llvm::DICompositeType *const object_union = DIB->createUnionType(                       //
        file_meta, "union", file_meta, interface_node->line, ptr_size_bits, ptr_align_bits, //
        llvm::DINode::FlagZero, union_elements, 0, type->get_type_string() + ".object"      //
    );

    std::vector<llvm::Metadata *> interface_members;
    interface_members.push_back(DIB->createMemberType(                            //
        file_meta, "active_type", file_meta, interface_node->line, ptr_size_bits, //
        ptr_align_bits, ptr_size_bits * 2, llvm::DINode::FlagZero, outer_ref)     //
    );
    interface_members.push_back(DIB->createMemberType(                                                                                //
        file_meta, "object", file_meta, interface_node->line, ptr_size_bits, ptr_align_bits, 0, llvm::DINode::FlagZero, object_union) //
    );
    const llvm::DINodeArray interface_elements = DIB->getOrCreateArray(interface_members);
    return DIB->createStructType(                                                                              //
        file_meta, interface_node->name, file_meta, interface_node->line, struct_size_bits, struct_align_bits, //
        llvm::DINode::FlagZero, nullptr, interface_elements, 0, nullptr, type->get_type_string()               //
    );
}

llvm::DIType *Generator::Debug::create_debug_type_multi(llvm::Module *const module, const std::shared_ptr<Type> &type) {
    const auto *multi_type = type->as<MultiType>();
    const std::string &type_str = type->to_string();

    if (type_str == "bool8") {
        llvm::DIFile *const file = DCU->getFile();
        llvm::DIType *const u8_type = get_or_create_debug_type(module, Type::get_primitive_type("u8"));

        std::vector<llvm::Metadata *> members;
        for (unsigned i = 0; i < 8; i++) {
            const std::string field_name = "$" + std::to_string(i);
            llvm::DIDerivedType *const member = DIB->createMemberType(                  //
                file, field_name, file, 0, 1, 1, i, llvm::DINode::FlagBitField, u8_type //
            );
            members.emplace_back(member);
        }
        return DIB->createStructType(                                                                                          //
            file, "bool8", file, 0, 8, 8, llvm::DINode::FlagZero, nullptr, DIB->getOrCreateArray(members), 0, nullptr, "bool8" //
        );
    }

    llvm::Type *const llvm_type = IR::get_type(module, type).first;
    llvm::DIType *const elem_debug_type = get_or_create_debug_type(module, multi_type->base_type);
    const uint64_t size_bits = elem_debug_type->getSizeInBits() * multi_type->width;
    const uint32_t align_bits = Allocation::calculate_type_alignment(llvm_type) * 8;

    const llvm::DINodeArray array = DIB->getOrCreateArray({DIB->getOrCreateSubrange(0, multi_type->width)});
    return DIB->createVectorType(size_bits, align_bits, elem_debug_type, array);
}

llvm::DIType *Generator::Debug::create_debug_type_opaque(llvm::Module *const module) {
    const size_t ptr_size = module->getDataLayout().getPointerSizeInBits();
    const uint32_t ptr_align = Allocation::calculate_type_alignment(PTR_TY) * 8;
    return DIB->createPointerType(nullptr, ptr_size, ptr_align);
}

llvm::DIType *Generator::Debug::create_debug_type_optional(llvm::Module *const module, const std::shared_ptr<Type> &type) {
    const auto *optional_type = type->as<OptionalType>();

    ASSERT(DCU != nullptr);
    llvm::DIFile *const file = DCU->getFile();

    llvm::DIType *const value_type = get_or_create_debug_type(module, optional_type->base_type);

    const std::string opt_str = type->get_type_string();
    ASSERT(type_map.find(opt_str) != type_map.end());
    llvm::StructType *const llvm_struct = type_map.at(opt_str);
    const uint64_t struct_size_bits = Allocation::get_type_size(module, llvm_struct) * 8;
    const uint32_t struct_align_bits = Allocation::calculate_type_alignment(llvm_struct) * 8;

    const uint64_t value_offset_bits = module->getDataLayout().getStructLayout(llvm_struct)->getElementOffset(1) * 8;

    llvm::DIType *const bool_type = DIB->createBasicType("bool", 8, llvm::dwarf::DW_ATE_boolean);
    llvm::DIDerivedType *const discriminator = DIB->createMemberType(                //
        file, "has_value", file, 0, 1, 1, 0, llvm::DINode::FlagArtificial, bool_type //
    );

    llvm::DICompositeType *const none_frame = DIB->createStructType(                   //
        file, "", file, 0, static_cast<size_t>(0), 1, llvm::DINode::FlagZero, nullptr, //
        DIB->getOrCreateArray({}), 0, nullptr, type->to_string() + ".none"             //
    );
    llvm::DIDerivedType *const none_variant = DIB->createVariantMemberType(                                      //
        file, "none", file, 0, 0, 1, 0, llvm::ConstantInt::getFalse(context), llvm::DINode::FlagZero, none_frame //
    );

    const uint64_t value_size = value_type->getSizeInBits();
    const uint32_t value_align = value_type->getAlignInBits();
    llvm::DIDerivedType *const some_variant = DIB->createVariantMemberType(     //
        file, "value", file, 0, value_size, value_align, value_offset_bits,     //
        llvm::ConstantInt::getTrue(context), llvm::DINode::FlagZero, value_type //
    );

    llvm::DICompositeType *const variant_part = DIB->createVariantPart(                                          //
        file, type->to_string(), file, 0, struct_size_bits, struct_align_bits, llvm::DINode::FlagZero,           //
        discriminator, DIB->getOrCreateArray({none_variant, some_variant}), type->get_type_string() + ".variant" //
    );
    return DIB->createStructType(                                                                      //
        file, type->to_string(), file, 0, struct_size_bits, struct_align_bits, llvm::DINode::FlagZero, //
        nullptr, DIB->getOrCreateArray({variant_part}), 0, nullptr, type->get_type_string()            //
    );
}

llvm::DIType *Generator::Debug::create_debug_type_primitive(llvm::Module *const module, const std::shared_ptr<Type> &type) {
    const std::string &type_str = type->to_string();
    uint64_t size_bits = 64;
    unsigned encoding = llvm::dwarf::DW_ATE_signed;
    if (type_str == "u8") {
        size_bits = 8;
        encoding = llvm::dwarf::DW_ATE_unsigned;
    } else if (type_str == "i8") {
        size_bits = 8;
        encoding = llvm::dwarf::DW_ATE_signed;
    } else if (type_str == "u16") {
        size_bits = 16;
        encoding = llvm::dwarf::DW_ATE_unsigned;
    } else if (type_str == "i16") {
        size_bits = 16;
        encoding = llvm::dwarf::DW_ATE_signed;
    } else if (type_str == "u32") {
        size_bits = 32;
        encoding = llvm::dwarf::DW_ATE_unsigned;
    } else if (type_str == "i32") {
        size_bits = 32;
        encoding = llvm::dwarf::DW_ATE_signed;
    } else if (type_str == "u64") {
        size_bits = 64;
        encoding = llvm::dwarf::DW_ATE_unsigned;
    } else if (type_str == "i64") {
        size_bits = 64;
        encoding = llvm::dwarf::DW_ATE_signed;
    } else if (type_str == "f32") {
        size_bits = 32;
        encoding = llvm::dwarf::DW_ATE_float;
    } else if (type_str == "f64") {
        size_bits = 64;
        encoding = llvm::dwarf::DW_ATE_float;
    } else if (type_str == "bool") {
        size_bits = 1;
        encoding = llvm::dwarf::DW_ATE_boolean;
    } else if (type_str == "str") {
        llvm::DIType *const u64_type = DIB->createBasicType("u64", 64, llvm::dwarf::DW_ATE_unsigned);
        llvm::DIType *const char_type = DIB->createBasicType("char", 8, llvm::dwarf::DW_ATE_signed_char);

        std::vector<uint64_t> count_ops = {llvm::dwarf::DW_OP_push_object_address, llvm::dwarf::DW_OP_constu, 8, llvm::dwarf::DW_OP_minus,
            llvm::dwarf::DW_OP_deref_size, 8};
        llvm::DIExpression *const count_expr = llvm::DIExpression::get(context, count_ops);
        llvm::DISubrange *const data_sub = DIB->getOrCreateSubrange(0, count_expr);
        llvm::DICompositeType *const data_arr = DIB->createArrayType(0, 8, char_type, DIB->getOrCreateArray({data_sub}));

        llvm::DIFile *const file = DCU->getFile();

        llvm::DIDerivedType *const len_member = DIB->createMemberType(file, "len", file, 0, 64, 64, 0, llvm::DINode::FlagZero, u64_type);
        llvm::DIDerivedType *const data_member = DIB->createMemberType(file, "data", file, 0, 0, 8, 64, llvm::DINode::FlagZero, data_arr);

        const llvm::DINodeArray elements = DIB->getOrCreateArray({len_member, data_member});
        llvm::DICompositeType *const str_struct =
            DIB->createStructType(file, "str", file, 0, 64, 64, llvm::DINode::FlagZero, nullptr, elements, 0, nullptr, "type.str.content");

        const size_t PTR_SIZE = module->getDataLayout().getPointerSizeInBits();
        return DIB->createPointerType(str_struct, PTR_SIZE, 0);
    } else if (type_str == "void") {
        return DIB->createUnspecifiedType("void");
    } else if (type_str == "anyerror") {
        return create_debug_type_error(module);
    }
    return DIB->createBasicType(type_str, size_bits, encoding);
}

llvm::DIType *Generator::Debug::create_debug_type_tuple(llvm::Module *const module, const std::shared_ptr<Type> &type) {
    const auto *tuple_type = type->as<TupleType>();

    llvm::DIFile *const file = DCU->getFile();
    const std::string llvm_type_str = type->get_type_string();
    llvm::StructType *const llvm_struct = type_map.at(llvm_type_str);

    const uint64_t struct_size_bits = Allocation::get_type_size(module, llvm_struct) * 8;
    const uint32_t struct_align_bits = Allocation::calculate_type_alignment(llvm_struct) * 8;

    std::vector<llvm::Metadata *> members;
    uint64_t offset_bits = 0;
    for (size_t i = 0; i < tuple_type->types.size(); ++i) {
        const std::string field_name = "$" + std::to_string(i);
        llvm::DIType *const field_debug_type = get_or_create_debug_type(module, tuple_type->types[i]);

        llvm::Type *const field_llvm_type = llvm_struct->getElementType(i);
        uint64_t const field_size_bits = Allocation::get_type_size(module, field_llvm_type) * 8;
        uint32_t const field_align_bits = Allocation::calculate_type_alignment(field_llvm_type) * 8;

        if (offset_bits % field_align_bits != 0) {
            offset_bits += field_align_bits - (offset_bits % field_align_bits);
        }

        llvm::DIDerivedType *const member = DIB->createMemberType(                                                              //
            file, field_name, file, 0, field_size_bits, field_align_bits, offset_bits, llvm::DINode::FlagZero, field_debug_type //
        );
        members.push_back(member);
        offset_bits += field_size_bits;
    }

    const llvm::DINodeArray elements = DIB->getOrCreateArray(members);
    return DIB->createStructType(                                              //
        file, type->to_string(), file, 0, struct_size_bits, struct_align_bits, //
        llvm::DINode::FlagZero, nullptr, elements, 0, nullptr, llvm_type_str   //
    );
}

llvm::DIType *Generator::Debug::create_debug_type_variant(llvm::Module *const module, const std::shared_ptr<Type> &type) {
    const auto *variant_type = type->as<VariantType>();
    if (variant_type->is_err_variant) {
        return create_debug_type_error(module);
    }

    llvm::DIFile *const file = DCU->getFile();
    const std::string llvm_type_str = type->get_type_string();
    llvm::StructType *const llvm_struct = type_map.at(llvm_type_str);

    llvm::DIType *const u32_type = DIB->createBasicType("u8", 8, llvm::dwarf::DW_ATE_unsigned);
    llvm::DIDerivedType *const discriminator = DIB->createMemberType(                          //
        file, "tag." + llvm_type_str, file, 0, 8, 8, 0, llvm::DINode::FlagArtificial, u32_type //
    );

    std::vector<llvm::Metadata *> variants;
    llvm::Type *const payload_type = llvm_struct->getElementType(1);
    const uint32_t payload_align = Allocation::calculate_type_alignment(payload_type);
    const uint64_t union_offset_bits = payload_align * 8;
    for (const auto &[tag, variant_value_type] : variant_type->get_possible_types()) {
        const std::string variant_name = tag.has_value() ? tag.value() : variant_value_type->to_string();
        const size_t discriminator_value = tag.has_value()      //
            ? variant_type->get_idx_of_tag(tag.value()).value() //
            : variant_type->get_idx_of_type(variant_value_type).value();
        llvm::ConstantInt *const discriminator_index = llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), discriminator_value);

        llvm::DIType *value_debug_type;
        uint64_t value_size;
        uint32_t value_align;

        if (variant_value_type->to_string() == "void") {
            value_debug_type = DIB->createStructType(                                               //
                file, "", file, 0, static_cast<size_t>(0), 1, llvm::DINode::FlagZero, nullptr,      //
                DIB->getOrCreateArray({}), 0, nullptr, llvm_type_str + "." + variant_name + ".void" //
            );
            value_size = 0;
            value_align = 1;
        } else {
            value_debug_type = get_or_create_debug_type(module, variant_value_type);
            value_size = value_debug_type->getSizeInBits();
            value_align = value_debug_type->getAlignInBits();
        }
        llvm::DIDerivedType *const variant_member = DIB->createVariantMemberType(    //
            file, variant_name, file, 0, value_size, value_align, union_offset_bits, //
            discriminator_index, llvm::DINode::FlagZero, value_debug_type            //
        );
        variants.push_back(variant_member);
    }

    const uint64_t struct_size_bits = Allocation::get_type_size(module, llvm_struct) * 8;
    const uint32_t struct_align_bits = Allocation::calculate_type_alignment(llvm_struct) * 8;
    llvm::DICompositeType *const variant_part = DIB->createVariantPart(                                //
        file, type->to_string(), file, 0, struct_size_bits, struct_align_bits, llvm::DINode::FlagZero, //
        discriminator, DIB->getOrCreateArray(variants), type->get_type_string() + ".variant"           //
    );
    return DIB->createStructType(                                                                      //
        file, type->to_string(), file, 0, struct_size_bits, struct_align_bits, llvm::DINode::FlagZero, //
        nullptr, DIB->getOrCreateArray({variant_part}), 0, nullptr, type->get_type_string()            //
    );
}

void Generator::Debug::create_function_debug_info(llvm::Function *const function, const std::string &function_name, const ASTNode *node) {
    const Hash &hash = node->file_hash;
    if (Debug::debug_files.find(hash) == Debug::debug_files.end()) {
        const auto &path = node->file_hash.path;
        if (!path.empty()) {
            llvm::DIFile *const file_meta = Debug::DIB->createFile(path.filename().string(), path.parent_path().string());
            Debug::debug_files[hash] = file_meta;
            if (Debug::DCU == nullptr) {
                Debug::DCU = Debug::DIB->createCompileUnit(llvm::dwarf::DW_LANG_C_plus_plus, file_meta, "flintc", false, "", 0);
            }
        }
    }
    llvm::DIFile *const file_meta = Debug::debug_files[hash];
    if (Debug::DCU != nullptr && file_meta != nullptr) {
        auto *subroutine_type = Debug::DIB->createSubroutineType(Debug::DIB->getOrCreateTypeArray({}));
        auto *subprogram = Debug::DIB->createFunction( //
            file_meta,                                 //
            function_name,                             //
            function->getName(),                       //
            file_meta,                                 //
            node->line,                                //
            subroutine_type,                           //
            node->line,                                //
            llvm::DINode::FlagZero,                    //
            llvm::DISubprogram::SPFlagDefinition       //
        );
        function->setSubprogram(subprogram);
    }
}

llvm::DIType *Generator::Debug::get_or_create_debug_type(llvm::Module *const module, const std::shared_ptr<Type> &type) {
    const std::string &type_str = type->to_string();
    if (debug_types.find(type_str) != debug_types.end()) {
        return debug_types.at(type_str);
    }
    llvm::DIType *di_type = nullptr;

    // For types that can be self-referential, insert a temporary forward declaration placeholder into the cache before creating the
    // complete type. This breaks infinite recursion when a type references itself through a pointer/optional field. The placeholder is
    // replaced with the complete type via replaceAllUsesWith after creation.
    // Only data and callables can be self-referential. For example a callable with a local variable of it's own type
    llvm::DICompositeType *placeholder = nullptr;
    if (type->get_variation() == Type::Variation::DATA || type->get_variation() == Type::Variation::FN) {
        placeholder = DIB->createReplaceableCompositeType(                                  //
            llvm::dwarf::DW_TAG_structure_type, type->to_string(), nullptr, DCU->getFile(), //
            0, 0, 0, 0, llvm::DINode::FlagFwdDecl, type->get_type_string()                  //
        );
        debug_types[type_str] = placeholder;
    }

    switch (type->get_variation()) {
        case Type::Variation::ALIAS: {
            const std::shared_ptr<Type> alias_type = type->as<AliasType>()->type;
            return get_or_create_debug_type(module, alias_type);
        }
        case Type::Variation::ARRAY:
            di_type = create_debug_type_array(module, type);
            break;
        case Type::Variation::DATA:
            di_type = create_debug_type_data(module, type);
            break;
        case Type::Variation::OBJECT:
            di_type = create_debug_type_object(module, type);
            break;
        case Type::Variation::ENUM:
            di_type = create_debug_type_enum(module, type);
            break;
        case Type::Variation::ERROR_SET:
            di_type = create_debug_type_error(module);
            break;
        case Type::Variation::FUNC:
            di_type = create_debug_type_func(module, type);
            break;
        case Type::Variation::FN:
            di_type = create_debug_type_fn(module);
            break;
        case Type::Variation::GROUP:
            ASSERT(false, "Group types cannot be represented as debug types as groups cannot be stored anywhere");
        case Type::Variation::INTERFACE:
            di_type = create_debug_type_interface(module, type);
            break;
        case Type::Variation::MULTI:
            di_type = create_debug_type_multi(module, type);
            break;
        case Type::Variation::OPAQUE:
            di_type = create_debug_type_opaque(module);
            break;
        case Type::Variation::OPTIONAL:
            di_type = create_debug_type_optional(module, type);
            break;
        case Type::Variation::POINTER:
            ASSERT(false, "Pointer types cannot be represented as debug types as pointers cannot be stored anywhere");
        case Type::Variation::PRIMITIVE:
            di_type = create_debug_type_primitive(module, type);
            break;
        case Type::Variation::RANGE:
            ASSERT(false, "Range types cannot be represented as debug types as ranges cannot be stored anywhere");
        case Type::Variation::TUPLE:
            di_type = create_debug_type_tuple(module, type);
            break;
        case Type::Variation::UNKNOWN:
            ASSERT(false, "Unknown types cannot be represented as debug types");
        case Type::Variation::VARIANT:
            di_type = create_debug_type_variant(module, type);
            break;
    }

    if (placeholder != nullptr && di_type != nullptr) {
        placeholder->replaceAllUsesWith(di_type);
    }
    debug_types[type_str] = di_type;
    return di_type;
}

void Generator::Debug::generate_variable_debug_info(                        //
    llvm::IRBuilder<> &builder,                                             //
    llvm::Function *parent,                                                 //
    const std::shared_ptr<Scope> scope,                                     //
    const std::unordered_map<std::string, llvm::Value *const> &allocations, //
    const Hash &hash_key                                                    //
) {
    llvm::DISubprogram *const sp = parent->getSubprogram();
    if (sp == nullptr || DIB == nullptr) {
        return;
    }
    llvm::DIFile *const file_meta = debug_files.at(hash_key);
    if (!file_meta) {
        return;
    }

    for (const auto &stmt : scope->body) {
        switch (stmt->get_variation()) {
            case StatementNode::Variation::DECLARATION: {
                const auto *decl = stmt->as<DeclarationNode>();
                const std::string alloca_name = "s" + std::to_string(scope->scope_id) + "::" + decl->name;
                llvm::Value *const alloca = allocations.at(alloca_name);
                llvm::DIType *const debug_type = get_or_create_debug_type(parent->getParent(), decl->type);
                llvm::DILocalVariable *const var = DIB->createAutoVariable(sp, decl->name, file_meta, decl->line, debug_type, true);
                llvm::DILocation *const diloc = llvm::DILocation::get(Generator::context, sp->getLine(), 0, sp);
                llvm::DIExpression *const expr = DIB->createExpression({llvm::dwarf::DW_OP_deref});
                DIB->insertDbgValueIntrinsic(alloca, var, expr, diloc, builder.GetInsertBlock()->end());
                break;
            }
            case StatementNode::Variation::IF: {
                const auto *node = stmt->as<IfNode>();
                const IfNode *if_node = node;
                while (if_node != nullptr) {
                    generate_variable_debug_info(builder, parent, if_node->then_scope, allocations, hash_key);
                    if (!if_node->else_scope.has_value()) {
                        break;
                    }
                    if (std::holds_alternative<std::unique_ptr<IfNode>>(if_node->else_scope.value())) {
                        if_node = std::get<std::unique_ptr<IfNode>>(if_node->else_scope.value()).get();
                        continue;
                    }
                    generate_variable_debug_info(                                      //
                        builder, parent,                                               //
                        std::get<std::shared_ptr<Scope>>(if_node->else_scope.value()), //
                        allocations, hash_key                                          //
                    );
                    break;
                }
                break;
            }
            case StatementNode::Variation::WHILE: {
                const auto *node = stmt->as<WhileNode>();
                generate_variable_debug_info(builder, parent, node->scope, allocations, hash_key);
                break;
            }
            case StatementNode::Variation::DO_WHILE: {
                const auto *node = stmt->as<DoWhileNode>();
                generate_variable_debug_info(builder, parent, node->scope, allocations, hash_key);
                break;
            }
            case StatementNode::Variation::FOR_LOOP: {
                const auto *node = stmt->as<ForLoopNode>();
                if (node->definition_scope != nullptr) {
                    generate_variable_debug_info(builder, parent, node->definition_scope, allocations, hash_key);
                }
                generate_variable_debug_info(builder, parent, node->body, allocations, hash_key);
                break;
            }
            case StatementNode::Variation::ENHANCED_FOR_LOOP: {
                const auto *node = stmt->as<EnhForLoopNode>();
                if (node->definition_scope != nullptr) {
                    generate_variable_debug_info(builder, parent, node->definition_scope, allocations, hash_key);
                }
                generate_variable_debug_info(builder, parent, node->body, allocations, hash_key);
                break;
            }
            case StatementNode::Variation::CATCH: {
                const auto *node = stmt->as<CatchNode>();
                generate_variable_debug_info(builder, parent, node->scope, allocations, hash_key);
                break;
            }
            case StatementNode::Variation::SWITCH: {
                const auto *node = stmt->as<SwitchStatement>();
                for (const auto &branch : node->branches) {
                    generate_variable_debug_info(builder, parent, branch.body, allocations, hash_key);
                }
                break;
            }
            default:
                break;
        }
    }
}

void Generator::Debug::generate_parameter_debug_info(                       //
    llvm::IRBuilder<> &builder,                                             //
    llvm::Function *parent,                                                 //
    const FunctionNode *function_node,                                      //
    const std::unordered_map<std::string, llvm::Value *const> &allocations, //
    const Hash &hash_key                                                    //
) {
    llvm::DISubprogram *const sp = parent->getSubprogram();
    if (sp == nullptr || DIB == nullptr) {
        return;
    }
    llvm::DIFile *const file_meta = debug_files.at(hash_key);
    if (file_meta == nullptr) {
        return;
    }

    for (size_t i = 0; i < function_node->parameters.size(); i++) {
        const auto &[param_type, param_name, param_mutable] = function_node->parameters.at(i);
        const std::string alloca_name = "s" + std::to_string(function_node->scope.value()->scope_id) + "::" + param_name;
        llvm::Value *const alloca = allocations.at(alloca_name);
        llvm::DIType *const debug_type = get_or_create_debug_type(parent->getParent(), param_type);
        llvm::DILocalVariable *const var = DIB->createParameterVariable(            //
            sp, param_name, i + 1, file_meta, function_node->line, debug_type, true //
        );
        llvm::DILocation *const diloc = llvm::DILocation::get(Generator::context, sp->getLine(), 0, sp);
        llvm::DIExpression *const expr = DIB->createExpression({llvm::dwarf::DW_OP_deref});
        DIB->insertDbgValueIntrinsic(alloca, var, expr, diloc, builder.GetInsertBlock()->end());
    }
}
