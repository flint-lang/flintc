#include "completion_data.hpp"

#include <sstream>

// Static data definitions
const std::vector<CompletionItem> CompletionData::keywords_ = {
    {"def", CompletionItemKind::Keyword, "Function definition", "def ${1:function_name}($2)$3:$0", true},
    {"if", CompletionItemKind::Keyword, "Conditional statement", "if ", false},
    {"else", CompletionItemKind::Keyword, "Else clause", "else ", false},
    {"for", CompletionItemKind::Keyword, "For loop", "for ", false},
    {"in", CompletionItemKind::Keyword, "Iterator keyword", "in ", false},
    {"while", CompletionItemKind::Keyword, "While loop", "while ", false},
    {"do", CompletionItemKind::Keyword, "Do block", "do ", false},
    {"switch", CompletionItemKind::Keyword, "Switch statement", "switch ", false},
    {"return", CompletionItemKind::Keyword, "Return statement", "return ", false},
    {"break", CompletionItemKind::Keyword, "Break statement", "break", false},
    {"continue", CompletionItemKind::Keyword, "Continue statement", "continue", false},
    {"throw", CompletionItemKind::Keyword, "Throw exception", "throw ", false},
    {"catch", CompletionItemKind::Keyword, "Catch exception", "catch ", false},
    {"not", CompletionItemKind::Keyword, "Logical not operator", "not ", false},
    {"and", CompletionItemKind::Keyword, "Logical and operator", "and ", false},
    {"or", CompletionItemKind::Keyword, "Logical or operator", "or ", false},
    {"as", CompletionItemKind::Keyword, "Type casting", "as ", false},
    {"spawn", CompletionItemKind::Keyword, "Spawn thread", "spawn ", false},
    {"sync", CompletionItemKind::Keyword, "Synchronize", "sync ", false},
    {"lock", CompletionItemKind::Keyword, "Lock resource", "lock ", false},
};

const std::vector<CompletionItem> CompletionData::types_ = {
    // Basic types
    {"str", CompletionItemKind::TypeParameter, "String type", "str", false},
    {"fn", CompletionItemKind::TypeParameter, "Function type", "fn", false},
    {"bp", CompletionItemKind::TypeParameter, "Blueprint type", "bp", false},
    {"void", CompletionItemKind::TypeParameter, "Void type", "void", false},
    {"bool", CompletionItemKind::TypeParameter, "Boolean type", "bool", false},
    {"bool8", CompletionItemKind::TypeParameter, "8-bit boolean type", "bool8", false},
    {"anyerror", CompletionItemKind::TypeParameter, "Any error type", "anyerror", false},

    // Integer types
    {"i8", CompletionItemKind::TypeParameter, "8-bit signed integer", "i8", false},
    {"i16", CompletionItemKind::TypeParameter, "16-bit signed integer", "i16", false},
    {"i32", CompletionItemKind::TypeParameter, "32-bit signed integer", "i32", false},
    {"i64", CompletionItemKind::TypeParameter, "64-bit signed integer", "i64", false},
    {"u8", CompletionItemKind::TypeParameter, "8-bit unsigned integer", "u8", false},
    {"u16", CompletionItemKind::TypeParameter, "16-bit unsigned integer", "u16", false},
    {"u32", CompletionItemKind::TypeParameter, "32-bit unsigned integer", "u32", false},
    {"u64", CompletionItemKind::TypeParameter, "64-bit unsigned integer", "u64", false},

    // Float types
    {"f32", CompletionItemKind::TypeParameter, "32-bit float", "f32", false},
    {"f64", CompletionItemKind::TypeParameter, "64-bit float", "f64", false},

    // Vector types - u8
    {"u8x2", CompletionItemKind::TypeParameter, "2-element u8 vector", "u8x2", false},
    {"u8x3", CompletionItemKind::TypeParameter, "3-element u8 vector", "u8x3", false},
    {"u8x4", CompletionItemKind::TypeParameter, "4-element u8 vector", "u8x4", false},
    {"u8x8", CompletionItemKind::TypeParameter, "8-element u8 vector", "u8x8", false},

    // Vector types - i32
    {"i32x2", CompletionItemKind::TypeParameter, "2-element i32 vector", "i32x2", false},
    {"i32x3", CompletionItemKind::TypeParameter, "3-element i32 vector", "i32x3", false},
    {"i32x4", CompletionItemKind::TypeParameter, "4-element i32 vector", "i32x4", false},
    {"i32x8", CompletionItemKind::TypeParameter, "8-element i32 vector", "i32x8", false},

    // Vector types - i64
    {"i64x2", CompletionItemKind::TypeParameter, "2-element i64 vector", "i64x2", false},
    {"i64x3", CompletionItemKind::TypeParameter, "3-element i64 vector", "i64x3", false},
    {"i64x4", CompletionItemKind::TypeParameter, "4-element i64 vector", "i64x4", false},

    // Vector types - f32
    {"f32x2", CompletionItemKind::TypeParameter, "2-element f32 vector", "f32x2", false},
    {"f32x3", CompletionItemKind::TypeParameter, "3-element f32 vector", "f32x3", false},
    {"f32x4", CompletionItemKind::TypeParameter, "4-element f32 vector", "f32x4", false},
    {"f32x8", CompletionItemKind::TypeParameter, "8-element f32 vector", "f32x8", false},

    // Vector types - f64
    {"f64x2", CompletionItemKind::TypeParameter, "2-element f64 vector", "f64x2", false},
    {"f64x3", CompletionItemKind::TypeParameter, "3-element f64 vector", "f64x3", false},
    {"f64x4", CompletionItemKind::TypeParameter, "4-element f64 vector", "f64x4", false},
};

const std::vector<CompletionItem> CompletionData::definitions_ = {
    {"data", CompletionItemKind::Class, "Data structure definition", "data ", false},
    {"func", CompletionItemKind::Class, "Func module definition", "func ", false},
    {"entity", CompletionItemKind::Class, "Entity definition", "entity ", false},
    {"enum", CompletionItemKind::Class, "Enumeration definition", "enum ", false},
    {"variant", CompletionItemKind::Class, "Variant definition", "variant ", false},
    {"error", CompletionItemKind::Class, "Error type definition", "error ", false},
    {"test", CompletionItemKind::Class, "Test block", "test \\\"${1:test_name}\\\":", true},
};

const std::vector<CompletionItem> CompletionData::modules_ = {
    {"use", CompletionItemKind::Module, "Import statement", "use ", false},
    {"extern", CompletionItemKind::Module, "External declaration", "extern ", false},
    {"export", CompletionItemKind::Module, "Export declaration", "export ", false},
    {"requires", CompletionItemKind::Module, "Requires declaration", "requires(${1:DataType d}):", true},
    {"extends", CompletionItemKind::Module, "Extends declaration", "extends(${1:EntityType e}):", true},
    {"link", CompletionItemKind::Module, "Link declaration", "link:", false},
    {"hook", CompletionItemKind::Module, "Hook declaration", "hook:", false},
};

const std::vector<CompletionItem> CompletionData::storage_classes_ = {
    {"const", CompletionItemKind::Keyword, "Constant declaration", "const ", false},
    {"mut", CompletionItemKind::Keyword, "Mutable declaration", "mut ", false},
    {"shared", CompletionItemKind::Keyword, "Shared declaration", "shared ", false},
};

const std::vector<CompletionItem> CompletionData::constants_ = {
    {"true", CompletionItemKind::Constant, "Boolean true", "true", false},
    {"false", CompletionItemKind::Constant, "Boolean false", "false", false},
    {"none", CompletionItemKind::Constant, "None value", "none", false},
};

const std::vector<CompletionItem> &CompletionData::get_keywords() {
    return keywords_;
}

const std::vector<CompletionItem> &CompletionData::get_types() {
    return types_;
}

const std::vector<CompletionItem> &CompletionData::get_definitions() {
    return definitions_;
}

const std::vector<CompletionItem> &CompletionData::get_modules() {
    return modules_;
}

const std::vector<CompletionItem> &CompletionData::get_storage_classes() {
    return storage_classes_;
}

const std::vector<CompletionItem> &CompletionData::get_constants() {
    return constants_;
}

std::vector<CompletionItem> CompletionData::get_all_completions() {
    std::vector<CompletionItem> all_items;

    // Combine all categories
    const auto &keywords = get_keywords();
    const auto &types = get_types();
    const auto &definitions = get_definitions();
    const auto &modules = get_modules();
    const auto &storage_classes = get_storage_classes();
    const auto &constants = get_constants();

    all_items.insert(all_items.end(), keywords.begin(), keywords.end());
    all_items.insert(all_items.end(), types.begin(), types.end());
    all_items.insert(all_items.end(), definitions.begin(), definitions.end());
    all_items.insert(all_items.end(), modules.begin(), modules.end());
    all_items.insert(all_items.end(), storage_classes.begin(), storage_classes.end());
    all_items.insert(all_items.end(), constants.begin(), constants.end());

    return all_items;
}

std::string completion_item_to_json(const CompletionItem &item) {
    std::stringstream json;
    json << "{\n";
    json << "        \"label\": \"" << std::get<0>(item) << "\",\n";
    json << "        \"kind\": " << static_cast<int>(std::get<1>(item)) << ",\n";
    json << "        \"detail\": \"" << std::get<2>(item) << "\",\n";
    json << "        \"insertText\": \"" << std::get<3>(item) << "\"";
    if (std::get<4>(item)) {
        json << ",\n        \"insertTextFormat\": 2\n"; // Handle the inserted text as snippets to support the snippet syntax of the LSP
    } else {
        json << "\n";
    }
    json << "      }";
    return json.str();
}

std::string completion_items_to_json_array(const std::vector<CompletionItem> &items) {
    std::stringstream json;
    json << "[\n";

    for (size_t i = 0; i < items.size(); ++i) {
        json << "      " << completion_item_to_json(items[i]);
        if (i < items.size() - 1) {
            json << ",";
        }
        json << "\n";
    }

    json << "    ]";
    return json.str();
}
