#include "debug.hpp"

#include "error/error.hpp"
#include "error/error_type.hpp"
#include "lexer/lexer_utils.hpp"
#include "parser/ast/scope.hpp"
#include "parser/parser.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <mutex>
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
                for (unsigned int i = 0; i < indent_lvl; i++) {
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
        namespace Local {
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
                std::cout << TextFormat::BOLD_START << header << TextFormat::BOLD_END;
                if (header.size() + (4 * indent_lvl) > C_SIZE) {
                    std::cout << tree_characters.at(HOR);
                } else {
                    std::cout << create_n_str(C_SIZE - header.size() - (4 * indent_lvl), tree_characters.at(HOR));
                }
                std::cout << "> ";
            }

            void print_type(const std::variant<std::string, std::vector<std::string>> &type) {
                if (std::holds_alternative<std::string>(type)) {
                    std::cout << std::get<std::string>(type);
                } else {
                    const std::vector<std::string> &types = std::get<std::vector<std::string>>(type);
                    std::cout << "(";
                    for (auto it = types.begin(); it != types.end(); ++it) {
                        if (it != types.begin()) {
                            std::cout << ", ";
                        }
                        std::cout << *it;
                    }
                    std::cout << ")";
                }
            }
        } // namespace Local

        void print_all_files() {
            std::lock_guard<std::mutex> lock(Resolver::file_map_mutex);
            for (const auto &file_pair : Resolver::file_map) {
                print_file(file_pair.second);
            }
        }

        /// print_file
        ///     Prints the AST of the given file node
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
                    print_data(0, empty, *data_node);
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
                } else if (const auto *test_node = dynamic_cast<const TestNode *>(node.get())) {
                    print_test(0, empty, *test_node);
                } else {
                    THROW_BASIC_ERR(ERR_DEBUG);
                    return;
                }
            }
            std::cout << std::endl;
        }

        // --- EXPRESSIONS ---

        void print_variable(unsigned int indent_lvl, uint2 empty, const VariableNode &var) {
            Local::print_header(indent_lvl, empty, "Variable ");
            std::cout << var.name;
            std::cout << std::endl;
        }

        void print_unary_op(unsigned int indent_lvl, uint2 empty, const UnaryOpBase &unary) {
            Local::print_header(indent_lvl, empty, "UnOp ");
            std::cout << "operation: " << get_token_name(unary.operator_token);
            std::cout << std::endl;

            empty.second = ++indent_lvl;
            print_expression(indent_lvl, empty, unary.operand);
        }

        void print_literal(unsigned int indent_lvl, uint2 empty, const LiteralNode &lit) {
            Local::print_header(indent_lvl, empty, "Lit ");
            std::cout << std::get<std::string>(lit.type) << ": ";
            if (std::holds_alternative<int>(lit.value)) {
                std::cout << std::get<int>(lit.value);
            } else if (std::holds_alternative<float>(lit.value)) {
                std::cout << std::get<float>(lit.value);
            } else if (std::holds_alternative<bool>(lit.value)) {
                std::cout << (std::get<bool>(lit.value) ? "true" : "false");
            } else if (std::holds_alternative<std::string>(lit.value)) {
                std::cout << std::get<std::string>(lit.value);
            }
            if (lit.is_folded) {
                std::cout << " [folded]";
            }
            std::cout << std::endl;
        }

        void print_call(unsigned int indent_lvl, uint2 empty, const CallNodeBase &call) {
            Local::print_header(indent_lvl, empty, "Call ");
            std::cout << "'" << call.function_name << "(";
            for (auto it = call.arguments.begin(); it != call.arguments.end(); ++it) {
                if (it != call.arguments.begin()) {
                    std::cout << ", ";
                }
                std::cout << (it->second ? "ref" : "val");
            }
            std::cout << ")' [c" << call.call_id << "] in [s" << call.scope_id << "]";
            if (!call.arguments.empty()) {
                std::cout << " with args";
            }
            std::cout << std::endl;
            unsigned int counter = 0;
            for (const auto &arg : call.arguments) {
                if (++counter == call.arguments.size()) {
                    empty.second = indent_lvl + 2;
                }
                print_expression(indent_lvl + 1, empty, arg.first);
            }
        }

        void print_binary_op(unsigned int indent_lvl, uint2 empty, const BinaryOpNode &bin) {
            Local::print_header(indent_lvl, empty, "BinOp ");
            std::cout << get_token_name(bin.operator_token);
            std::cout << std::endl;
            Local::print_header(indent_lvl + 1, empty, "LHS ");
            std::cout << std::endl;
            empty.second = indent_lvl + 1;
            print_expression(indent_lvl + 2, empty, bin.left);
            empty.second = indent_lvl + 2;
            Local::print_header(indent_lvl + 1, empty, "RHS ");
            std::cout << std::endl;
            empty.second = indent_lvl + 3;
            print_expression(indent_lvl + 2, empty, bin.right);
        }

        void print_type_cast(unsigned int indent_lvl, uint2 empty, const TypeCastNode &cast) {
            Local::print_header(indent_lvl, empty, "TypeCast ");
            Local::print_type(cast.type);
            std::cout << std::endl;
            empty.second = indent_lvl + 2;
            print_expression(indent_lvl + 1, empty, cast.expr);
        }

        void print_initilalizer(unsigned int indent_lvl, uint2 empty, const InitializerNode &initializer) {
            Local::print_header(indent_lvl, empty, "Initializer ");
            std::cout << "of " << (initializer.is_data ? "data" : "entity") << " type '" << std::get<std::string>(initializer.type) << "'";
            std::cout << std::endl;
            empty.second = indent_lvl + 1;
            for (auto &expr : initializer.args) {
                print_expression(indent_lvl + 1, empty, expr);
            }
        }

        void print_group_expression(unsigned int indent_lvl, uint2 empty, const GroupExpressionNode &group) {
            Local::print_header(indent_lvl, empty, "Group Expr ");
            std::cout << "group types: ";
            Local::print_type(group.type);
            std::cout << std::endl;

            empty.second = indent_lvl + 1;
            for (auto &expr : group.expressions) {
                print_expression(indent_lvl + 1, empty, expr);
            }
        }

        void print_data_access(unsigned int indent_lvl, uint2 empty, const DataAccessNode &access) {
            Local::print_header(indent_lvl, empty, "Data Access ");
            std::cout << "[" << access.data_type << "]: " << access.var_name << "." << access.field_name << " at ID " << access.field_id
                      << " with type ";
            Local::print_type(access.type);
            std::cout << std::endl;
        }

        void print_grouped_data_access(unsigned int indent_lvl, uint2 empty, const GroupedDataAccessNode &access) {
            Local::print_header(indent_lvl, empty, "Grouped Access ");
            std::cout << access.var_name << ".(";
            for (auto it = access.field_names.begin(); it != access.field_names.end(); ++it) {
                if (it != access.field_names.begin()) {
                    std::cout << ", ";
                }
                std::cout << *it;
            }
            std::cout << ") at IDs (";
            for (auto it = access.field_ids.begin(); it != access.field_ids.end(); ++it) {
                if (it != access.field_ids.begin()) {
                    std::cout << ", ";
                }
                std::cout << *it;
            }
            std::cout << ") of types ";
            Local::print_type(access.type);
            std::cout << std::endl;
        }

        void print_expression(unsigned int indent_lvl, uint2 empty, const std::unique_ptr<ExpressionNode> &expr) {
            if (const auto *variable_node = dynamic_cast<const VariableNode *>(expr.get())) {
                print_variable(indent_lvl, empty, *variable_node);
            } else if (const auto *unary_op_node = dynamic_cast<const UnaryOpExpression *>(expr.get())) {
                print_unary_op(indent_lvl, empty, *unary_op_node);
            } else if (const auto *literal_node = dynamic_cast<const LiteralNode *>(expr.get())) {
                print_literal(indent_lvl, empty, *literal_node);
            } else if (const auto *call_node = dynamic_cast<const CallNodeExpression *>(expr.get())) {
                print_call(indent_lvl, empty, *dynamic_cast<const CallNodeBase *>(call_node));
            } else if (const auto *binary_op_node = dynamic_cast<const BinaryOpNode *>(expr.get())) {
                print_binary_op(indent_lvl, empty, *binary_op_node);
            } else if (const auto *type_cast_node = dynamic_cast<const TypeCastNode *>(expr.get())) {
                print_type_cast(indent_lvl, empty, *type_cast_node);
            } else if (const auto *initializer = dynamic_cast<const InitializerNode *>(expr.get())) {
                print_initilalizer(indent_lvl, empty, *initializer);
            } else if (const auto *group_node = dynamic_cast<const GroupExpressionNode *>(expr.get())) {
                print_group_expression(indent_lvl, empty, *group_node);
            } else if (const auto *data_access = dynamic_cast<const DataAccessNode *>(expr.get())) {
                print_data_access(indent_lvl, empty, *data_access);
            } else if (const auto *grouped_access = dynamic_cast<const GroupedDataAccessNode *>(expr.get())) {
                print_grouped_data_access(indent_lvl, empty, *grouped_access);
            } else {
                THROW_BASIC_ERR(ERR_DEBUG);
                return;
            }
        }

        // --- STATEMENTS ---

        void print_throw(unsigned int indent_lvl, uint2 empty, const ThrowNode &return_node) {
            Local::print_header(indent_lvl, empty, "Throw ");
            std::cout << "throw";
            std::cout << std::endl;

            empty.second = indent_lvl + 2;
            print_expression(++indent_lvl, empty, return_node.throw_value);
        }

        void print_return(unsigned int indent_lvl, uint2 empty, const ReturnNode &return_node) {
            Local::print_header(indent_lvl, empty, "Return ");
            std::cout << "return";
            std::cout << std::endl;

            empty.second = indent_lvl + 2;
            print_expression(++indent_lvl, empty, return_node.return_value);
        }

        void print_if(unsigned int indent_lvl, uint2 empty, const IfNode &if_node) {
            Local::print_header(indent_lvl, empty, "If ");
            std::cout << "if " << std::endl;

            empty.second = indent_lvl + 2;
            print_expression(indent_lvl + 1, empty, if_node.condition);

            empty.second = indent_lvl + 1;
            Local::print_header(indent_lvl, empty, "Then ");
            std::cout << "then [s" << if_node.then_scope->scope_id << "]" << std::endl;
            print_body(indent_lvl + 1, empty, if_node.then_scope->body);

            // empty.second = indent_lvl + 1;
            Local::print_header(indent_lvl, empty, "Else ");

            // empty.second = indent_lvl + 2;
            if (if_node.else_scope.has_value()) {
                if (std::holds_alternative<std::unique_ptr<Scope>>(if_node.else_scope.value())) {
                    std::cout << "" << std::get<std::unique_ptr<Scope>>(if_node.else_scope.value())->scope_id << "]" << std::endl;
                    print_body(indent_lvl + 1, empty, std::get<std::unique_ptr<Scope>>(if_node.else_scope.value())->body);
                } else {
                    std::cout << std::endl;
                    print_if(indent_lvl + 1, empty, *std::get<std::unique_ptr<IfNode>>(if_node.else_scope.value()));
                }
            } else {
                std::cout << std::endl;
            }
        }

        void print_while(unsigned int indent_lvl, uint2 empty, const WhileNode &while_node) {
            Local::print_header(indent_lvl, empty, "While ");
            std::cout << "while" << std::endl;
            empty.second = indent_lvl + 1;
            print_expression(indent_lvl + 1, empty, while_node.condition);

            empty.second = indent_lvl;
            Local::print_header(indent_lvl, empty, "Do ");
            std::cout << "do [s" << while_node.scope->scope_id << "]" << std::endl;
            empty.second = indent_lvl + 1;
            print_body(indent_lvl + 1, empty, while_node.scope->body);
        }

        void print_for(unsigned int indent_lvl, uint2 empty, const ForLoopNode &for_node) {
            Local::print_header(indent_lvl, empty, "For ");
            std::cout << "for [s" << for_node.definition_scope->scope_id << "]" << std::endl;
            empty.second = indent_lvl + 1;
            print_expression(indent_lvl + 1, empty, for_node.condition);

            empty.second = indent_lvl;
            Local::print_header(indent_lvl, empty, "Do ");
            std::cout << "do [s" << for_node.body->scope_id << "]" << std::endl;

            empty.second = indent_lvl + 1;
            print_body(indent_lvl + 1, empty, for_node.body->body);
        }

        void print_catch(unsigned int indent_lvl, uint2 empty, const CatchNode &catch_node) {
            std::optional<CallNodeBase *> call_node = Parser::get_call_from_id(catch_node.call_id);
            if (!call_node.has_value()) {
                return;
            }
            Local::print_header(indent_lvl, empty, "Catch ");
            std::cout << "catch '";
            std::cout << call_node.value()->function_name;
            std::cout << "'";
            if (catch_node.var_name.has_value()) {
                std::cout << " in '";
                std::cout << catch_node.var_name.value();
                std::cout << "'";
            }
            std::cout << " [s" << catch_node.scope->scope_id << "] [c" << catch_node.call_id << "]" << std::endl;

            empty.second = indent_lvl + 1;
            print_body(indent_lvl + 1, empty, catch_node.scope->body);
        }

        void print_group_assignment(unsigned int indent_lvl, uint2 empty, const GroupAssignmentNode &assign) {
            Local::print_header(indent_lvl, empty, "Group Assign ");
            std::cout << "(";
            for (auto it = assign.assignees.begin(); it != assign.assignees.end(); ++it) {
                if (it != assign.assignees.begin()) {
                    std::cout << ", ";
                }
                std::cout << it->second;
            }
            std::cout << ") of types (";
            for (auto it = assign.assignees.begin(); it != assign.assignees.end(); ++it) {
                if (it != assign.assignees.begin()) {
                    std::cout << ", ";
                }
                std::cout << it->first;
            }
            std::cout << ") to be";
            std::cout << std::endl;

            empty.first++;
            empty.second = indent_lvl + 2;
            print_expression(++indent_lvl, empty, assign.expression);
        }

        void print_assignment(unsigned int indent_lvl, uint2 empty, const AssignmentNode &assign) {
            Local::print_header(indent_lvl, empty, "Assign ");
            std::cout << "'" << assign.name << "' to be";
            std::cout << std::endl;

            empty.first++;
            empty.second = indent_lvl + 2;
            print_expression(++indent_lvl, empty, assign.expression);
        }

        void print_group_declaration(unsigned int indent_lvl, uint2 empty, const GroupDeclarationNode &decl) {
            Local::print_header(indent_lvl, empty, "Group Decl ");
            std::cout << "of type (";
            for (auto it = decl.variables.begin(); it != decl.variables.end(); ++it) {
                if (it != decl.variables.begin()) {
                    std::cout << ", ";
                }
                std::cout << it->first;
            }
            std::cout << ") to be" << std::endl;

            empty.first++;
            empty.second = indent_lvl + 2;
            print_expression(++indent_lvl, empty, decl.initializer);
        }

        void print_declaration(unsigned int indent_lvl, uint2 empty, const DeclarationNode &decl) {
            Local::print_header(indent_lvl, empty, "Decl ");
            std::cout << "'" << decl.type << " ";
            std::cout << decl.name << "' to be";

            if (!decl.initializer.has_value()) {
                std::cout << " its default value" << std::endl;
                return;
            }

            std::cout << std::endl;
            empty.first++;
            empty.second = indent_lvl + 2;
            print_expression(++indent_lvl, empty, decl.initializer.value());
        }

        void print_data_field_assignment(unsigned int indent_lvl, uint2 empty, const DataFieldAssignmentNode &assignment) {
            Local::print_header(indent_lvl, empty, "Data Field Assignment ");
            std::cout << "assign [" << assignment.data_type << "] " << assignment.var_name << "." << assignment.field_name << " at IDs "
                      << assignment.field_id << " of types " << assignment.field_type << " to be";
            std::cout << std::endl;
            empty.first++;
            empty.second = indent_lvl + 2;
            print_expression(++indent_lvl, empty, assignment.expression);
        }

        void print_grouped_data_field_assignment(unsigned int indent_lvl, uint2 empty, const GroupedDataFieldAssignmentNode &assignment) {
            Local::print_header(indent_lvl, empty, "Grouped Field Assignment ");
            std::cout << "assign " << assignment.var_name << ".(";
            for (auto it = assignment.field_names.begin(); it != assignment.field_names.end(); ++it) {
                if (it != assignment.field_names.begin()) {
                    std::cout << ", ";
                }
                std::cout << *it;
            }
            std::cout << ") of types ";
            Local::print_type(assignment.field_types);
            std::cout << " at IDs (";
            for (auto it = assignment.field_ids.begin(); it != assignment.field_ids.end(); ++it) {
                if (it != assignment.field_ids.begin()) {
                    std::cout << ", ";
                }
                std::cout << *it;
            }
            std::cout << ") to be" << std::endl;
            empty.first++;
            empty.second = indent_lvl + 2;
            print_expression(++indent_lvl, empty, assignment.expression);
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
            } else if (const auto *group_assignment = dynamic_cast<const GroupAssignmentNode *>(statement.get())) {
                print_group_assignment(indent_lvl, empty, *group_assignment);
            } else if (const auto *assignment = dynamic_cast<const AssignmentNode *>(statement.get())) {
                print_assignment(indent_lvl, empty, *assignment);
            } else if (const auto *group_declaration = dynamic_cast<const GroupDeclarationNode *>(statement.get())) {
                print_group_declaration(indent_lvl, empty, *group_declaration);
            } else if (const auto *declaration = dynamic_cast<const DeclarationNode *>(statement.get())) {
                print_declaration(indent_lvl, empty, *declaration);
            } else if (const auto *throw_node = dynamic_cast<const ThrowNode *>(statement.get())) {
                print_throw(indent_lvl, empty, *throw_node);
            } else if (const auto *catch_node = dynamic_cast<const CatchNode *>(statement.get())) {
                print_catch(indent_lvl, empty, *catch_node);
            } else if (const auto *call_node = dynamic_cast<const CallNodeBase *>(statement.get())) {
                print_call(indent_lvl, empty, *call_node);
            } else if (const auto *unary_op_node = dynamic_cast<const UnaryOpStatement *>(statement.get())) {
                print_unary_op(indent_lvl, empty, *unary_op_node);
            } else if (const auto *data_assignment = dynamic_cast<const DataFieldAssignmentNode *>(statement.get())) {
                print_data_field_assignment(indent_lvl, empty, *data_assignment);
            } else if (const auto *grouped_data_assignment = dynamic_cast<const GroupedDataFieldAssignmentNode *>(statement.get())) {
                print_grouped_data_field_assignment(indent_lvl, empty, *grouped_data_assignment);
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

        // print_data
        //     Prints the content of the generated DataNode
        void print_data(unsigned int indent_lvl, uint2 empty, const DataNode &data) {
            Local::print_header(indent_lvl, empty, "Data");
            if (data.is_aligned) {
                std::cout << "aligned ";
            }
            if (data.is_immutable) {
                std::cout << "immutable ";
            }
            if (data.is_shared) {
                std::cout << "shared ";
            }
            std::cout << data.name << "(";
            for (auto it = data.order.begin(); it != data.order.end(); ++it) {
                if (it != data.order.begin()) {
                    std::cout << ", ";
                }
                std::cout << *it;
            }
            std::cout << "):" << std::endl;

            indent_lvl++;
            empty.first++;
            for (const auto &[field_name, field_type_default] : data.fields) {
                empty.second = indent_lvl + 1;
                Local::print_header(indent_lvl, empty, "Field");
                std::cout << field_type_default.first << " " << field_name << "\n";
                if (field_type_default.second.has_value()) {
                    // Only print default values if they exist
                    empty.second = indent_lvl + 2;
                    Local::print_header(indent_lvl + 1, empty, "Default Value");
                    std::cout << field_type_default.second.value();
                }
            }
        }

        // print_entity
        //     Prints the content of the generated EntityNode
        void print_entity([[maybe_unused]] unsigned int indent_lvl, [[maybe_unused]] const EntityNode &entity) {
            std::cout << "    Entity: " << typeid(entity).name() << "\n";
        }

        // print_enum
        //     Prints the content of the generated EnumNode
        void print_enum([[maybe_unused]] unsigned int indent_lvl, [[maybe_unused]] const EnumNode &enum_node) {
            std::cout << "    Enum: " << typeid(enum_node).name() << "\n";
        }

        // print_error
        //     Prints the content of the generated ErrorNode
        void print_error([[maybe_unused]] unsigned int indent_lvl, [[maybe_unused]] const ErrorNode &error) {
            std::cout << "    Error: " << typeid(error).name() << "\n";
        }

        // print_func
        //     Prints the content of the generated FuncNode
        void print_func([[maybe_unused]] unsigned int indent_lvl, [[maybe_unused]] const FuncNode &func) {
            std::cout << "    Func: " << typeid(func).name() << "\n";
        }

        /// print_function
        ///     Prints the content of the generated FunctionNode
        void print_function(unsigned int indent_lvl, uint2 empty, const FunctionNode &function) {
            Local::print_header(indent_lvl, empty, "Function ");

            if (function.is_aligned) {
                std::cout << "aligned ";
            }
            if (function.is_const) {
                std::cout << "const ";
            }
            std::cout << function.name << "(";
            size_t counter = 0;
            for (const std::tuple<std::string, std::string, bool> &param : function.parameters) {
                std::cout << (std::get<2>(param) ? "mut" : "const") << " ";                    // Whether the param is const or mut
                std::cout << (keywords.find(std::get<0>(param)) == keywords.end() ? "&" : ""); // If its a primitive or complex type
                std::cout << std::get<0>(param) << " ";                                        // The actual type
                std::cout << std::get<1>(param);                                               // The parameter name
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
            std::cout << " [s" << function.scope->scope_id << "]" << std::endl;

            // The function body
            empty.first++;
            empty.second = ++indent_lvl;
            print_body(indent_lvl, empty, function.scope->body);
        }

        /// print_import
        ///     Prints the content of the generated ImportNode
        void print_import(unsigned int indent_lvl, const ImportNode &import) {
            Local::print_header(indent_lvl, {0, 0}, "Import ");

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
        void print_link([[maybe_unused]] unsigned int indent_lvl, [[maybe_unused]] uint2 empty, [[maybe_unused]] const LinkNode &link) {
            std::cout << "    Link: " << typeid(link).name() << "\n";
        }

        /// print_link
        ///     Prints the content of the generated VariantNode
        void print_variant([[maybe_unused]] unsigned int indent_lvl, [[maybe_unused]] const VariantNode &variant) {
            std::cout << "    Variant: " << typeid(variant).name() << "\n";
        }

        void print_test(unsigned int indent_lvl, uint2 empty, const TestNode &test) {
            Local::print_header(indent_lvl, empty, "Test ");
            std::cout << test.file_name << " : \"" << test.name << "\" [s" << test.scope->scope_id << "]" << std::endl;

            // The test body
            empty.first++;
            empty.second = ++indent_lvl;
            print_body(indent_lvl, empty, test.scope->body);
        }
    } // namespace AST
} // namespace Debug
