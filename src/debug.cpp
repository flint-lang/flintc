#include "debug.hpp"

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
#include "parser/ast/node_type.hpp"
#include "parser/ast/statements/assignment_node.hpp"
#include "parser/ast/statements/declaration_node.hpp"
#include "parser/ast/statements/for_loop_node.hpp"
#include "parser/ast/statements/return_node.hpp"
#include "parser/ast/statements/statement_node.hpp"
#include "parser/ast/statements/while_node.hpp"
#include "types.hpp"

#include <iostream>
#include <typeinfo>

namespace Debug {
    /// print_token_context_vector
    ///     Prints all the TokenContext elements of the vector to the console
    void print_token_context_vector(const token_list &tokens) {
        for(const TokenContext &tc : tokens) {
            std::string name = get_token_name(tc.type);

            std::string type_container(30, ' ');
            std::string type = " | Type: '" + name + "' => " + std::to_string(static_cast<int>(tc.type));
            type_container.replace(0, type.length(), type);

            std::cout << "Line: " << tc.line << type_container << " | Lexme: " << tc.lexme << "\n";
        }
    }

    namespace AST {
        /// print_ast_tree
        ///     Prints the whole AST Tree recursively
        void print_ast_tree(const ProgramNode &program) {
            std::cout << "Program:\n";
            for(const ASTNode &node : program.definitions) {
                std::cout << "    ";
                NodeType type = get_ast_type(node);

                switch(type) {
                    default:
                    case NodeType::NONE: {
                        throw_err(ERR_NOT_IMPLEMENTED_YET);
                        break;
                    }
                    case NodeType::DATA: {
                        print_data(dynamic_cast<const DataNode&>(node));
                        break;
                    }
                    case NodeType::ENTITY: {
                        print_entity(dynamic_cast<const EntityNode&>(node));
                        break;
                    }
                    case NodeType::ENUM: {
                        print_enum(dynamic_cast<const EnumNode&>(node));
                        break;
                    }
                    case NodeType::ERROR: {
                        print_error(dynamic_cast<const ErrorNode&>(node));
                        break;
                    }
                    case NodeType::FUNC: {
                        print_func(dynamic_cast<const FuncNode&>(node));
                        break;
                    }
                    case NodeType::FUNCTION: {
                        print_function(dynamic_cast<const FunctionNode&>(node));
                        break;
                    }
                    case NodeType::IMPORT: {
                        print_import(dynamic_cast<const ImportNode&>(node));
                        break;
                    }
                    case NodeType::LINK: {
                        print_link(dynamic_cast<const LinkNode&>(node));
                        break;
                    }
                    case NodeType::VARIANT: {
                        print_variant(dynamic_cast<const VariantNode&>(node));
                        break;
                    }
                }
            }
        }

        /// get_ast_type
        ///     Returns the NodeType of the given ASTNode
        NodeType get_ast_type(const ASTNode &node) {
            if(typeid(node) == typeid(DataNode)) {
                return NodeType::DATA;
            }
            if(typeid(node) == typeid(EntityNode)) {
                return NodeType::ENTITY;
            }
            if(typeid(node) == typeid(EnumNode)) {
                return NodeType::ENUM;
            }
            if(typeid(node) == typeid(ErrorNode)) {
                return NodeType::ERROR;
            }
            if(typeid(node) == typeid(FuncNode)) {
                return NodeType::FUNC;
            }
            if(typeid(node) == typeid(FunctionNode)) {
                return NodeType::FUNCTION;
            }
            if(typeid(node) == typeid(ImportNode)) {
                return NodeType::IMPORT;
            }
            if(typeid(node) == typeid(LinkNode)) {
                return NodeType::LINK;
            }
            if(typeid(node) == typeid(VariantNode)) {
                return NodeType::VARIANT;
            }
            // TODO: OptNode

            return NodeType::NONE;
        }

        /// get_expression_type
        ///     Returns the NodeType of the given ExpressionNode
        NodeType get_expression_type(const ExpressionNode &node) {
            if(typeid(node) == typeid(BinaryOpNode)) {
                return NodeType::BINARY_OP;
            }
            if(typeid(node) == typeid(CallNode)) {
                return NodeType::CALL;
            }
            if(typeid(node) == typeid(LiteralNode)) {
                return NodeType::LITERAL;
            }
            if(typeid(node) == typeid(UnaryOpNode)) {
                return NodeType::UNARY_OP;
            }
            if(typeid(node) == typeid(VariableNode)) {
                return NodeType::VARIABLE;
            }

            return NodeType::NONE;
        }

        /// get_statement_type
        ///     Returns the NodeType of the given StatementNode
        NodeType get_statement_type(const StatementNode &node) {
            if(typeid(node) == typeid(AssignmentNode)) {
                return NodeType::ASSIGNMENT;
            }
            if(typeid(node) == typeid(DeclarationNode)) {
                return NodeType::DECL_EXPLICIT;
            }
            if(typeid(node) == typeid(ForLoopNode)) {
                return NodeType::FOR_LOOP;
            }
            if(typeid(node) == typeid(ReturnNode)) {
                return NodeType::RETURN;
            }
            if(typeid(node) == typeid(WhileNode)) {
                return NodeType::WHILE_LOOP;
            }

            return NodeType::NONE;
        }

        /// print_data
        ///     Prints the content of the generated DataNode
        void print_data(const DataNode &data) {

        }

        /// print_entity
        ///     Prints the content of the generated EntityNode
        void print_entity(const EntityNode &entity) {

        }

        /// print_enum
        ///     Prints the content of the generated EnumNode
        void print_enum(const EnumNode &enum_node) {

        }

        /// print_error
        ///     Prints the content of the generated ErrorNode
        void print_error(const ErrorNode &error) {

        }

        /// print_func
        ///     Prints the content of the generated FuncNode
        void print_func(const FuncNode &func) {

        }

        /// print_function
        ///     Prints the content of the generated FunctionNode
        void print_function(const FunctionNode &function) {

        }

        /// print_import
        ///     Prints the content of the generated ImportNode
        void print_import(const ImportNode &import) {

        }

        /// print_link
        ///     Prints the content of the generated LinkNode
        void print_link(const LinkNode &link) {

        }

        /// print_link
        ///     Prints the content of the generated VariantNode
        void print_variant(const VariantNode &variant) {

        }
    }
}
