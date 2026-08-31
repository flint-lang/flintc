#include "generator/generator.hpp"

#include "error/error.hpp"
#include "error/error_type.hpp"
#include "parser/ast/definitions/data_node.hpp"
#include "parser/ast/definitions/enum_node.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/ast/namespace.hpp"
#include "parser/parser.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/enum_type.hpp"
#include "parser/type/group_type.hpp"
#include "parser/type/primitive_type.hpp"
#include "parser/type/vector_type.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

struct HeaderCollect {
    std::vector<const DataType *> data_order;
    std::vector<const EnumType *> enum_order;
    std::vector<const VectorType *> vector_order;
    std::unordered_set<std::string> seen_datas;
    std::unordered_set<std::string> seen_enums;
    std::unordered_set<std::string> seen_vectors;
};

static std::string c_identifier(const std::string &name) {
    std::string result = name;
    std::replace_if(result.begin(), result.end(), [](char c) { return c == '.'; }, '_');
    return result;
}

static std::optional<std::string> header_c_type(const std::string &lib, const std::shared_ptr<Type> &type, bool is_mutable) {
    switch (type->get_variation()) {
        case Type::Variation::DATA: {
            const std::string name = lib + "_" + c_identifier(type->as<DataType>()->data_node->name);
            return is_mutable ? name + " *" : name;
        }
        case Type::Variation::ENUM:
            return c_identifier(type->as<EnumType>()->enum_node->name);
        case Type::Variation::PRIMITIVE: {
            const std::string type_name = type->as<PrimitiveType>()->type_name;
            if (type_name == "u8") {
                return "uint8_t";
            }
            if (type_name == "i8") {
                return "int8_t";
            }
            if (type_name == "u16") {
                return "uint16_t";
            }
            if (type_name == "i16") {
                return "int16_t";
            }
            if (type_name == "u32") {
                return "uint32_t";
            }
            if (type_name == "i32") {
                return "int32_t";
            }
            if (type_name == "u64") {
                return "uint64_t";
            }
            if (type_name == "i64") {
                return "int64_t";
            }
            if (type_name == "f32") {
                return "float";
            }
            if (type_name == "f64") {
                return "double";
            }
            if (type_name == "bool") {
                return "bool";
            }
            if (type_name == "str") {
                return "char *";
            }
            if (type_name == "void") {
                return "void";
            }
            return std::nullopt;
        }
        case Type::Variation::OPTIONAL: {
            const auto *opt = type->as<OptionalType>();
            return "FLINT_OPT(" + header_c_type(lib, opt->base_type, is_mutable).value() + ")";
        }
        case Type::Variation::VECTOR:
            return type->to_string();
        default:
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            return "";
    }
}

static bool header_collect_type(HeaderCollect &ctx, const std::shared_ptr<Type> &type) {
    switch (type->get_variation()) {
        case Type::Variation::DATA: {
            const auto *data = type->as<DataType>();
            const std::string key = data->get_type_string();
            if (ctx.seen_datas.count(key) != 0) {
                return true;
            }
            ctx.seen_datas.insert(key);
            ctx.data_order.push_back(data);
            for (const auto &field : data->data_node->fields) {
                if (!header_collect_type(ctx, field.type)) {
                    return false;
                }
            }
            return true;
        }
        case Type::Variation::ENUM: {
            const auto *enum_type = type->as<EnumType>();
            const std::string key = enum_type->get_type_string();
            if (ctx.seen_enums.count(key) == 0) {
                ctx.seen_enums.insert(key);
                ctx.enum_order.push_back(enum_type);
            }
            return true;
        }
        case Type::Variation::GROUP: {
            const auto *group = type->as<GroupType>();
            for (const auto &sub : group->types) {
                if (!header_collect_type(ctx, sub)) {
                    return false;
                }
            }
            return true;
        }
        case Type::Variation::OPTIONAL: {
            const auto *opt = type->as<OptionalType>();
            if (!header_collect_type(ctx, opt->base_type)) {
                return false;
            }
            return true;
        }
        case Type::Variation::PRIMITIVE:
            return header_c_type("", type, false).has_value();
        case Type::Variation::VECTOR: {
            const auto *vector_type = type->as<VectorType>();
            const std::string key = vector_type->to_string();
            if (ctx.seen_vectors.count(key) == 0) {
                ctx.seen_vectors.insert(key);
                ctx.vector_order.push_back(vector_type);
            }
            return true;
        }
        default:
            // Arrays, optionals, refs, fn pointers, etc. — not in the exported C ABI yet
            THROW_BASIC_ERR(ERR_NOT_IMPLEMENTED_YET);
            return false;
    }
    return true;
}

static void header_emit_types(const std::string &lib, std::ofstream &out, const HeaderCollect &ctx) {
    for (const auto *vec : ctx.vector_order) {
        const std::string base = header_c_type(lib, vec->base_type, false).value();
        out << "#ifndef FLINT_" << vec->to_string() << "\n";
        out << "#define FLINT_" << vec->to_string() << "\n";
        out << "typedef struct " << vec->to_string() << " { ";
        for (size_t i = 0; i < vec->width; i++) {
            if (i > 0) {
                out << " ";
            }
            out << base << " v" << i << ";";
        }
        out << " } " << vec->to_string() << ";\n";
        out << "#endif\n";
    }
    if (!ctx.vector_order.empty()) {
        out << "\n";
    }

    for (const auto *enum_type : ctx.enum_order) {
        const std::string name = c_identifier(enum_type->enum_node->name);
        out << "typedef enum " << name << " {\n";
        for (const auto &[tag, val] : enum_type->enum_node->values) {
            out << "    " << tag << " = " << val << ",\n";
        }
        out << "} " << name << ";\n\n";
    }

    for (const auto *data : ctx.data_order) {
        const std::string name = c_identifier(data->data_node->name);
        out << "typedef struct " << lib << "_" << name << " {\n";
        for (const auto &field : data->data_node->fields) {
            out << "    " << header_c_type(lib, field.type, true).value() << " " << c_identifier(field.name) << ";\n";
        }
        out << "} " << lib << "_" << name << ";\n\n";
    }
}

static void header_emit_functions(const std::string &lib, std::ofstream &out, const std::vector<const FunctionNode *> &exported) {
    for (const auto *fn : exported) {
        if (fn->return_types.empty()) {
            out << "void";
        } else if (fn->return_types.size() > 1) {
            const std::string ret_struct = lib + "_" + c_identifier(fn->name) + "_ret";
            out << "typedef struct " << ret_struct << " {\n";
            for (size_t i = 0; i < fn->return_types.size(); i++) {
                out << "    " << header_c_type(lib, fn->return_types.at(i), false).value() << " ret_" << i << ";\n";
            }
            out << "} " << ret_struct << ";\n";
            out << ret_struct;
        } else {
            out << header_c_type(lib, fn->return_types.front(), false).value();
        }

        out << " " << lib << "_" << c_identifier(fn->name) << "(";
        bool first = true;
        for (const auto &param : fn->parameters) {
            if (!first) {
                out << ", ";
            }
            first = false;
            const std::string type_str = header_c_type(lib, param.type, param.is_mutable).value();
            out << (param.is_mutable ? "" : "const ") << type_str;
            if (type_str.at(type_str.size() - 1) != '*') {
                out << " ";
            }
            out << c_identifier(param.name);
        }
        out << ") asm(\"" << fn->file_hash.to_string() << "." << fn->name << ".export\");\n";
    }
}

bool Generator::generate_header(const std::filesystem::path &libname) {
    const std::string header_path = libname.string() + ".h";
    std::ofstream header(header_path);

    std::vector<const FunctionNode *> exported;
    for (const auto &instance : Parser::instances) {
        for (const std::unique_ptr<DefinitionNode> &def : instance.file_node_ptr->file_namespace->public_symbols.definitions) {
            if (def->get_variation() != DefinitionNode::Variation::FUNCTION) {
                continue;
            }
            const auto *fn = def->as<FunctionNode>();
            if (fn->visibility == FunctionNode::Visibility::EXPORT) {
                exported.push_back(fn);
            }
        }
    }

    if (exported.empty()) {
        return true;
    }

    HeaderCollect ctx;
    for (const auto *fn : exported) {
        for (const auto &param : fn->parameters) {
            if (!header_collect_type(ctx, param.type)) {
                header.close();
                std::filesystem::remove(std::filesystem::path(header_path));
                return false;
            }
        }
        for (const auto &ret : fn->return_types) {
            if (!header_collect_type(ctx, ret)) {
                header.close();
                std::filesystem::remove(std::filesystem::path(header_path));
                return false;
            }
        }
    }

    header << "#pragma once\n\n";
    header << "#include <stdbool.h>\n";
    header << "#include <stdint.h>\n\n";

    header << "#ifndef FLINT_OPT\n";
    header << "#define FLINT_OPT(T) struct { bool has_value; T value; }\n";
    header << "#endif\n\n";

    const std::string lib = libname.string();
    header_emit_types(lib, header, ctx);

    header << "/// @brief Returns 'false' on failure and 'true' if everything was OK\n";
    header << "extern bool " << lib << "_init(void) asm(\"flint.init." << lib << "\");\n\n";
    // TODO: Add a deinit function some time in the future
    // header << "extern void " << lib << "_deinit(void) asm(\"flint.deinit." << lib << "\");\n\n";

    header_emit_functions(lib, header, exported);
    return true;
}
