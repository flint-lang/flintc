#include "debug.hpp"

#include "types.hpp"

#include "error/error.hpp"
#include "error/error_type.hpp"
#include "lexer/lexer.hpp"

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

#include "parser/ast/statements/assignment_node.hpp"
#include "parser/ast/statements/declaration_node.hpp"
#include "parser/ast/statements/for_loop_node.hpp"
#include "parser/ast/statements/return_node.hpp"
#include "parser/ast/statements/statement_node.hpp"
#include "parser/ast/statements/while_node.hpp"

#include <cassert>
#include <iostream>
#include <ostream>
#include <typeinfo>
#include <memory>
#include <variant>

namespace Debug {
    /// get_string_container
    ///     Returns the given string inside a container of the given size
    std::string get_string_container(const unsigned int size, const std::string &value) {
        assert(size > value.size());
        std::string container(size, ' ');
        container.replace(0, value.size(), value);
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

    namespace AST {
        /// print_ast_tree
        ///     Prints the whole AST Tree recursively
        void print_program(const ProgramNode &program) {
            std::cout << "Program:\n";
            for(const std::unique_ptr<ASTNode> &node : program.definitions) {
                if(const auto *data_node = dynamic_cast<const DataNode*>(node.get())) {
                    print_data(*data_node);
                } else if (const auto *entity_node = dynamic_cast<const EntityNode*>(node.get())) {
                    print_entity(*entity_node);
                } else if (const auto *enum_node = dynamic_cast<const EnumNode*>(node.get())) {
                    print_enum(*enum_node);
                } else if (const auto *func_node = dynamic_cast<const FuncNode*>(node.get())) {
                    print_func(*func_node);
                } else if (const auto *function_node = dynamic_cast<const FunctionNode*>(node.get())) {
                    print_function(*function_node);
                } else if (const auto *import_node = dynamic_cast<const ImportNode*>(node.get())) {
                    print_import(*import_node);
                } else if (const auto *link_node = dynamic_cast<const LinkNode*>(node.get())) {
                    print_link(*link_node);
                } else if (const auto *variant_node = dynamic_cast<const VariantNode*>(node.get())) {
                    print_variant(*variant_node);
                } else {
                    throw_err(ERR_DEBUG);
                }
            }
        }

        /// print_data
        ///     Prints the content of the generated DataNode
        void print_data(const DataNode &data) {
            std::cout << "    Data: " << typeid(data).name() << "\n";
        }

        /// print_entity
        ///     Prints the content of the generated EntityNode
        void print_entity(const EntityNode &entity) {
            std::cout << "    Entity: " << typeid(entity).name() << "\n";

        }

        /// print_enum
        ///     Prints the content of the generated EnumNode
        void print_enum(const EnumNode &enum_node) {
            std::cout << "    Enum: " << typeid(enum_node).name() << "\n";

        }

        /// print_error
        ///     Prints the content of the generated ErrorNode
        void print_error(const ErrorNode &error) {
            std::cout << "    Error: " << typeid(error).name() << "\n";

        }

        /// print_func
        ///     Prints the content of the generated FuncNode
        void print_func(const FuncNode &func) {
            std::cout << "    Func: " << typeid(func).name() << "\n";

        }

        /// print_function
        ///     Prints the content of the generated FunctionNode
        void print_function(const FunctionNode &function) {
            std::cout << "    ";
            std::cout << get_string_container(15, "Function:");


            std::cout << std::endl;
        }

        /// print_import
        ///     Prints the content of the generated ImportNode
        void print_import(const ImportNode &import) {
            std::cout << "    ";
            std::cout << get_string_container(15, "Import:");
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
        void print_link(const LinkNode &link) {
            std::cout << "    Link: " << typeid(link).name() << "\n";

        }

        /// print_link
        ///     Prints the content of the generated VariantNode
        void print_variant(const VariantNode &variant) {
            std::cout << "    Variant: " << typeid(variant).name() << "\n";

        }
    }
}
