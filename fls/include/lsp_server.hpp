#pragma once

#include "completion_data.hpp"
#include "parser/ast/file_node.hpp"

#ifndef FLINT_LSP
#define FLINT_LSP
#endif
#include "error/diagnostics.hpp"

#include <optional>
#include <string>
#include <vector>

/// @class `LspServer`
/// @brief Main Class of the whole LS
class LspServer {
  public:
    LspServer() = delete;

    /// @function `run`
    /// @brief Executes the main loop of the server. Listens for any messages on stdin and processes them
    static void run();

    /// @function `parse_program`
    /// @brief Parses the whole program and returns the main file of the program
    ///
    /// @brief `file_path` The path to the root file of the parsing process
    /// @brief `file_content` The content of the file, possibly
    ///
    /// @note This function is mutexed, so only one thread can ever re-parse the file at once
    /// @note All internal state of the parser etc is cleaned at the beginning of the function before actual parsing begins
    static std::optional<FileNode *> parse_program(const std::string &file_path, const std::optional<std::string> &file_content);

    /// @function `log_info`
    /// @brief Logs a given message with the [INFO] prefix to stdout
    ///
    /// @brief `message` The message too log
    static void log_info(const std::string &message);

  private:
    /// @function `process_message`
    /// @brief Processes a given message and calls the corresponding handler functions
    ///
    /// @param `content` The content of the message
    static void process_message(const std::string &content);

    /*
     * ==========================
     * MESSAGE HANDLING FUNCTIONS
     * ==========================
     */

    /// @function `send_initialize_response`
    /// @brief Sends the initialization response over stdout
    ///
    /// @param `request_id` The ID of the request which is handled by this function
    static void send_initialize_response(const std::string &request_id = "1");

    /// @function `send_shutdown_response`
    /// @brief Sends the shutdown response over stdout
    ///
    /// @param `request_id` The ID of the request which is handled by this function
    static void send_shutdown_response(const std::string &request_id = "2");

    /// @function `send_completion`
    /// @brief Sends the completion response over stdout
    ///
    /// @param `content` The content of the request message
    static void send_completion_response(const std::string &content);

    /// @struct `DefinitionResult`
    /// @brief Carries the target location plus the source range to highlight
    struct DefinitionResult {
        /// @var `target_hash`
        /// @brief The hash of the file the target is located at
        Hash target_hash;

        /// @var `target_line`
        /// @brief The target line is 0-indexed
        int target_line = 0;

        /// @var `target_col`
        /// @brief The target column is 0-indexed
        int target_col = 0;

        /// @var `source_start`
        /// @brief The source start is a 0-indexed VSCode character, -1 = unknown
        int source_start = -1;

        /// @var `source_end`
        /// @brief The source end is a 0-indexed VSCode character, -1 = unknown
        int source_end = -1;
    };

    /// @function `send_definition_response`
    /// @brief Sends the definition response over stdout
    ///
    /// @param `content` The content of the request message
    static void send_definition_response(const std::string &content);

    /// @function `find_definition_at_position`
    /// @brief Finds the definition of the symbol at the given position
    ///
    /// @param `file_path` The file path
    /// @param `line` The line number (0-based)
    /// @param `character` The character number (0-based)
    /// @return `std::optional<DefinitionResult>` Target location + source range
    static std::optional<DefinitionResult> find_definition_at_position( //
        const std::string &file_path,                                   //
        int line,                                                       //
        int character                                                   //
    );

    /// @function `publish_diagnostics`
    /// @brief Publishes diagnostics for a file
    ///
    /// @param `file_uri` The URI of the file
    static void publish_diagnostics(const std::string &file_uri);

    /// @function `add_nodes_from_namespace_to_completions`
    /// @brief Adds all the nodes of the given namespace to the completions list
    ///
    /// @brief `file_namespace` The namespace from which to add all definitions to the list of possible completions
    /// @brief `completions` The list of completions to which to add the definitions
    /// @brief `imported_files` The list of all imported files of this file node
    /// @brief `is_root_file` Whether the given file is the root file. If it's the root file then all nodes of all imported files are added
    /// to the completions as well, if it's not the root file then all imported file's definitions will not be added as Flint has a strict
    /// inclusion depth of 1, always
    static void add_nodes_from_namespace_to_completions( //
        const Namespace *file_namespace,                 //
        std::vector<CompletionItem> &completions,        //
        std::vector<const ImportNode *> &imported_files, //
        const bool is_root_file                          //
    );

    /// @function `try_parse_and_add_completions`
    /// @brief Tries to parse the file graph and then add all completions of the parser AST to the completions list
    ///
    /// @param `file_path` The path to the file to start parsing at
    /// @param `line` The line at which the completion is requested
    /// @param `character` The character in the given line at which the completion is requested
    /// @param `completions` The list of completions to which to add the AST definitions
    static void try_parse_and_add_completions(   //
        const std::string &file_path,            //
        int line,                                //
        int character,                           //
        std::vector<CompletionItem> &completions //
    );

    /// @function `get_context_aware_completions`
    /// @brief Parses the given file and collects all types, functions etc from the file for the completion system
    ///
    /// @param `file_path` The file path string to the file which needs to be parsed and checked
    /// @param `line` The line at which the completion is requested
    /// @param `character` The character in the given line at which the completion is requested
    ///
    /// @note This function parses not only the given file but all files it includes too, to give suggestions of each file it included
    static std::vector<CompletionItem> get_context_aware_completions(const std::string &file_path, int line, int character);

    /// @function `send_hover_response`
    /// @brief Sends the hover response over stdout
    ///
    /// @param `request_id` The ID of the request which is handled by this function
    static void send_hover_response(const std::string &request_id = "4");

    /*
     * =================
     * UTILITY FUNCTIONS
     * =================
     */

    /// @function `handle_document_open`
    /// @brief Handles the event when a document got opened
    ///
    /// @brief `content` The message content from the LSP of the open event
    static void handle_document_open(const std::string &content);

    /// @function `handle_document_change`
    /// @brief Handles the event when a document changed
    ///
    /// @brief `content` The message content from the LSP of the change event
    static void handle_document_change(const std::string &content);

    /// @function `handle_document_save`
    /// @brief Handles the event when a document is saved
    ///
    /// @brief `content` The message content from the LSP of the save event
    static void handle_document_save(const std::string &content);

    /// @function `extract_file_uri`
    /// @brief Extracts the file URI from a textDocument request
    ///
    /// @param `content` The LSP message content
    /// @return `std::string` The file URI (empty if not found)
    static std::string extract_file_uri(const std::string &content);

    /// @function `extract_file_content_from_change`
    /// @brief Extracts the file content from a textDocument request
    ///
    /// @param `content` The LSP message content
    /// @return `std::string` The file content (empty if not found)
    static std::string extract_file_content_from_change(const std::string &content);

    /// @function `unescape_json_string`
    /// @brief Unescapes all escapes in the string from a json message
    ///
    /// @param `escaped` The escaped string to unescape
    /// @result `std::string` The unescaped result
    static std::string unescape_json_string(const std::string &escaped);

    /// @function `extract_position`
    /// @brief Extracts line and character position from completion request
    ///
    /// @param `content` The LSP message content
    /// @return `std::pair<int, int>` Line and character position (-1, -1 if not found)
    static std::pair<int, int> extract_position(const std::string &content);

    /// @function `uri_to_file_path`
    /// @brief Converts file:// URI to local file path
    ///
    /// @param `uri` The file URI
    /// @return `std::string` Local file path
    static std::string uri_to_file_path(const std::string &uri);

    /// @function `diagnostic_to_json`
    /// @brief Converts a diagnostic to JSON format
    ///
    /// @param `diagnostic` The diagnostic to convert
    /// @return `std::string` The converte diagnostic JSON string representation
    static std::string diagnostic_to_json(const Diagnostic &diagnostic) {
        const int line = std::get<0>(diagnostic.range);
        const int column = std::get<1>(diagnostic.range);
        const int length = std::get<2>(diagnostic.range);
        std::stringstream json;
        json << "{\n";
        json << "        \"range\": {\n";
        json << "          \"start\": {\"line\": " << line << ", \"character\": " << column << "},\n";
        json << "          \"end\": {\"line\": " << line << ", \"character\": " << column + length << "}\n";
        json << "        },\n";
        json << "        \"severity\": " << static_cast<int>(diagnostic.level) << ",\n";
        json << "        \"message\": \"" << diagnostic.message << "\",\n";
        json << "        \"source\": \"" << diagnostic.source << "\"\n";
        json << "      }";
        return json.str();
    }

    /// @function `diagnostics_to_json_array`
    /// @brief Converts a vector of diagnostics to a JSON array
    ///
    /// @return `std::string` The converted diagnostics JSON array
    static std::string diagnostics_to_json_array() {
        std::stringstream json;
        json << "[\n";

        for (size_t i = 0; i < diagnostics.size(); ++i) {
            json << "      " << diagnostic_to_json(diagnostics[i]);
            if (i < diagnostics.size() - 1) {
                json << ",";
            }
            json << "\n";
        }

        json << "    ]";
        return json.str();
    }

    /*
     * =================
     * HOVER IFNO FUNCTIONS
     * =================
     */

    /// @struct `LocalVariable`
    /// @brief Represents a local variable at the cursor position
    struct LocalVariable {
        std::string name;
        std::shared_ptr<Type> type;
        Hash file_hash;
        unsigned int line = 0;
        unsigned int column = 0;
    };

    /// @struct `ImportedFile`
    /// @brief Represents an imported file at the cursor position
    struct ImportedFile {
        Hash file_hash;
    };

    /// @typedef `PositionInfo`
    /// @brief Unified result type for node lookup — one of: a variable, an import, a type, or a function
    using PositionInfo = std::variant<LocalVariable, ImportedFile, std::shared_ptr<Type>, const FunctionNode *>;

    /// @function `get_token_at_pos`
    /// @brief Finds the token at the given position in the namespace and returns it, if any token is present at that position
    ///
    /// @param `ns` The namespace in which to search for the token at the position
    /// @param `line` The line the token could be
    /// @param `col` The column the token could be
    /// @return `std::optional<TokenContext>` A token context for the token at line:col, nullopt if there is no token there
    static std::optional<TokenContext> get_token_at_pos(const Namespace *ns, unsigned int line, unsigned int col);

    /// @function `find_node_in_expr`
    /// @brief Tries to find a node (PositionInfo) inside a given expression
    ///
    /// @param `expr` The expression to recursively search through to get the value under the cursor
    /// @param `scope` The scope in which the expression is contained in
    /// @param `line` The cursor line position to search for a value at
    /// @param `line` The cursor column position to search for a value at
    /// @return `std::optional<PositionInfo>` Positional info for the value under the cursor, if any value is found
    static std::optional<PositionInfo> find_node_in_expr( //
        const ExpressionNode *expr,                       //
        const Scope *scope,                               //
        unsigned int line,                                //
        unsigned int col                                  //
    );

    /// @function `find_node_in_stmt`
    /// @brief Tries to find a node (PositionInfo) inside a given statement
    ///
    /// @param `ns` The namespace in which to search for the value
    /// @param `stmt` The statement to recursively search through to get the value under the cursor
    /// @param `scope` The scope in which the statement is contained in
    /// @param `line` The cursor line position to search for a value at
    /// @param `col` The cursor column position to search for a value at
    /// @return `std::optional<PositionInfo>` Positional info for the value under the cursor, if any value is found
    static std::optional<PositionInfo> find_node_in_stmt( //
        const Namespace *ns,                              //
        const StatementNode *stmt,                        //
        const Scope *scope,                               //
        const unsigned int line,                          //
        const unsigned int col                            //
    );

    /// @function `find_node_in_scope`
    /// @brief Tries to find a node (PositionInfo) inside a given scope
    ///
    /// @param `ns` The namespace in which to search for the value
    /// @param `scope` The scope to recursively search through to get the value under the cursor
    /// @param `line` The cursor line position to search for a value at
    /// @param `col` The cursor column position to search for a value at
    /// @return `std::optional<PositionInfo>` Positional info for the value under the cursor, if any value is found
    static std::optional<PositionInfo> find_node_in_scope( //
        const Namespace *ns,                               //
        const Scope *scope,                                //
        const unsigned int line,                           //
        const unsigned int col                             //
    );

    /// @function `find_node_in_def`
    /// @brief Tries to find a node (PositionInfo) inside a given definition
    ///
    /// @param `ns` The namespace in which to search for the value
    /// @param `def` The definition to recursively search through to get the value under the cursor
    /// @param `line` The cursor line position to search for a value at
    /// @param `col` The cursor column position to search for a value at
    /// @return `std::optional<PositionInfo>` Positional info for the value under the cursor, if any value is found
    static std::optional<PositionInfo> find_node_in_def( //
        const Namespace *ns,                             //
        const DefinitionNode *def,                       //
        const unsigned int line,                         //
        const unsigned int col                           //
    );

    /// @function `find_node_at`
    /// @brief Tries to find a node at the given position inside the given namespace
    ///
    /// @param `ns` The namespace in which to search for the value under the cursor
    /// @param `line` The cursor line position to search for a value at
    /// @param `col` The cursor column position to search for a value at
    /// @return `std::optional<PositionInfo>` Positional info for the value under the cursor, if any value is found
    static std::optional<PositionInfo> find_node_at(const Namespace *ns, unsigned int line, unsigned int col);

    /// @function `vscode_char_to_column`
    /// @brief Converts the vscode character representation to a Flint-internal representation by expanding hard tabs etc.
    ///
    /// @param `line` A string view of the source code line to convert the character index to a column number
    /// @param `character` The character index to get the column from
    /// @return `unsigned int` The column of a given character index at a given line
    static unsigned int vscode_char_to_column(std::string_view line, int character);

    /// @function `column_to_vscode_char`
    /// @brief Converts the column to a vscode character index representation
    ///
    /// @param `line` A string veiw of the source code line to convert the column number to a character index
    /// @param `col` The column to get the character index from
    /// @return `unsigned int` The character index of a given column in a given line
    static unsigned int column_to_vscode_char(std::string_view line, unsigned int col);

    /// @function `get_type_definition_location`
    /// @brief Returns the position where a given type was defined at, if there exists a position for it
    ///
    /// @param `type` The type to get the position info where it was defined at
    /// @return `std::optional<std::tuple<Hash, int, int>>` An tuple containing the Hash of the file the type was defined in together with
    ///                                                     the line and column the type was defined at, or nullopt of the type wasn't
    ///                                                     defined aynwhere (primitive types, inline types etc)
    static std::optional<std::tuple<Hash, int, int>> get_type_definition_location(const std::shared_ptr<Type> &type);

    /// @function `build_type_hover_info`
    /// @brief Builds the hover information when hovering over different types to provide good hover information for every single type
    ///
    /// @param `type` The type to build the hover info from
    /// @return `std::string` The hover info of the given type
    static std::string build_type_hover_info(const std::shared_ptr<Type> &type);

    /// @function `build_function_hover_info`
    /// @brief Builds the hover information when hovering over a given function to provide good hover information for it
    ///
    /// @param `fn` The function to get the hover information from
    /// @return `std::string` The hover info of the given function
    static std::string build_function_hover_info(const FunctionNode *fn);

    /// @function `json_escape`
    /// @brief Escapes all characters in a given string to be suitable for json
    ///
    /// @param `s` The string to make "json compatible"
    /// @return `std::string` The "json compatible" string
    static std::string json_escape(const std::string &s);
};
