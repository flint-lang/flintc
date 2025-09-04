#include "completion_data.hpp"

#include <sstream>

// Static data definitions
const std::vector<CompletionItem> CompletionData::keywords_ = {
    {"if", CompletionItemKind::Keyword, "Conditional statement", "if "},
    {"else", CompletionItemKind::Keyword, "Else clause", "else "},
    {"for", CompletionItemKind::Keyword, "For loop", "for "},
    {"in", CompletionItemKind::Keyword, "Iterator keyword", "in "},
    {"while", CompletionItemKind::Keyword, "While loop", "while "},
    {"do", CompletionItemKind::Keyword, "Do block", "do "},
    {"switch", CompletionItemKind::Keyword, "Switch statement", "switch "},
    {"return", CompletionItemKind::Keyword, "Return statement", "return "},
    {"break", CompletionItemKind::Keyword, "Break statement", "break"},
    {"continue", CompletionItemKind::Keyword, "Continue statement", "continue"},
    {"throw", CompletionItemKind::Keyword, "Throw exception", "throw "},
    {"catch", CompletionItemKind::Keyword, "Catch exception", "catch "},
    {"not", CompletionItemKind::Keyword, "Logical not operator", "not "},
    {"and", CompletionItemKind::Keyword, "Logical and operator", "and "},
    {"or", CompletionItemKind::Keyword, "Logical or operator", "or "},
    {"as", CompletionItemKind::Keyword, "Type casting", "as "},
    {"spawn", CompletionItemKind::Keyword, "Spawn thread", "spawn "},
    {"sync", CompletionItemKind::Keyword, "Synchronize", "sync "},
    {"lock", CompletionItemKind::Keyword, "Lock resource", "lock "},
};

const std::vector<CompletionItem> CompletionData::types_ = {
    // Basic types
    {"str", CompletionItemKind::TypeParameter, "String type", "str"},
    {"fn", CompletionItemKind::TypeParameter, "Function type", "fn"},
    {"bp", CompletionItemKind::TypeParameter, "Blueprint type", "bp"},
    {"void", CompletionItemKind::TypeParameter, "Void type", "void"},
    {"bool", CompletionItemKind::TypeParameter, "Boolean type", "bool"},
    {"bool8", CompletionItemKind::TypeParameter, "8-bit boolean type", "bool8"},
    {"anyerror", CompletionItemKind::TypeParameter, "Any error type", "anyerror"},

    // Integer types
    {"u8", CompletionItemKind::TypeParameter, "8-bit unsigned integer", "u8"},
    {"i32", CompletionItemKind::TypeParameter, "32-bit signed integer", "i32"},
    {"i64", CompletionItemKind::TypeParameter, "64-bit signed integer", "i64"},
    {"u32", CompletionItemKind::TypeParameter, "32-bit unsigned integer", "u32"},
    {"u64", CompletionItemKind::TypeParameter, "64-bit unsigned integer", "u64"},

    // Float types
    {"f32", CompletionItemKind::TypeParameter, "32-bit float", "f32"},
    {"f64", CompletionItemKind::TypeParameter, "64-bit float", "f64"},

    // Vector types - u8
    {"u8x2", CompletionItemKind::TypeParameter, "2-element u8 vector", "u8x2"},
    {"u8x3", CompletionItemKind::TypeParameter, "3-element u8 vector", "u8x3"},
    {"u8x4", CompletionItemKind::TypeParameter, "4-element u8 vector", "u8x4"},
    {"u8x8", CompletionItemKind::TypeParameter, "8-element u8 vector", "u8x8"},

    // Vector types - i32
    {"i32x2", CompletionItemKind::TypeParameter, "2-element i32 vector", "i32x2"},
    {"i32x3", CompletionItemKind::TypeParameter, "3-element i32 vector", "i32x3"},
    {"i32x4", CompletionItemKind::TypeParameter, "4-element i32 vector", "i32x4"},
    {"i32x8", CompletionItemKind::TypeParameter, "8-element i32 vector", "i32x8"},

    // Vector types - i64
    {"i64x2", CompletionItemKind::TypeParameter, "2-element i64 vector", "i64x2"},
    {"i64x3", CompletionItemKind::TypeParameter, "3-element i64 vector", "i64x3"},
    {"i64x4", CompletionItemKind::TypeParameter, "4-element i64 vector", "i64x4"},

    // Vector types - f32
    {"f32x2", CompletionItemKind::TypeParameter, "2-element f32 vector", "f32x2"},
    {"f32x3", CompletionItemKind::TypeParameter, "3-element f32 vector", "f32x3"},
    {"f32x4", CompletionItemKind::TypeParameter, "4-element f32 vector", "f32x4"},
    {"f32x8", CompletionItemKind::TypeParameter, "8-element f32 vector", "f32x8"},

    // Vector types - f64
    {"f64x2", CompletionItemKind::TypeParameter, "2-element f64 vector", "f64x2"},
    {"f64x3", CompletionItemKind::TypeParameter, "3-element f64 vector", "f64x3"},
    {"f64x4", CompletionItemKind::TypeParameter, "4-element f64 vector", "f64x4"},
};

const std::vector<CompletionItem> CompletionData::functions_ = {
    {"def", CompletionItemKind::Function, "Function definition", "def "},
    {"func", CompletionItemKind::Function, "Function definition (alternative)", "func "},
    {"test", CompletionItemKind::Function, "Test block", "test \\\"@{$0}\\\":"},
};

const std::vector<CompletionItem> CompletionData::modules_ = {
    {"data", CompletionItemKind::Class, "Data structure definition", "data "},
    {"entity", CompletionItemKind::Class, "Entity definition", "entity "},
    {"enum", CompletionItemKind::Class, "Enumeration definition", "enum "},
    {"variant", CompletionItemKind::Class, "Variant definition", "variant "},
    {"error", CompletionItemKind::Class, "Error type definition", "error "},
    {"use", CompletionItemKind::Module, "Import statement", "use "},
    {"extern", CompletionItemKind::Module, "External declaration", "extern "},
    {"export", CompletionItemKind::Module, "Export declaration", "export "},
    {"requires", CompletionItemKind::Module, "Requires declaration", "requires "},
    {"extends", CompletionItemKind::Module, "Extends declaration", "extends "},
    {"link", CompletionItemKind::Module, "Link declaration", "link "},
};

const std::vector<CompletionItem> CompletionData::storage_classes_ = {
    {"const", CompletionItemKind::Keyword, "Constant declaration", "const "},
    {"mut", CompletionItemKind::Keyword, "Mutable declaration", "mut "},
    {"shared", CompletionItemKind::Keyword, "Shared declaration", "shared "},
};

const std::vector<CompletionItem> CompletionData::constants_ = {
    {"true", CompletionItemKind::Constant, "Boolean true", "true"},
    {"false", CompletionItemKind::Constant, "Boolean false", "false"},
    {"none", CompletionItemKind::Constant, "None value", "none"},
};

const std::vector<CompletionItem> &CompletionData::get_keywords() {
    return keywords_;
}

const std::vector<CompletionItem> &CompletionData::get_types() {
    return types_;
}

const std::vector<CompletionItem> &CompletionData::get_functions() {
    return functions_;
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
    const auto &functions = get_functions();
    const auto &modules = get_modules();
    const auto &storage_classes = get_storage_classes();
    const auto &constants = get_constants();

    all_items.insert(all_items.end(), keywords.begin(), keywords.end());
    all_items.insert(all_items.end(), types.begin(), types.end());
    all_items.insert(all_items.end(), functions.begin(), functions.end());
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
    json << "        \"insertText\": \"" << std::get<3>(item) << "\"\n";
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
