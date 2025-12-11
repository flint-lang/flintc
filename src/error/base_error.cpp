#include "error/error_types/base_error.hpp"
#include "lexer/lexer.hpp"
#include "lexer/lexer_utils.hpp"
#include "parser/parser.hpp"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <stack>

std::string BaseError::to_string() const {
    std::ostringstream oss;
    if (hash.empty()) {
        oss << RED << error_type_names.at(error_type) << DEFAULT << " at " << GREEN << "unknown file" << DEFAULT << "\n" << "├┤E0000│\n";
        return oss.str();
    }
    oss << RED << error_type_names.at(error_type) << DEFAULT << " at " << GREEN
        << std::filesystem::relative(hash.path, std::filesystem::current_path()).string() << ":" << line << ":" << column << DEFAULT
        << "\n";
    if (error_type == ERR_LEXING) {
        // The lines have not been lexed and added to the parser instances yet, so trying to print the lines will cause an exception
        // Instead, we need to do minimal printing, without the file content
        oss << "├┤E0000│\n";
        return oss.str();
    }
    // Print the lines in which the error happened as a stack, as we will add prior lines to the stack and then print it in reverse
    std::stack<std::string> lines_to_print;
    std::optional<const Parser *> parser = Parser::get_instance_from_hash(hash);
    assert(parser.has_value());
    const std::vector<std::pair<unsigned int, std::string_view>> &source_code_lines = parser.value()->get_source_code_lines();
    // First, we need to get the indent level of the line the error happened in
    unsigned int indent_lvl = source_code_lines.at(line - 1).first;
    unsigned int leading_spaces = indent_lvl * Lexer::TAB_SIZE;
    std::string_view err_line = source_code_lines.at(line - 1).second;
    while (leading_spaces > 0) {
        if (err_line[0] == '\t') {
            err_line = err_line.substr(1);
        } else if (err_line[0] == ' ') {
            err_line = err_line.substr(Lexer::TAB_SIZE);
        } else {
            // This should never come here, it's my fault if it would
            assert(false);
        }
        leading_spaces -= Lexer::TAB_SIZE;
    }
    // Get the number of characters needed to represent the `line` number (3 for `123` etc)
    const unsigned int line_space = std::to_string(line).size();
    oss << "└";
    for (unsigned int i = 0; i < line_space; i++) {
        oss << "─";
    }
    oss << "┬┤E0000│\n";
    // We push the error line as the first line to the stack until we have reached the top level
    std::stringstream line_string;
    line_string << std::left << std::setw(line_space) << std::to_string(line) << " │ " << GREY;
    for (unsigned int i = 0; i < indent_lvl; i++) {
        // The `»` unicode character takes up 2 bytes, that's why we need to set the width to one more to visually end up with
        // TAB_SIZE characters
        line_string << std::left << std::setw(Lexer::TAB_SIZE + 1) << "»";
    }
    const unsigned int offset = indent_lvl * Lexer::TAB_SIZE;
    line_string << DEFAULT << std::string(err_line.substr(0, column - 1 - offset));
    line_string << RED_UNDERLINE << std::string(err_line.substr(column - 1 - offset, length));
    line_string << DEFAULT << std::string(err_line.substr(column - 1 - offset + length));
    lines_to_print.push(line_string.str());
    line_string.str("");
    line_string.clear();
    for (unsigned int current_line = line - 1; current_line > 0; current_line--) {
        unsigned int line_indent_lvl = source_code_lines.at(current_line - 1).first;
        std::string_view current_line_view = source_code_lines.at(current_line - 1).second;
        leading_spaces = line_indent_lvl * Lexer::TAB_SIZE;
        while (leading_spaces > 0) {
            if (current_line_view[0] == '\t') {
                current_line_view = current_line_view.substr(1);
            } else if (current_line_view[0] == ' ') {
                current_line_view = current_line_view.substr(Lexer::TAB_SIZE);
            } else {
                // This should never come here, it's my fault if it would
                assert(false);
            }
            leading_spaces -= Lexer::TAB_SIZE;
        }
        // Check if the current line even contains anything other than spaces, \t and \n and only continue if it contains *real* content
        size_t comment_pos = current_line_view.find("//");
        if (comment_pos == std::string::npos) {
            comment_pos = current_line_view.size();
        }
        std::string_view code_part = current_line_view.substr(0, comment_pos);
        if (code_part.find_first_not_of(" \t\n") == std::string_view::npos) {
            // Is empty line
            continue;
        }
        if (line_indent_lvl < indent_lvl) {
            // Double-indent difference should not be possible at all
            assert(line_indent_lvl == indent_lvl - 1);
            line_string << std::left << std::setw(line_space) << std::to_string(current_line) << " │ " << GREY;
            for (unsigned int i = 0; i < line_indent_lvl; i++) {
                // The `»` unicode character takes up 2 bytes, that's why we need to set the width to one more to visually end up with
                // TAB_SIZE characters
                line_string << std::left << std::setw(Lexer::TAB_SIZE + 1) << "»";
            }
            line_string << DEFAULT << std::string(current_line_view);
            lines_to_print.push(line_string.str());
            line_string.str("");
            line_string.clear();
            indent_lvl--;
            if (indent_lvl == 0) {
                break;
            }
        }
    }
    // Okay now we can add all the lines to the console output in reverse
    while (!lines_to_print.empty()) {
        oss << lines_to_print.top();
        lines_to_print.pop();
    }
    // We can now add the marker for the errors to specify where the error was
    oss << "┌";
    for (unsigned int i = 0; i < line_space; i++) {
        oss << "─";
    }
    oss << "┴─";
    for (unsigned int i = column; i > 1; i--) {
        oss << "─";
    }
    oss << "┘\n";
    return oss.str();
}

Diagnostic BaseError::to_diagnostic() const {
    if (hash.empty()) {
        return Diagnostic(std::make_tuple(0, 0, 0), DiagnosticLevel::Error, "NO_MESSAGE", "");
    }
    // We first need to get the correct character. The only character that spans wider is the \t, so we first need to get the indent lvl of
    // the line the error happened in and then substract INDENT_LVL * (TAB_WIDTH - 1) to get the correct character offset
    const std::string &file_path_string = hash.path.string();
    int indent_lvl = Parser::get_instance_from_hash(hash).value()->get_source_code_lines().at(line - 1).first;
    int character = column - 1 - (indent_lvl * (Lexer::TAB_SIZE - 1));
    return Diagnostic(std::make_tuple(line - 1, character, length), DiagnosticLevel::Error, "NO_MESSAGE", file_path_string);
}

std::string BaseError::trim_right(const std::string &str) {
    size_t size = str.length();
    for (auto it = str.rbegin(); it != str.rend() && std::isspace(*it); ++it) {
        --size;
    }
    return str.substr(0, size);
}

std::string BaseError::get_token_string(const std::vector<Token> &tokens) {
    std::ostringstream oss;
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        if (it != tokens.begin()) {
            oss << " ";
        }
        oss << "'" << get_token_name(*it) << "'";
    }
    return oss.str();
}

[[nodiscard]] std::string BaseError::get_token_string(const token_slice &tokens, const std::vector<Token> &ignore_tokens) {
    std::stringstream token_str;
    for (auto it = tokens.first; it != tokens.second; it++) {
        if (std::find(ignore_tokens.begin(), ignore_tokens.end(), it->token) != ignore_tokens.end()) {
            continue;
        }
        switch (it->token) {
            case TOK_EOF:
                continue;
            case TOK_TYPE:
                token_str << it->type->to_string();
                if (space_needed(tokens, it, {TOK_RIGHT_PAREN, TOK_COMMA, TOK_SEMICOLON, TOK_COLON})) {
                    token_str << " ";
                }
                break;
            case TOK_STR_VALUE:
                token_str << "\"" + std::string(it->lexme) + "\"";
                if (space_needed(tokens, it, {TOK_RIGHT_PAREN, TOK_COMMA, TOK_SEMICOLON, TOK_COLON})) {
                    token_str << " ";
                }
                break;
            case TOK_CHAR_VALUE:
                token_str << "'" + std::string(it->lexme) + "' ";
                break;
            case TOK_IDENTIFIER:
                token_str << it->lexme;
                if (space_needed(tokens, it, {TOK_LEFT_PAREN, TOK_RIGHT_PAREN, TOK_COMMA, TOK_SEMICOLON, TOK_COLON})) {
                    token_str << " ";
                }
                break;
            case TOK_LEFT_PAREN:
                token_str << it->lexme;
                break;
            case TOK_INDENT:
                token_str << std::string(Lexer::TAB_SIZE, ' ');
                break;
            default:
                token_str << it->lexme;
                if (space_needed(tokens, it, {TOK_RIGHT_PAREN, TOK_COMMA, TOK_SEMICOLON, TOK_COLON})) {
                    token_str << " ";
                }
                break;
        }
    }
    return trim_right(token_str.str());
}

std::string BaseError::get_function_signature_string(   //
    const std::string &function_name,                   //
    const std::vector<std::shared_ptr<Type>> &arg_types //
) {
    std::stringstream oss;
    oss << function_name << "(";
    for (auto arg = arg_types.begin(); arg != arg_types.end(); ++arg) {
        oss << (*arg)->to_string() << (arg != arg_types.begin() ? ", " : "");
    }
    oss << ")";
    return oss.str();
}

bool BaseError::space_needed(                  //
    const token_slice &tokens,                 //
    const token_list::const_iterator iterator, //
    const std::vector<Token> &ignores          //
) {
    return iterator != std::prev(tokens.second) && std::find(ignores.begin(), ignores.end(), std::next(iterator)->token) == ignores.end();
}

std::string BaseError::get_wiki_link() {
    std::stringstream ss;
    ss << "https://flint-lang.github.io/v" << MAJOR << "." << MINOR << "." << PATCH << "-" << VERSION;
    return ss.str();
}
