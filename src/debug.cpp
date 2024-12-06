#include "debug.hpp"

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

#include <vector>
#include <iostream>

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
    }
}
