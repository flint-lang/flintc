#pragma once

#include <string>
#include <tuple>
#include <vector>

/// @enum `CompletionItemKind`
/// @brief All possible kinds of completions there are for the LSP
enum class CompletionItemKind : int {
    Text = 1,
    Method = 2,
    Function = 3,
    Constructor = 4,
    Field = 5,
    Variable = 6,
    Class = 7,
    Interface = 8,
    Module = 9,
    Property = 10,
    Unit = 11,
    Value = 12,
    Enum = 13,
    Keyword = 14,
    Snippet = 15,
    Color = 16,
    File = 17,
    Reference = 18,
    Folder = 19,
    EnumMember = 20,
    Constant = 21,
    Struct = 22,
    Event = 23,
    Operator = 24,
    TypeParameter = 25,
};

/// @brief Alias for the tuple for all completion items, where each item is an entry in the completion json response
///
/// These are the fields of the CompletionItem:
///   - The label of the completion (the string to match for)
///   - The kind of completion, which item kind it is
///   - A detail field for the item's description
///   - The text / snippet to insert
///   - Whether the completion item is a snippet (true) or pure text (false)
using CompletionItem = std::tuple<std::string, CompletionItemKind, std::string, std::string, bool>;

/// @class `CompletionData`
/// @brief Class which is responsible for managing all the data of completions
class CompletionData {
  public:
    CompletionData() = delete;

    // Get all completion items organized by category
    static const std::vector<CompletionItem> &get_keywords();
    static const std::vector<CompletionItem> &get_types();
    static const std::vector<CompletionItem> &get_definitions();
    static const std::vector<CompletionItem> &get_modules();
    static const std::vector<CompletionItem> &get_storage_classes();
    static const std::vector<CompletionItem> &get_constants();

    // Get all completion items combined
    static std::vector<CompletionItem> get_all_completions();

  private:
    static const std::vector<CompletionItem> keywords_;
    static const std::vector<CompletionItem> types_;
    static const std::vector<CompletionItem> definitions_;
    static const std::vector<CompletionItem> modules_;
    static const std::vector<CompletionItem> storage_classes_;
    static const std::vector<CompletionItem> constants_;
};

/// @function `completion_item_to_json`
/// @brief Helper function to convert completion item to JSON string
///
/// @param `item` The completion item to convert to an string
/// @return `std::string` The converted json string
std::string completion_item_to_json(const CompletionItem &item);

/// @function `completion_items_to_json_array`
/// @brief Helper function to convert vector of completion items to JSON array
///
/// @param `items` The list of all items to convert to one items json array
/// @return `std::string` The converted json string array
std::string completion_items_to_json_array(const std::vector<CompletionItem> &items);
