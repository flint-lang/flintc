#include "debug.hpp"

#include "types.hpp"

#include "error/error.hpp"
#include "error/error_type.hpp"
#include "lexer/lexer.hpp"
#include "test_utils.hpp"

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
#include "parser/ast/expressions/call_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/expressions/literal_node.hpp"
#include "parser/ast/expressions/unary_op_node.hpp"
#include "parser/ast/expressions/variable_node.hpp"

#include "parser/ast/statements/if_node.hpp"
#include "parser/ast/statements/assignment_node.hpp"
#include "parser/ast/statements/declaration_node.hpp"
#include "parser/ast/statements/for_loop_node.hpp"
#include "parser/ast/statements/return_node.hpp"
#include "parser/ast/statements/statement_node.hpp"
#include "parser/ast/statements/while_node.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <ostream>
#include <string>
#include <typeinfo>
#include <memory>
#include <variant>

namespace Debug {
    static const unsigned int C_SIZE = 25;

    /// get_string_container
    ///     Returns the given string inside a container of the given size
    std::string get_string_container(unsigned int size, const std::string &value) {
        if(value.size() > size) {
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
    void print_token_context_vector(const token_list &tokens) {
        for(const TokenContext &tc : tokens) {
            std::string name = get_token_name(tc.type);

            std::string type = " | Type: '" + name + "' => " + std::to_string(static_cast<int>(tc.type));
            std::string type_container = get_string_container(30, type);

            std::cout << "Line: " << tc.line << type_container << " | Lexme: " << tc.lexme << "\n";
        }
    }

    /// print_tree_row
    ///
    void print_tree_row(const std::vector<TreeType> &types, bool is_test) {
        std::string addition;
        for(const TreeType &type: types) {
            addition += " ";
            switch(type) {
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
                    addition += "    ";
            }
        }
        if(is_test) {
            TestUtils::append_string(addition);
        } else {
            std::cout << addition;
        }
    }

    /// print_tree_characters
    ///     Prints the tree characters to the console
    void print_tree_characters(const std::vector<TreeType> &types) {
        for(const TreeType &type : types) {
            std::cout << tree_characters.at(type);
        }
    }

    /// create_n_str
    ///     Takes the given string and puts it into a result string n times
    std::string create_n_str(unsigned int n, const std::string &str) {
        std::string ret;
        for(unsigned int i = 0; i < n; i++) {
            ret.append(str);
        }
        return ret;
    }

    namespace AST {
        namespace {
            /// print_table_header
            ///     Prints the header of a table from the AST tree
            void print_table_header(const unsigned int &padding, const std::vector<std::pair<unsigned int, std::string>> &cells) {
                std::cout << get_string_container(padding, "");

                for(const auto &cell : cells) {
                    std::cout << get_string_container(cell.first, cell.second);
                    std::cout << "| ";
                }
                std::cout << std::endl;
                std::cout << get_string_container(padding, "");
                for(const auto &cell : cells) {
                    std::cout << fill_container_with(cell.first, '-');
                    std::cout << "|-";
                }
                std::cout << std::endl;
            }

            /// print_table_row
            ///     Prints a table row
            void print_table_row(const unsigned int &padding, const std::vector<std::pair<unsigned int, std::string>> &cells) {
                std::cout << get_string_container(padding, "");

                for(const auto &cell : cells) {
                    std::cout << get_string_container(cell.first, cell.second);
                    std::cout << "| ";
                }
                std::cout << std::endl;
            }

            /// print_header
            ///     Prints the header of the AST node (the left part incl. the header name)
            void print_header(unsigned int indent_lvl, const std::string &header) {
                for(int i = 0; i < indent_lvl - 1; i++) {
                    print_tree_row({VERT}, false);
                }
                print_tree_row({BRANCH}, false);
                std::cout << header;
                print_tree_characters({BRANCH});
                std::cout << create_n_str(C_SIZE - header.size() - (4 * indent_lvl), tree_characters.at(HOR));
                //print_tree_characters({ARROW});
                std::cout << "> ";
            }
        }
        /// print_ast_tree
        ///     Prints the whole AST Tree recursively
        void print_program(const ProgramNode &program) {
            std::cout << "Program:\n";
            for(const std::unique_ptr<ASTNode> &node : program.definitions) {
                if(const auto *data_node = dynamic_cast<const DataNode*>(node.get())) {
                    print_data(1, *data_node);
                } else if (const auto *entity_node = dynamic_cast<const EntityNode*>(node.get())) {
                    print_entity(1, *entity_node);
                } else if (const auto *enum_node = dynamic_cast<const EnumNode*>(node.get())) {
                    print_enum(1, *enum_node);
                } else if (const auto *func_node = dynamic_cast<const FuncNode*>(node.get())) {
                    print_func(1, *func_node);
                } else if (const auto *function_node = dynamic_cast<const FunctionNode*>(node.get())) {
                    print_function(1, *function_node);
                } else if (const auto *import_node = dynamic_cast<const ImportNode*>(node.get())) {
                    print_import(1, *import_node);
                } else if (const auto *link_node = dynamic_cast<const LinkNode*>(node.get())) {
                    print_link(1, *link_node);
                } else if (const auto *variant_node = dynamic_cast<const VariantNode*>(node.get())) {
                    print_variant(1, *variant_node);
                } else {
                    throw_err(ERR_DEBUG);
                }
            }
        }


        // --- EXPRESSIONS ---

        void print_variable(unsigned int indent_lvl, const VariableNode &var) {

        }

        void print_unary_op(unsigned int indent_lvl, const UnaryOpNode &unary) {

        }

        void print_literal(unsigned int indent_lvl, const LiteralNode &lit) {

        }

        void print_call(unsigned int indent_lvl, const CallNode &call) {

        }

        void print_binary_op(unsigned int indent_lvl, const BinaryOpNode &bin) {

        }

        void print_expression(unsigned int indent_lvl, const std::unique_ptr<ExpressionNode> &expr) {
        }


        // --- STATEMENTS ---

        void print_return(unsigned int indent_lvl, const ReturnNode &return_node) {

        }

        void print_if(unsigned int indent_lvl, const IfNode &if_node) {

        }

        void print_while(unsigned int indent_lvl, const WhileNode &while_node) {

        }

        void print_for(unsigned int indent_lvl, const ForLoopNode &for_node) {

        }

        void print_assignment(unsigned int indent_lvl, const AssignmentNode &assign) {

        }

        void print_declaration(unsigned int indent_lvl, const DeclarationNode &decl) {

        }

        void print_statement(unsigned int indent_lvl, const std::unique_ptr<StatementNode> &statement) {
        }

        void print_body(unsigned int indent_lvl, const std::vector<std::variant<std::unique_ptr<StatementNode>, std::unique_ptr<CallNode>>> &body) {
            for(const auto &body_line : body) {
                if(std::holds_alternative<std::unique_ptr<StatementNode>>(body_line)) {
                    print_statement(indent_lvl, std::get<std::unique_ptr<StatementNode>>(body_line));
                } else if (std::holds_alternative<std::unique_ptr<CallNode>>(body_line)) {
                    print_call(indent_lvl, *std::get<std::unique_ptr<CallNode>>(body_line));
                }
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
        void print_function(unsigned int indent_lvl, const FunctionNode &function) {
            print_header(indent_lvl, "Function ");

            if(function.is_aligned) {
                std::cout << "aligned ";
            }
            if(function.is_const) {
                std::cout << "const ";
            }
            std::cout << function.name << "(";
            size_t counter = 0;
            for(const std::pair<std::string, std::string> &param: function.parameters) {
                std::cout << param.first << " " << param.second;
                if(++counter != function.parameters.size()) {
                    std::cout << ", ";
                }
            }
            std::cout << ")";
            if(!function.return_types.empty()) {
                std::cout <<" -> ";
            }
            if(function.return_types.size() > 1) {
                std::cout << "(";
            }
            counter = 0;
            for(const std::string &ret: function.return_types) {
                std::cout << ret;
                if(++counter != function.return_types.size()) {
                    std::cout << ", ";
                }
            }
            if(function.return_types.size() > 1) {
                std::cout << ")";
            }
            std::cout << std::endl;

            // The function body
            print_body(++indent_lvl, function.body);
        }

        /// print_import
        ///     Prints the content of the generated ImportNode
        void print_import(unsigned int indent_lvl, const ImportNode &import) {
            print_header(indent_lvl, "Import ");

            if(std::holds_alternative<std::string>(import.path)) {
                std::cout << std::get<std::string>(import.path);
            } else if (std::holds_alternative<std::vector<std::string>>(import.path)) {
                std::vector<std::string> path_vector = std::get<std::vector<std::string>>(import.path);
                auto iterator = path_vector.begin();
                while(iterator != path_vector.end()) {
                    if(iterator != path_vector.begin()) {
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
        void print_link(unsigned int indent_lvl, const LinkNode &link) {
            std::cout << "    Link: " << typeid(link).name() << "\n";

        }

        /// print_link
        ///     Prints the content of the generated VariantNode
        void print_variant(unsigned int indent_lvl, const VariantNode &variant) {
            std::cout << "    Variant: " << typeid(variant).name() << "\n";

        }
    }
}
