#include "debug.hpp"

#include "error/error.hpp"
#include "error/error_type.hpp"
#include "lexer/lexer_utils.hpp"
#include "parser/parser.hpp"
#include "result.hpp"
#include "types.hpp"

#include "parser/ast/definitions/data_node.hpp"
#include "parser/ast/definitions/entity_node.hpp"
#include "parser/ast/definitions/enum_node.hpp"
#include "parser/ast/definitions/error_node.hpp"
#include "parser/ast/definitions/func_node.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/ast/definitions/import_node.hpp"
#include "parser/ast/definitions/link_node.hpp"
#include "parser/ast/definitions/variant_node.hpp"

#include "parser/ast/expressions/binary_op_node.hpp"
#include "parser/ast/expressions/call_node_expression.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/expressions/literal_node.hpp"
#include "parser/ast/expressions/type_cast_node.hpp"
#include "parser/ast/expressions/unary_op_node.hpp"
#include "parser/ast/expressions/variable_node.hpp"

#include "parser/ast/statements/assignment_node.hpp"
#include "parser/ast/statements/catch_node.hpp"
#include "parser/ast/statements/declaration_node.hpp"
#include "parser/ast/statements/for_loop_node.hpp"
#include "parser/ast/statements/if_node.hpp"
#include "parser/ast/statements/return_node.hpp"
#include "parser/ast/statements/statement_node.hpp"
#include "parser/ast/statements/throw_node.hpp"
#include "parser/ast/statements/while_node.hpp"

#include "parser/ast/call_node_base.hpp"
#include "parser/ast/file_node.hpp"
#include "parser/ast/scope.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <typeinfo>
#include <variant>

namespace Debug {
    static const unsigned int C_SIZE = 35;

    /// get_string_container
    ///     Returns the given string inside a container of the given size
    std::string get_string_container(unsigned int size, const std::string &value) {
        if (value.size() > size) {
            size = value.size();
        }
        std::string container(size, ' ');
        container.replace(0, value.size(), value);
        return container;
    }

    /// print_in_container
    ///     Prints the given string in a container of the given size
    void print_in_container(unsigned int size, const std::string &str) {
        std::cout << get_string_container(size, str);
    }

    /// fill_container_with
    ///     Fills a container of a given size with a given character
    std::string fill_container_with(const unsigned int &size, const char &character) {
        std::string container(size, character);
        return container;
    }

    /// print_token_context_vector
    ///     Prints all the TokenContext elements of the vector to the console
    void print_token_context_vector(const token_list &tokens, const std::string &file_name) {
        if (!DEBUG_MODE) {
            return;
        }
        std::cout << YELLOW << "[Debug Info] Printing token vector of file '" << file_name << "'" << DEFAULT << std::endl;
        for (const TokenContext &tc : tokens) {
            std::string name = get_token_name(tc.type);

            std::string type = " | Type: '" + name + "' => " + std::to_string(static_cast<int>(tc.type));
            std::string type_container = get_string_container(30, type);

            std::cout << "Line: " << tc.line << " | Column: " << tc.column << type_container << " | Lexme: " << tc.lexme << "\n";
        }
    }

    /// print_tree_row
    ///
    void print_tree_row(const std::vector<TreeType> &types, TestResult *result) {
        std::string addition;
        for (const TreeType &type : types) {
            addition += " ";
            switch (type) {
                case VERT:
                    addition += tree_characters.at(type) + "  ";
                    break;
                case BRANCH:
                case SINGLE:
                    addition += tree_characters.at(type) + tree_characters.at(TreeType::HOR) + " ";
                    break;
                case HOR:
                    addition += tree_characters.at(type) + tree_characters.at(type) + tree_characters.at(type) + tree_characters.at(type);
                    break;
                default:
                    addition += "   ";
            }
        }
        if (result != nullptr) {
            result->append(addition);
        } else {
            std::cout << addition;
        }
    }

    /// print_tree_characters
    ///     Prints the tree characters to the console
    void print_tree_characters(const std::vector<TreeType> &types) {
        for (const TreeType &type : types) {
            std::cout << tree_characters.at(type);
        }
    }

    /// create_n_str
    ///     Takes the given string and puts it into a result string n times
    std::string create_n_str(unsigned int n, const std::string &str) {
        std::string ret;
        for (unsigned int i = 0; i < n; i++) {
            ret.append(str);
        }
        return ret;
    }

    namespace Dep {
        /// print_dep_tree
        ///     Prints the dependency tree and marks if a reference is direct or indirect (weak_ptr)
        void print_dep_tree(unsigned int indent_lvl, const std::variant<std::shared_ptr<DepNode>, std::weak_ptr<DepNode>> &dep_node) {
            if (!DEBUG_MODE) {
                return;
            }
            if (indent_lvl == 0) {
                std::cout << YELLOW << "[Debug Info] Printing depenceny tree" << DEFAULT << std::endl;
            }
            if (std::holds_alternative<std::weak_ptr<DepNode>>(dep_node)) {
                std::cout << std::get<std::weak_ptr<DepNode>>(dep_node).lock()->file_name << " [weak]" << std::endl;
                return;
            }

            std::shared_ptr<DepNode> dep_node_sh = std::get<std::shared_ptr<DepNode>>(dep_node);
            std::cout << dep_node_sh->file_name << std::endl;
            for (auto dep = dep_node_sh->dependencies.begin(); dep != dep_node_sh->dependencies.end(); ++dep) {
                for (int i = 0; i < indent_lvl; i++) {
                    std::cout << " " << tree_characters.at(VERT) << "  ";
                }
                if ((dep + 1) != dep_node_sh->dependencies.end()) {
                    std::cout << " " << tree_characters.at(BRANCH) << tree_characters.at(HOR) << " ";
                } else {
                    std::cout << " " << tree_characters.at(SINGLE) << tree_characters.at(HOR) << " ";
                }
                print_dep_tree(indent_lvl + 1, *dep);
            }
            if (indent_lvl == 0) {
                std::cout << std::endl;
            }
        }
    } // namespace Dep

    namespace AST {
        namespace {
            /// print_table_header
            ///     Prints the header of a table from the AST tree
            void print_table_header(const unsigned int &padding, const std::vector<std::pair<unsigned int, std::string>> &cells) {
                std::cout << get_string_container(padding, "");

                for (const auto &cell : cells) {
                    std::cout << get_string_container(cell.first, cell.second);
                    std::cout << "| ";
                }
                std::cout << std::endl;
                std::cout << get_string_container(padding, "");
                for (const auto &cell : cells) {
                    std::cout << fill_container_with(cell.first, '-');
                    std::cout << "|-";
                }
                std::cout << std::endl;
            }

            /// print_table_row
            ///     Prints a table row
            void print_table_row(const unsigned int &padding, const std::vector<std::pair<unsigned int, std::string>> &cells) {
                std::cout << get_string_container(padding, "");

                for (const auto &cell : cells) {
                    std::cout << get_string_container(cell.first, cell.second);
                    std::cout << "| ";
                }
                std::cout << std::endl;
            }

            /// print_header
            ///     Prints the header of the AST node (the left part incl. the header name)
            void print_header(unsigned int indent_lvl, uint2 empty, const std::string &header) {
                // print "normal" verts up to the "empty's" first part
                for (unsigned int i = 0; i < empty.first; i++) {
                    print_tree_row({VERT}, nullptr);
                }
                // print "empty" for all the elements from empty.first -> empty.second
                for (unsigned int i = empty.first; i < (empty.second < indent_lvl ? empty.second : indent_lvl); i++) {
                    print_tree_row({NONE}, nullptr);
                }
                // print "vert" for all elements from empty.second to indent_lvl
                for (unsigned int i = empty.second; i < indent_lvl; i++) {
                    print_tree_row({VERT}, nullptr);
                }
                // print either "single" or "branch" depending on the emptys second value
                if (empty.second > indent_lvl) {
                    print_tree_row({SINGLE}, nullptr);
                } else {
                    print_tree_row({BRANCH}, nullptr);
                }
                std::cout << header;
                if (header.size() + (4 * indent_lvl) > C_SIZE) {
                    std::cout << tree_characters.at(HOR);
                } else {
                    std::cout << create_n_str(C_SIZE - header.size() - (4 * indent_lvl), tree_characters.at(HOR));
                }
                std::cout << "> ";
            }
        } // namespace

        /// print_ast_tree
        ///     Prints the whole AST Tree recursively
        void print_file(const FileNode &file) {
            if (!DEBUG_MODE) {
                return;
            }
            std::cout << YELLOW << "[Debug Info] Printing AST of file '" << file.file_name << "'" << DEFAULT << std::endl;
            std::cout << "File \"" << file.file_name << "\"" << std::endl;
            unsigned int counter = 0;
            uint2 empty = {0, 0};
            for (const std::unique_ptr<ASTNode> &node : file.definitions) {
                if (++counter == file.definitions.size()) {
                    empty.first = 0;
                    empty.second = 2;
                }
                if (const auto *data_node = dynamic_cast<const DataNode *>(node.get())) {
                    print_data(0, *data_node);
                } else if (const auto *entity_node = dynamic_cast<const EntityNode *>(node.get())) {
                    print_entity(0, *entity_node);
                } else if (const auto *enum_node = dynamic_cast<const EnumNode *>(node.get())) {
                    print_enum(0, *enum_node);
                } else if (const auto *func_node = dynamic_cast<const FuncNode *>(node.get())) {
                    print_func(0, *func_node);
                } else if (const auto *function_node = dynamic_cast<const FunctionNode *>(node.get())) {
                    print_function(0, empty, *function_node);
                } else if (const auto *import_node = dynamic_cast<const ImportNode *>(node.get())) {
                    print_import(0, *import_node);
                } else if (const auto *link_node = dynamic_cast<const LinkNode *>(node.get())) {
                    print_link(0, empty, *link_node);
                } else if (const auto *variant_node = dynamic_cast<const VariantNode *>(node.get())) {
                    print_variant(0, *variant_node);
                } else {
                    THROW_BASIC_ERR(ERR_DEBUG);
                    return;
                }
            }
            std::cout << std::endl;
        }

        // --- EXPRESSIONS ---

        void print_variable(unsigned int indent_lvl, uint2 empty, const VariableNode &var) {
            print_header(indent_lvl, empty, "Variable ");
            std::cout << var.name;
            std::cout << std::endl;
        }

        void print_unary_op(unsigned int indent_lvl, uint2 empty, const UnaryOpNode &unary) {
            print_header(indent_lvl, empty, "UnOp ");
            std::cout << "operation: " << get_token_name(unary.operator_token);
            std::cout << std::endl;

            empty.second = ++indent_lvl;
            print_expression(indent_lvl, empty, unary.operand);
        }

        void print_literal(unsigned int indent_lvl, uint2 empty, const LiteralNode &lit) {
            print_header(indent_lvl, empty, "Lit ");
            std::cout << "value: ";
            if (std::holds_alternative<int>(lit.value)) {
                std::cout << std::get<int>(lit.value);
            } else if (std::holds_alternative<float>(lit.value)) {
                std::cout << std::get<float>(lit.value);
            } else if (std::holds_alternative<bool>(lit.value)) {
                std::cout << (std::get<bool>(lit.value) ? "true" : "false");
            } else if (std::holds_alternative<std::string>(lit.value)) {
                std::cout << std::get<std::string>(lit.value);
            }
            std::cout << std::endl;
        }

        void print_call(unsigned int indent_lvl, uint2 empty, const CallNodeBase &call) {
            print_header(indent_lvl, empty, "Call ");
            std::cout << "'" << call.function_name << "' with args";
            std::cout << std::endl;
            unsigned int counter = 0;
            for (const auto &arg : call.arguments) {
                if (++counter == call.arguments.size()) {
                    empty.second = indent_lvl + 2;
                }
                print_expression(indent_lvl + 1, empty, arg);
            }
        }

        void print_binary_op(unsigned int indent_lvl, uint2 empty, const BinaryOpNode &bin) {
            print_header(indent_lvl, empty, "BinOp ");
            std::cout << get_token_name(bin.operator_token);
            std::cout << std::endl;
            print_header(indent_lvl + 1, empty, "LHS ");
            std::cout << std::endl;
            empty.second = indent_lvl + 1;
            print_expression(indent_lvl + 2, empty, bin.left);
            empty.second = indent_lvl + 2;
            print_header(indent_lvl + 1, empty, "RHS ");
            std::cout << std::endl;
            empty.second = indent_lvl + 3;
            print_expression(indent_lvl + 2, empty, bin.right);
        }

        void print_type_cast(unsigned int indent_lvl, uint2 empty, const TypeCastNode &cast) {
            print_header(indent_lvl, empty, "TypeCast ");
            std::cout << cast.type << std::endl;
            empty.second = indent_lvl + 2;
            print_expression(indent_lvl + 1, empty, cast.expr);
        }

        void print_expression(unsigned int indent_lvl, uint2 empty, const std::unique_ptr<ExpressionNode> &expr) {
            if (const auto *variable_node = dynamic_cast<const VariableNode *>(expr.get())) {
                print_variable(indent_lvl, empty, *variable_node);
            } else if (const auto *unary_op_node = dynamic_cast<const UnaryOpNode *>(expr.get())) {
                print_unary_op(indent_lvl, empty, *unary_op_node);
            } else if (const auto *literal_node = dynamic_cast<const LiteralNode *>(expr.get())) {
                print_literal(indent_lvl, empty, *literal_node);
            } else if (const auto *call_node = dynamic_cast<const CallNodeExpression *>(expr.get())) {
                print_call(indent_lvl, empty, *dynamic_cast<const CallNodeBase *>(call_node));
            } else if (const auto *binary_op_node = dynamic_cast<const BinaryOpNode *>(expr.get())) {
                print_binary_op(indent_lvl, empty, *binary_op_node);
            } else if (const auto *type_cast_node = dynamic_cast<const TypeCastNode *>(expr.get())) {
                print_type_cast(indent_lvl, empty, *type_cast_node);
            } else {
                THROW_BASIC_ERR(ERR_DEBUG);
                return;
            }
        }

        // --- STATEMENTS ---

        void print_throw(unsigned int indent_lvl, uint2 empty, const ThrowNode &return_node) {
            print_header(indent_lvl, empty, "Throw ");
            std::cout << "throw";
            std::cout << std::endl;

            empty.second = indent_lvl + 2;
            print_expression(++indent_lvl, empty, return_node.throw_value);
        }

        void print_return(unsigned int indent_lvl, uint2 empty, const ReturnNode &return_node) {
            print_header(indent_lvl, empty, "Return ");
            std::cout << "return";
            std::cout << std::endl;

            empty.second = indent_lvl + 2;
            print_expression(++indent_lvl, empty, return_node.return_value);
        }

        void print_if(unsigned int indent_lvl, uint2 empty, const IfNode &if_node) {
            print_header(indent_lvl, empty, "If ");
            std::cout << "if " << std::endl;

            empty.second = indent_lvl + 2;
            print_expression(indent_lvl + 1, empty, if_node.condition);

            empty.second = indent_lvl + 1;
            print_header(indent_lvl, empty, "Then ");
            std::cout << std::endl;
            print_body(indent_lvl + 1, empty, if_node.then_scope->body);

            // empty.second = indent_lvl + 1;
            print_header(indent_lvl, empty, "Else ");
            std::cout << std::endl;

            // empty.second = indent_lvl + 2;
            if (if_node.else_scope.has_value()) {
                if (std::holds_alternative<std::unique_ptr<Scope>>(if_node.else_scope.value())) {
                    print_body(indent_lvl + 1, empty, std::get<std::unique_ptr<Scope>>(if_node.else_scope.value())->body);
                } else {
                    print_if(indent_lvl + 1, empty, *std::get<std::unique_ptr<IfNode>>(if_node.else_scope.value()));
                }
            }
        }

        void print_while(unsigned int indent_lvl, uint2 empty, const WhileNode &while_node) {
            print_header(indent_lvl, empty, "While ");
            std::cout << "while" << std::endl;
            empty.second = indent_lvl + 1;
            print_expression(indent_lvl + 1, empty, while_node.condition);

            empty.second = indent_lvl;
            print_header(indent_lvl, empty, "Do ");
            std::cout << "do" << std::endl;
            empty.second = indent_lvl + 1;
            print_body(indent_lvl + 1, empty, while_node.scope->body);
        }

        void print_for(unsigned int indent_lvl, uint2 empty, const ForLoopNode &for_node) {
            print_header(indent_lvl, empty, "For ");
            std::cout << "for ";
            std::cout << for_node.iterator_name;
            std::cout << " in ";
            print_expression(0, {0, 0}, for_node.iterable);

            empty.second = indent_lvl + 1;
            print_body(indent_lvl + 1, empty, for_node.scope->body);
        }

        void print_catch(unsigned int indent_lvl, uint2 empty, const CatchNode &catch_node) {
            std::optional<CallNodeBase *> call_node = Parser::get_call_from_id(catch_node.call_id);
            if (!call_node.has_value()) {
                return;
            }
            print_header(indent_lvl, empty, "Catch ");
            std::cout << "catch '";
            std::cout << call_node.value()->function_name;
            std::cout << "'";
            if (catch_node.var_name.has_value()) {
                std::cout << " in '";
                std::cout << catch_node.var_name.value();
                std::cout << "'";
            }
            std::cout << std::endl;

            empty.second = indent_lvl + 1;
            print_body(indent_lvl + 1, empty, catch_node.scope->body);
        }

        void print_assignment(unsigned int indent_lvl, uint2 empty, const AssignmentNode &assign) {
            print_header(indent_lvl, empty, "Assign ");
            std::cout << "'" << assign.name << "' to be";
            std::cout << std::endl;

            empty.first++;
            empty.second = indent_lvl + 2;
            print_expression(++indent_lvl, empty, assign.expression);
        }

        void print_declaration(unsigned int indent_lvl, uint2 empty, const DeclarationNode &decl) {
            print_header(indent_lvl, empty, "Decl ");
            std::cout << "'" << decl.type << " ";
            std::cout << decl.name << "' to be";
            std::cout << std::endl;

            empty.first++;
            empty.second = indent_lvl + 2;
            print_expression(++indent_lvl, empty, decl.initializer);
        }

        void print_statement(unsigned int indent_lvl, uint2 empty, const std::unique_ptr<StatementNode> &statement) {
            if (const auto *return_node = dynamic_cast<const ReturnNode *>(statement.get())) {
                print_return(indent_lvl, empty, *return_node);
            } else if (const auto *if_node = dynamic_cast<const IfNode *>(statement.get())) {
                print_if(indent_lvl, empty, *if_node);
            } else if (const auto *while_node = dynamic_cast<const WhileNode *>(statement.get())) {
                print_while(indent_lvl, empty, *while_node);
            } else if (const auto *for_node = dynamic_cast<const ForLoopNode *>(statement.get())) {
                print_for(indent_lvl, empty, *for_node);
            } else if (const auto *assignment = dynamic_cast<const AssignmentNode *>(statement.get())) {
                print_assignment(indent_lvl, empty, *assignment);
            } else if (const auto *declaration = dynamic_cast<const DeclarationNode *>(statement.get())) {
                print_declaration(indent_lvl, empty, *declaration);
            } else if (const auto *throw_node = dynamic_cast<const ThrowNode *>(statement.get())) {
                print_throw(indent_lvl, empty, *throw_node);
            } else if (const auto *catch_node = dynamic_cast<const CatchNode *>(statement.get())) {
                print_catch(indent_lvl, empty, *catch_node);
            } else if (const auto *call_node = dynamic_cast<const CallNodeBase *>(statement.get())) {
                print_call(indent_lvl, empty, *call_node);
            } else {
                THROW_BASIC_ERR(ERR_DEBUG);
                return;
            }
        }

        void print_body(unsigned int indent_lvl, uint2 empty, const std::vector<std::unique_ptr<StatementNode>> &body) {
            unsigned int counter = 0;
            for (const auto &body_line : body) {
                if (++counter == body.size()) {
                    if (dynamic_cast<const IfNode *>(body_line.get()) != nullptr) {
                        empty.second = indent_lvl + 1;
                    }
                }
                print_statement(indent_lvl, empty, body_line);
            }
        }

        // --- DEFINITIONS ---

        /// print_data
        ///     Prints the content of the generated DataNode
        void print_data(unsigned int indent_lvl, const DataNode &data) {
            std::cout << "    Data: " << typeid(data).name() << "\n";
        }

        /// print_entity
        ///     Prints the content of the generated EntityNode
        void print_entity(unsigned int indent_lvl, const EntityNode &entity) {
            std::cout << "    Entity: " << typeid(entity).name() << "\n";
        }

        /// print_enum
        ///     Prints the content of the generated EnumNode
        void print_enum(unsigned int indent_lvl, const EnumNode &enum_node) {
            std::cout << "    Enum: " << typeid(enum_node).name() << "\n";
        }

        /// print_error
        ///     Prints the content of the generated ErrorNode
        void print_error(unsigned int indent_lvl, const ErrorNode &error) {
            std::cout << "    Error: " << typeid(error).name() << "\n";
        }

        /// print_func
        ///     Prints the content of the generated FuncNode
        void print_func(unsigned int indent_lvl, const FuncNode &func) {
            std::cout << "    Func: " << typeid(func).name() << "\n";
        }

        /// print_function
        ///     Prints the content of the generated FunctionNode
        void print_function(unsigned int indent_lvl, uint2 empty, const FunctionNode &function) {
            print_header(indent_lvl, empty, "Function ");

            if (function.is_aligned) {
                std::cout << "aligned ";
            }
            if (function.is_const) {
                std::cout << "const ";
            }
            std::cout << function.name << "(";
            size_t counter = 0;
            for (const std::pair<std::string, std::string> &param : function.parameters) {
                std::cout << param.first << " " << param.second;
                if (++counter != function.parameters.size()) {
                    std::cout << ", ";
                }
            }
            std::cout << ")";
            if (!function.return_types.empty()) {
                std::cout << " -> ";
            }
            if (function.return_types.size() > 1) {
                std::cout << "(";
            }
            counter = 0;
            for (const std::string &ret : function.return_types) {
                std::cout << ret;
                if (++counter != function.return_types.size()) {
                    std::cout << ", ";
                }
            }
            if (function.return_types.size() > 1) {
                std::cout << ")";
            }
            std::cout << std::endl;

            // The function body
            empty.first++;
            empty.second = ++indent_lvl;
            print_body(indent_lvl, empty, function.scope->body);
        }

        /// print_import
        ///     Prints the content of the generated ImportNode
        void print_import(unsigned int indent_lvl, const ImportNode &import) {
            print_header(indent_lvl, {0, 0}, "Import ");

            if (std::holds_alternative<std::string>(import.path)) {
                std::cout << "\"" << std::get<std::string>(import.path) << "\"";
            } else if (std::holds_alternative<std::vector<std::string>>(import.path)) {
                std::vector<std::string> path_vector = std::get<std::vector<std::string>>(import.path);
                auto iterator = path_vector.begin();
                while (iterator != path_vector.end()) {
                    if (iterator != path_vector.begin()) {
                        std::cout << "::";
                    }
                    std::cout << *iterator;
                    ++iterator;
                }
            }

            std::cout << std::endl;
        }

        /// print_link
        ///     Prints the content of the generated LinkNode
        void print_link(unsigned int indent_lvl, uint2 empty, const LinkNode &link) {
            std::cout << "    Link: " << typeid(link).name() << "\n";
        }

        /// print_link
        ///     Prints the content of the generated VariantNode
        void print_variant(unsigned int indent_lvl, const VariantNode &variant) {
            std::cout << "    Variant: " << typeid(variant).name() << "\n";
        }
    } // namespace AST
} // namespace Debug
