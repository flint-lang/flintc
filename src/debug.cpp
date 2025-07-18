#include "debug.hpp"

#include "globals.hpp"
#include "lexer/lexer_utils.hpp"
#include "parser/ast/expressions/call_node_expression.hpp"
#include "parser/ast/expressions/unary_op_expression.hpp"
#include "parser/ast/scope.hpp"
#include "parser/ast/statements/break_node.hpp"
#include "parser/ast/statements/continue_node.hpp"
#include "parser/ast/statements/unary_op_statement.hpp"
#include "persistent_thread_pool.hpp"

bool PRINT_TOK_STREAM = true;
bool PRINT_DEP_TREE = true;
bool PRINT_AST = true;
bool PRINT_IR_PROGRAM = true;
bool PRINT_PROFILE_RESULTS = true;
bool HARD_CRASH = false;
bool NO_GENERATION = false;

std::string RED = "\033[31m";
std::string GREEN = "\033[32m";
std::string YELLOW = "\033[33m";
std::string BLUE = "\033[34m";
std::string WHITE = "\033[37m";
std::string DEFAULT = "\033[0m";

unsigned int BUILTIN_LIBS_TO_PRINT = 0;
Target COMPILATION_TARGET = Target::NATIVE;
ArithmeticOverflowMode overflow_mode = ArithmeticOverflowMode::PRINT;
ArrayOutOfBoundsMode oob_mode = ArrayOutOfBoundsMode::PRINT;
OptionalUnwrapMode unwrap_mode = OptionalUnwrapMode::CRASH;

PersistentThreadPool thread_pool;

#include <cassert>
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
    void print_token_context_vector(const token_slice &tokens, const std::string &file_name) {
        if (!DEBUG_MODE) {
            return;
        }
        std::cout << YELLOW << "[Debug Info] Printing token vector of file '" << file_name << "'" << DEFAULT << std::endl;
        std::stringstream type_stream;
        for (auto tc = tokens.first; tc != tokens.second; ++tc) {
            type_stream << " | Type: '" << get_token_name(tc->token) << "' => " << std::to_string(static_cast<int>(tc->token));
            std::cout << "Line: " << tc->line << " | Column: " << tc->column << get_string_container(30, type_stream.str()) << " | Lexme: ";
            if (tc->token == TOK_TYPE) {
                std::cout << tc->type->to_string();
            } else {
                std::cout << tc->lexme;
            }
            std::cout << "\n";
            type_stream.str(std::string());
            type_stream.clear();
        }
    }

    /// print_tree_row
    ///
    void print_tree_row(const std::vector<TreeType> &types, TestResult *result) {
        std::string addition;
        for (const TreeType &type : types) {
            addition = tree_blocks.at(type);
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
            std::cout << tree_blocks.at(type);
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
                    std::cout << tree_blocks.at(VERT);
                }
                if ((dep + 1) != dep_node_sh->dependencies.end()) {
                    std::cout << tree_blocks.at(BRANCH);
                } else {
                    std::cout << tree_blocks.at(SINGLE);
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
            void print_tree_line(TreeBits &bits, unsigned int max_level) {
                for (unsigned int level = 0; level <= max_level; level++) {
                    std::cout << tree_blocks.at(bits.get(level));
                }
            }

            /// print_header
            ///     Prints the header of the AST node (the left part incl. the header name)
            void print_header(unsigned int indent_lvl, TreeBits &bits, const std::string &header) {
                print_tree_line(bits, indent_lvl);
                // Print the header text with formatting
                std::cout << TextFormat::BOLD_START << header << TextFormat::BOLD_END;

                // Add the horizontal line to align all comments
                if (header.size() + (4 * indent_lvl) > C_SIZE) {
                    std::cout << HOR;
                } else {
                    std::cout << create_n_str(C_SIZE - header.size() - (4 * indent_lvl), HOR);
                }
                std::cout << "> ";
                switch (bits.get(indent_lvl)) {
                    case NONE:
                        [[fallthrough]];
                    case SINGLE:
                        bits.set(NONE, indent_lvl);
                        break;
                    case VERT:
                        [[fallthrough]];
                    case BRANCH:
                        bits.set(VERT, indent_lvl);
                        break;
                }
            }
        } // namespace Local

        void print_all_files() {
            std::lock_guard<std::mutex> lock(Resolver::file_map_mutex);
            for (const auto &file_pair : Resolver::file_map) {
                print_file(*file_pair.second);
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

            for (const std::unique_ptr<ASTNode> &node : file.definitions) {
                // Create fresh TreeBits for each node
                TreeBits bits;

                // Set the first bit whether this is the
                if (++counter == file.definitions.size()) {
                    bits.set(SINGLE, 0);
                } else {
                    bits.set(BRANCH, 0);
                }

                if (const auto *data_node = dynamic_cast<const DataNode *>(node.get())) {
                    print_data(0, bits, *data_node);
                } else if (const auto *entity_node = dynamic_cast<const EntityNode *>(node.get())) {
                    print_entity(0, bits, *entity_node);
                } else if (const auto *enum_node = dynamic_cast<const EnumNode *>(node.get())) {
                    print_enum(0, bits, *enum_node);
                } else if (const auto *func_node = dynamic_cast<const FuncNode *>(node.get())) {
                    print_func(0, bits, *func_node);
                } else if (const auto *function_node = dynamic_cast<const FunctionNode *>(node.get())) {
                    print_function(0, bits, *function_node);
                } else if (const auto *import_node = dynamic_cast<const ImportNode *>(node.get())) {
                    print_import(0, bits, *import_node);
                } else if (const auto *link_node = dynamic_cast<const LinkNode *>(node.get())) {
                    print_link(0, bits, *link_node);
                } else if (const auto *variant_node = dynamic_cast<const VariantNode *>(node.get())) {
                    print_variant(0, bits, *variant_node);
                } else if (const auto *test_node = dynamic_cast<const TestNode *>(node.get())) {
                    print_test(0, bits, *test_node);
                } else {
                    assert(false);
                }
            }
            std::cout << std::endl;
        }

        // --- EXPRESSIONS ---

        void print_variable(unsigned int indent_lvl, TreeBits &bits, const VariableNode &var) {
            Local::print_header(indent_lvl, bits, "Variable ");
            std::cout << var.name;
            std::cout << std::endl;
        }

        void print_unary_op(unsigned int indent_lvl, TreeBits &bits, const UnaryOpBase &unary) {
            Local::print_header(indent_lvl, bits, "UnOp ");
            std::cout << "operation: " << get_token_name(unary.operator_token);
            std::cout << std::endl;

            // Unary ops only have one child, so that child is a single
            TreeBits child_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, child_bits, unary.operand);
        }

        void print_literal(unsigned int indent_lvl, TreeBits &bits, const LiteralNode &lit) {
            Local::print_header(indent_lvl, bits, "Lit ");
            std::cout << lit.type->to_string();
            std::cout << ": ";
            if (std::holds_alternative<unsigned long>(lit.value)) {
                std::cout << std::get<unsigned long>(lit.value);
            } else if (std::holds_alternative<long>(lit.value)) {
                std::cout << std::get<long>(lit.value);
            } else if (std::holds_alternative<unsigned int>(lit.value)) {
                std::cout << std::get<unsigned int>(lit.value);
            } else if (std::holds_alternative<int>(lit.value)) {
                std::cout << std::get<int>(lit.value);
            } else if (std::holds_alternative<double>(lit.value)) {
                std::cout << std::get<double>(lit.value);
            } else if (std::holds_alternative<float>(lit.value)) {
                std::cout << std::get<float>(lit.value);
            } else if (std::holds_alternative<bool>(lit.value)) {
                std::cout << (std::get<bool>(lit.value) ? "true" : "false");
            } else if (std::holds_alternative<char>(lit.value)) {
                std::cout << "'" << std::string(1, std::get<char>(lit.value)) << "'";
            } else if (std::holds_alternative<std::string>(lit.value)) {
                const std::string &lit_val = std::get<std::string>(lit.value);
                std::cout << "\"";
                for (size_t i = 0; i < lit_val.length(); i++) {
                    switch (lit_val[i]) {
                        case '\n':
                            std::cout << "\\n";
                            break;
                        case '\t':
                            std::cout << "\\t";
                            break;
                        case '\r':
                            std::cout << "\\r";
                            break;
                        case '\\':
                            std::cout << "\\";
                            break;
                        case '\0':
                            std::cout << "\\0";
                            break;
                        default:
                            std::cout << lit_val[i];
                    }
                }
                std::cout << "\"";
            } else if (std::holds_alternative<std::optional<void *>>(lit.value)) {
                std::cout << "none";
            }
            if (lit.is_folded) {
                std::cout << " [folded]";
            }
            std::cout << std::endl;
        }

        void print_string_interpolation(unsigned int indent_lvl, TreeBits &bits, const StringInterpolationNode &interpol) {
            Local::print_header(indent_lvl, bits, "Interpol ");
            std::cout << std::endl;

            indent_lvl++;
            for (auto var = interpol.string_content.begin(); var != interpol.string_content.end(); ++var) {
                bool is_last = std::next(var) == interpol.string_content.end();
                TreeBits child_bits = bits.child(indent_lvl, is_last);

                if (std::holds_alternative<std::unique_ptr<LiteralNode>>(*var)) {
                    print_literal(indent_lvl, child_bits, *std::get<std::unique_ptr<LiteralNode>>(*var));
                } else {
                    print_expression(indent_lvl, child_bits, std::get<std::unique_ptr<ExpressionNode>>(*var));
                }
            }
        }

        void print_call(unsigned int indent_lvl, TreeBits &bits, const CallNodeBase &call) {
            Local::print_header(indent_lvl, bits, "Call ");
            std::cout << "[" << (call.can_throw ? "throws" : "nothrow") << "] ";
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

            indent_lvl++;
            for (auto arg = call.arguments.begin(); arg != call.arguments.end(); ++arg) {
                bool is_last = std::next(arg) == call.arguments.end();
                TreeBits child_bits = bits.child(indent_lvl, is_last);
                print_expression(indent_lvl, child_bits, arg->first);
            }
        }

        void print_binary_op(unsigned int indent_lvl, TreeBits &bits, const BinaryOpNode &bin) {
            Local::print_header(indent_lvl, bits, "BinOp ");
            std::cout << get_token_name(bin.operator_token);
            if (bin.is_shorthand) {
                std::cout << " [append]";
            }
            std::cout << std::endl;

            // LHS part
            indent_lvl++;
            TreeBits lhs_bits = bits.child(indent_lvl, false);
            Local::print_header(indent_lvl, lhs_bits, "LHS ");
            std::cout << bin.left->type->to_string();
            std::cout << std::endl;

            TreeBits lhs_expr_bits = bits.child(indent_lvl + 1, true);
            lhs_expr_bits.set(VERT, indent_lvl);
            print_expression(indent_lvl + 1, lhs_expr_bits, bin.left);

            // RHS part
            TreeBits rhs_bits = bits.child(indent_lvl, true);
            Local::print_header(indent_lvl, rhs_bits, "RHS ");
            std::cout << bin.right->type->to_string();
            std::cout << std::endl;

            TreeBits rhs_expr_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, rhs_expr_bits, bin.right);
        }

        void print_type_cast(unsigned int indent_lvl, TreeBits &bits, const TypeCastNode &cast) {
            Local::print_header(indent_lvl, bits, "TypeCast ");
            std::cout << cast.expr->type->to_string();
            std::cout << " -> ";
            std::cout << cast.type->to_string();
            std::cout << std::endl;

            indent_lvl++;
            TreeBits expr_bits = bits.child(indent_lvl, true);
            print_expression(indent_lvl, expr_bits, cast.expr);
        }

        void print_initializer(unsigned int indent_lvl, TreeBits &bits, const InitializerNode &initializer) {
            Local::print_header(indent_lvl, bits, "Initializer ");
            std::cout << "of " << (initializer.is_data ? "data" : "entity") << " type '";
            std::cout << initializer.type->to_string();
            std::cout << "'" << std::endl;

            indent_lvl++;
            for (auto expr = initializer.args.begin(); expr != initializer.args.end(); ++expr) {
                bool is_last = std::next(expr) == initializer.args.end();
                TreeBits child_bits = bits.child(indent_lvl, is_last);
                print_expression(indent_lvl, child_bits, *expr);
            }
        }

        void print_group_expression(unsigned int indent_lvl, TreeBits &bits, const GroupExpressionNode &group) {
            Local::print_header(indent_lvl, bits, "Group Expr ");
            std::cout << "group types: ";
            std::cout << group.type->to_string();
            std::cout << std::endl;

            unsigned int counter = 0;
            indent_lvl++;
            for (auto &expr : group.expressions) {
                bool is_last = ++counter == group.expressions.size();
                TreeBits expr_bits = bits.child(indent_lvl, is_last);
                print_expression(indent_lvl, expr_bits, expr);
            }
        }

        void print_array_initializer(unsigned int indent_lvl, TreeBits &bits, const ArrayInitializerNode &init) {
            Local::print_header(indent_lvl, bits, "Array Initializer ");
            std::cout << init.type->to_string();
            std::cout << std::endl;

            // Print length expressions
            indent_lvl++;
            for (size_t i = 0; i < init.length_expressions.size(); i++) {
                TreeBits init_bits = bits.child(indent_lvl, false);
                Local::print_header(indent_lvl, init_bits, "Len Init " + std::to_string(i) + " ");
                std::cout << init.length_expressions[i]->type->to_string();
                std::cout << std::endl;

                TreeBits expr_bits = init_bits.child(indent_lvl + 1, true);
                print_expression(indent_lvl + 1, expr_bits, init.length_expressions[i]);
            }

            // Print initializer value
            TreeBits init_bits = bits.child(indent_lvl, true);
            Local::print_header(indent_lvl, init_bits, "Initializer ");
            std::cout << init.initializer_value->type->to_string();
            std::cout << std::endl;

            TreeBits value_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, value_bits, init.initializer_value);
        }

        void print_array_access(unsigned int indent_lvl, TreeBits &bits, const ArrayAccessNode &access) {
            Local::print_header(indent_lvl, bits, "Array Access ");
            std::cout << access.variable_type->to_string() << " " << access.variable_name << " -> ";
            std::cout << access.type->to_string();
            std::cout << std::endl;

            indent_lvl++;
            for (size_t i = 0; i < access.indexing_expressions.size(); i++) {
                bool is_last = i + 1 == access.indexing_expressions.size();
                TreeBits header_bits = bits.child(indent_lvl, is_last);
                Local::print_header(indent_lvl, header_bits, "ID " + std::to_string(i) + " ");
                std::cout << access.indexing_expressions[i]->type->to_string();
                std::cout << std::endl;

                TreeBits expr_bits = header_bits.child(indent_lvl + 1, true);
                print_expression(indent_lvl + 1, expr_bits, access.indexing_expressions[i]);
            }
        }

        void print_data_access(unsigned int indent_lvl, TreeBits &bits, const DataAccessNode &access) {
            Local::print_header(indent_lvl, bits, "Data Access ");
            std::cout << "[" << access.data_type->to_string() << "]: ";
            if (std::holds_alternative<std::string>(access.variable)) {
                std::cout << std::get<std::string>(access.variable) << ".";
                if (access.field_name.has_value()) {
                    std::cout << access.field_name.value() << " at ID " << access.field_id;
                } else {
                    std::cout << "$" << access.field_id;
                }
                std::cout << " with type " << access.type->to_string() << std::endl;
            } else {
                std::cout << "stacked access of field ";
                if (access.field_name.has_value()) {
                    std::cout << access.field_name.value() << " at ID " << access.field_id;
                } else {
                    std::cout << "$" << access.field_id;
                }
                std::cout << " with type " << access.type->to_string() << " of stack:" << std::endl;

                TreeBits stack_bits = bits.child(indent_lvl + 1, true);
                print_expression(indent_lvl + 1, stack_bits, std::get<std::unique_ptr<ExpressionNode>>(access.variable));
            }
        }

        void print_grouped_data_access(unsigned int indent_lvl, TreeBits &bits, const GroupedDataAccessNode &access) {
            Local::print_header(indent_lvl, bits, "Grouped Access ");
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
            std::cout << access.type->to_string();
            std::cout << std::endl;
        }

        void print_switch_expression(unsigned int indent_lvl, TreeBits &bits, const SwitchExpression &switch_expression) {
            Local::print_header(indent_lvl, bits, "Switch ");
            std::cout << "switch\n";
            TreeBits expr_header_bits = bits.child(indent_lvl + 1, false);
            Local::print_header(indent_lvl + 1, expr_header_bits, "Expression ");
            std::cout << "\n";
            TreeBits expr_bits = expr_header_bits.child(indent_lvl + 2, true);
            print_expression(indent_lvl + 2, expr_bits, switch_expression.switcher);
            for (auto branch = switch_expression.branches.begin(); branch != switch_expression.branches.end(); ++branch) {
                const bool is_last = std::next(branch) == switch_expression.branches.end();
                TreeBits case_bits = bits.child(indent_lvl + 1, is_last);
                Local::print_header(indent_lvl + 1, case_bits, "Case ");
                std::cout << "\n";
                for (auto it = branch->matches.begin(); it != branch->matches.end(); ++it) {
                    TreeBits case_match_header_bits = case_bits.child(indent_lvl + 2, false);
                    Local::print_header(indent_lvl + 2, case_match_header_bits, "Case Match ");
                    std::cout << "[s" << branch->scope->scope_id << "]\n";
                    TreeBits case_match_bits = case_match_header_bits.child(indent_lvl + 3, std::next(it) == branch->matches.end());
                    print_expression(indent_lvl + 3, case_match_bits, *it);
                }
                TreeBits case_expr_header_bits = case_bits.child(indent_lvl + 2, true);
                Local::print_header(indent_lvl + 2, case_expr_header_bits, "Case Expr ");
                std::cout << "\n";
                TreeBits case_expr_bits = case_expr_header_bits.child(indent_lvl + 3, true);
                print_expression(indent_lvl + 3, case_expr_bits, branch->expr);
            }
        }

        void print_default(unsigned int indent_lvl, TreeBits &bits, const DefaultNode &default_node) {
            Local::print_header(indent_lvl, bits, "Default ");
            std::cout << "of type " << default_node.type->to_string() << std::endl;
        }

        void print_expression(unsigned int indent_lvl, TreeBits &bits, const std::unique_ptr<ExpressionNode> &expr) {
            if (const auto *variable_node = dynamic_cast<const VariableNode *>(expr.get())) {
                print_variable(indent_lvl, bits, *variable_node);
            } else if (const auto *unary_op_node = dynamic_cast<const UnaryOpExpression *>(expr.get())) {
                print_unary_op(indent_lvl, bits, *unary_op_node);
            } else if (const auto *literal_node = dynamic_cast<const LiteralNode *>(expr.get())) {
                print_literal(indent_lvl, bits, *literal_node);
            } else if (const auto *call_node = dynamic_cast<const CallNodeExpression *>(expr.get())) {
                print_call(indent_lvl, bits, *dynamic_cast<const CallNodeBase *>(call_node));
            } else if (const auto *binary_op_node = dynamic_cast<const BinaryOpNode *>(expr.get())) {
                print_binary_op(indent_lvl, bits, *binary_op_node);
            } else if (const auto *type_cast_node = dynamic_cast<const TypeCastNode *>(expr.get())) {
                print_type_cast(indent_lvl, bits, *type_cast_node);
            } else if (const auto *initializer = dynamic_cast<const InitializerNode *>(expr.get())) {
                print_initializer(indent_lvl, bits, *initializer);
            } else if (const auto *group_node = dynamic_cast<const GroupExpressionNode *>(expr.get())) {
                print_group_expression(indent_lvl, bits, *group_node);
            } else if (const auto *data_access = dynamic_cast<const DataAccessNode *>(expr.get())) {
                print_data_access(indent_lvl, bits, *data_access);
            } else if (const auto *grouped_access = dynamic_cast<const GroupedDataAccessNode *>(expr.get())) {
                print_grouped_data_access(indent_lvl, bits, *grouped_access);
            } else if (const auto *interpol = dynamic_cast<const StringInterpolationNode *>(expr.get())) {
                print_string_interpolation(indent_lvl, bits, *interpol);
            } else if (const auto *array_init = dynamic_cast<const ArrayInitializerNode *>(expr.get())) {
                print_array_initializer(indent_lvl, bits, *array_init);
            } else if (const auto *array_access = dynamic_cast<const ArrayAccessNode *>(expr.get())) {
                print_array_access(indent_lvl, bits, *array_access);
            } else if (const auto *switch_expression = dynamic_cast<const SwitchExpression *>(expr.get())) {
                print_switch_expression(indent_lvl, bits, *switch_expression);
            } else if (const auto *default_node = dynamic_cast<const DefaultNode *>(expr.get())) {
                print_default(indent_lvl, bits, *default_node);
            } else {
                assert(false);
            }
        }

        // --- STATEMENTS ---

        void print_throw(unsigned int indent_lvl, TreeBits &bits, const ThrowNode &return_node) {
            Local::print_header(indent_lvl, bits, "Throw ");
            std::cout << "throw";
            std::cout << std::endl;

            TreeBits expr_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, expr_bits, return_node.throw_value);
        }

        void print_return(unsigned int indent_lvl, TreeBits &bits, const ReturnNode &return_node) {
            Local::print_header(indent_lvl, bits, "Return ");
            std::cout << "return";
            std::cout << std::endl;

            if (return_node.return_value.has_value()) {
                TreeBits expr_bits = bits.child(indent_lvl + 1, true);
                print_expression(indent_lvl + 1, expr_bits, return_node.return_value.value());
            }
        }

        void print_if(unsigned int indent_lvl, TreeBits &bits, const IfNode &if_node) {
            Local::print_header(indent_lvl, bits, "If ");
            std::cout << "if " << std::endl;

            // Print condition with appropriate tree state
            indent_lvl++;
            TreeBits condition_bits = bits.child(indent_lvl, false);
            print_expression(indent_lvl, condition_bits, if_node.condition);

            // Print "Then" branch
            TreeBits then_bits = bits.child(indent_lvl, !if_node.else_scope.has_value());
            Local::print_header(indent_lvl, then_bits, "Then ");
            std::cout << "then [s" << if_node.then_scope->scope_id << "]" << std::endl;

            print_body(indent_lvl + 1, then_bits, if_node.then_scope->body);

            // Print "Else" branch
            if (if_node.else_scope.has_value()) {
                TreeBits else_bits = bits.child(indent_lvl, true);
                Local::print_header(indent_lvl, else_bits, "Else ");

                if (std::holds_alternative<std::shared_ptr<Scope>>(if_node.else_scope.value())) {
                    std::cout << "else [s" << std::get<std::shared_ptr<Scope>>(if_node.else_scope.value())->scope_id << "]" << std::endl;

                    TreeBits else_body_bits = bits.child(indent_lvl + 1, true);
                    print_body(indent_lvl + 1, else_body_bits, std::get<std::shared_ptr<Scope>>(if_node.else_scope.value())->body);
                } else {
                    std::cout << std::endl;
                    TreeBits else_if_bits = bits.child(indent_lvl + 1, true);
                    print_if(indent_lvl + 1, else_if_bits, *std::get<std::unique_ptr<IfNode>>(if_node.else_scope.value()));
                }
            }
        }

        void print_while(unsigned int indent_lvl, TreeBits &bits, const WhileNode &while_node) {
            Local::print_header(indent_lvl, bits, "While ");
            std::cout << "while" << std::endl;

            TreeBits cond_bits = bits.child(indent_lvl + 1, false);
            print_expression(indent_lvl + 1, cond_bits, while_node.condition);

            TreeBits do_bits = bits.child(indent_lvl, true);
            Local::print_header(indent_lvl, do_bits, "Do ");
            std::cout << "do [s" << while_node.scope->scope_id << "]" << std::endl;

            TreeBits body_bits = bits.child(indent_lvl + 1, true);
            print_body(indent_lvl + 1, body_bits, while_node.scope->body);
        }

        void print_for(unsigned int indent_lvl, TreeBits &bits, const ForLoopNode &for_node) {
            Local::print_header(indent_lvl, bits, "For ");
            std::cout << "for [s" << for_node.definition_scope->scope_id << "]" << std::endl;

            TreeBits cond_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, cond_bits, for_node.condition);

            TreeBits do_bits = bits.child(indent_lvl, true);
            Local::print_header(indent_lvl, do_bits, "Do ");
            std::cout << "do [s" << for_node.body->scope_id << "]" << std::endl;

            TreeBits body_bits = bits.child(indent_lvl + 1, true);
            print_body(indent_lvl + 1, body_bits, for_node.body->body);
        }

        void print_enh_for(unsigned int indent_lvl, TreeBits &bits, const EnhForLoopNode &for_node) {
            Local::print_header(indent_lvl, bits, "Enh For ");
            std::cout << "[s" << for_node.definition_scope->scope_id << "] for ";
            if (std::holds_alternative<std::string>(for_node.iterators)) {
                std::cout << std::get<std::string>(for_node.iterators);
            } else {
                auto iterators = std::get<std::pair<std::optional<std::string>, std::optional<std::string>>>(for_node.iterators);
                std::cout << "(";
                if (iterators.first.has_value()) {
                    const auto idx_type = for_node.definition_scope->get_variable_type(iterators.first.value()).value();
                    std::cout << iterators.first.value() << " [" << idx_type->to_string() << "]";
                } else {
                    std::cout << "_";
                }
                std::cout << ", ";
                if (iterators.second.has_value()) {
                    const auto elem_type = for_node.definition_scope->get_variable_type(iterators.second.value()).value();
                    std::cout << iterators.second.value() << " [" << elem_type->to_string() << "]";
                } else {
                    std::cout << "_";
                }
                std::cout << ")";
            }
            std::cout << " in ";
            std::cout << std::endl;

            TreeBits expr_bits = bits.child(indent_lvl + 1, false);
            print_expression(indent_lvl + 1, expr_bits, for_node.iterable);

            TreeBits do_bits = bits.child(indent_lvl, true);
            Local::print_header(indent_lvl, do_bits, "Do ");
            std::cout << "[s" << for_node.body->scope_id << "] do" << std::endl;

            TreeBits body_bits = bits.child(indent_lvl + 1, true);
            print_body(indent_lvl + 1, body_bits, for_node.body->body);
        }

        void print_switch_statement(unsigned int indent_lvl, TreeBits &bits, const SwitchStatement &switch_statement) {
            Local::print_header(indent_lvl, bits, "Switch ");
            std::cout << "switch\n";
            TreeBits expr_header_bits = bits.child(indent_lvl + 1, false);
            Local::print_header(indent_lvl + 1, expr_header_bits, "Expression ");
            std::cout << "\n";
            TreeBits expr_bits = expr_header_bits.child(indent_lvl + 2, true);
            print_expression(indent_lvl + 2, expr_bits, switch_statement.switcher);
            for (auto branch = switch_statement.branches.begin(); branch != switch_statement.branches.end(); ++branch) {
                const bool is_last = std::next(branch) == switch_statement.branches.end();
                TreeBits case_bits = bits.child(indent_lvl + 1, is_last);
                Local::print_header(indent_lvl + 1, case_bits, "Case ");
                std::cout << "\n";
                for (auto it = branch->matches.begin(); it != branch->matches.end(); ++it) {
                    TreeBits case_expr_header_bits = case_bits.child(indent_lvl + 2, false);
                    Local::print_header(indent_lvl + 2, case_expr_header_bits, "Case Match ");
                    std::cout << "\n";
                    TreeBits case_expr_bits = case_expr_header_bits.child(indent_lvl + 3, std::next(it) == branch->matches.end());
                    print_expression(indent_lvl + 3, case_expr_bits, *it);
                }
                TreeBits case_body_header_bits = case_bits.child(indent_lvl + 2, true);
                Local::print_header(indent_lvl + 2, case_body_header_bits, "Case Body ");
                std::cout << "[s" << branch->body->scope_id << "]\n";
                TreeBits case_body_bits = case_body_header_bits.child(indent_lvl + 3, true);
                print_body(indent_lvl + 3, case_body_bits, branch->body->body);
            }
        }

        void print_catch(unsigned int indent_lvl, TreeBits &bits, const CatchNode &catch_node) {
            Local::print_header(indent_lvl, bits, "Catch ");
            std::cout << "catch '";
            std::cout << catch_node.call_node->function_name;
            std::cout << "'";
            if (catch_node.var_name.has_value()) {
                std::cout << " in '";
                std::cout << catch_node.var_name.value();
                std::cout << "'";
            }
            std::cout << " [s" << catch_node.scope->scope_id << "] [c" << catch_node.call_node->call_id << "]" << std::endl;

            print_body(indent_lvl + 1, bits, catch_node.scope->body);
        }

        void print_group_assignment(unsigned int indent_lvl, TreeBits &bits, const GroupAssignmentNode &assign) {
            Local::print_header(indent_lvl, bits, "Group Assign ");
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
                std::cout << it->first->to_string();
            }
            std::cout << ") to be";
            std::cout << std::endl;

            TreeBits expr_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, expr_bits, assign.expression);
        }

        void print_assignment(unsigned int indent_lvl, TreeBits &bits, const AssignmentNode &assign) {
            Local::print_header(indent_lvl, bits, "Assign ");
            std::cout << "'" << assign.type->to_string() << " " << assign.name << "'";
            if (assign.is_shorthand) {
                std::cout << " [shorthand]";
            }
            std::cout << " to be";
            std::cout << std::endl;

            TreeBits expr_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, expr_bits, assign.expression);
        }

        void print_array_assignment(unsigned int indent_lvl, TreeBits &bits, const ArrayAssignmentNode &assign) {
            Local::print_header(indent_lvl, bits, "Array Assign ");
            std::cout << assign.array_type->to_string() << " " << assign.variable_name;
            std::cout << std::endl;

            indent_lvl++;
            for (size_t i = 0; i < assign.indexing_expressions.size(); i++) {
                TreeBits id_bits = bits.child(indent_lvl, false);
                Local::print_header(indent_lvl, id_bits, "ID " + std::to_string(i) + " ");
                std::cout << assign.indexing_expressions[i]->type->to_string();
                std::cout << std::endl;

                TreeBits expr_bits = id_bits.child(indent_lvl + 1, true);
                print_expression(indent_lvl + 1, expr_bits, assign.indexing_expressions[i]);
            }

            TreeBits assignee_bits = bits.child(indent_lvl, true);
            Local::print_header(indent_lvl, assignee_bits, "Assignee ");
            std::cout << "of type " << assign.expression->type->to_string();
            std::cout << std::endl;

            TreeBits assign_expr_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, assign_expr_bits, assign.expression);
        }

        void print_group_declaration(unsigned int indent_lvl, TreeBits &bits, const GroupDeclarationNode &decl) {
            Local::print_header(indent_lvl, bits, "Group Decl ");
            std::cout << "(";
            for (size_t i = 0; i < decl.variables.size(); i++) {
                if (i > 0) {
                    std::cout << ", ";
                }
                std::cout << decl.variables[i].second;
            }
            std::cout << ") of types (";
            for (auto it = decl.variables.begin(); it != decl.variables.end(); ++it) {
                if (it != decl.variables.begin()) {
                    std::cout << ", ";
                }
                std::cout << it->first->to_string();
            }
            std::cout << ") to be" << std::endl;

            TreeBits expr_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, expr_bits, decl.initializer);
        }

        void print_declaration(unsigned int indent_lvl, TreeBits &bits, const DeclarationNode &decl) {
            Local::print_header(indent_lvl, bits, "Decl ");
            std::cout << "'" << decl.type->to_string() << " ";
            std::cout << decl.name << "' to be";

            if (!decl.initializer.has_value()) {
                std::cout << " its default value" << std::endl;
                return;
            }
            std::cout << std::endl;

            TreeBits expr_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, expr_bits, decl.initializer.value());
        }

        void print_data_field_assignment(unsigned int indent_lvl, TreeBits &bits, const DataFieldAssignmentNode &assignment) {
            Local::print_header(indent_lvl, bits, "Data Field Assignment ");
            std::cout << "assign [" << assignment.data_type->to_string() << "] " << assignment.var_name << ".";
            if (assignment.field_name.has_value()) {
                std::cout << assignment.field_name.value() << " at ID " << assignment.field_id;
            } else {
                std::cout << "$" << assignment.field_id;
            }
            std::cout << " of types " << assignment.field_type->to_string() << " to be";
            std::cout << std::endl;

            TreeBits expr_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, expr_bits, assignment.expression);
        }

        void print_grouped_data_field_assignment(                                                     //
            unsigned int indent_lvl, TreeBits &bits, const GroupedDataFieldAssignmentNode &assignment //
        ) {
            Local::print_header(indent_lvl, bits, "Grouped Field Assignment ");
            std::cout << "assign " << assignment.var_name << ".(";
            for (auto it = assignment.field_names.begin(); it != assignment.field_names.end(); ++it) {
                if (it != assignment.field_names.begin()) {
                    std::cout << ", ";
                }
                std::cout << *it;
            }
            std::cout << ") of types ";
            if (assignment.field_types.size() > 1) {
                std::cout << "(";
                for (size_t i = 0; i < assignment.field_types.size(); i++) {
                    if (i > 0) {
                        std::cout << ", ";
                    }
                    std::cout << assignment.field_types[i]->to_string();
                }
                std::cout << ")";
            } else {
                std::cout << assignment.field_types.front()->to_string();
            }
            std::cout << " at IDs (";
            for (auto it = assignment.field_ids.begin(); it != assignment.field_ids.end(); ++it) {
                if (it != assignment.field_ids.begin()) {
                    std::cout << ", ";
                }
                std::cout << *it;
            }
            std::cout << ") to be" << std::endl;

            TreeBits expr_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, expr_bits, assignment.expression);
        }

        void print_stacked_assignment(unsigned int indent_lvl, TreeBits &bits, const StackedAssignmentNode &assignment) {
            Local::print_header(indent_lvl, bits, "Stacked Assignment ");
            std::cout << "Field " << std::to_string(assignment.field_id) << " [" << assignment.field_name << " : "
                      << assignment.field_type->to_string() << "] from";
            std::cout << std::endl;

            // Print base expression
            indent_lvl++;
            TreeBits base_bits = bits.child(indent_lvl, false);
            Local::print_header(indent_lvl, base_bits, "Base Expression ");
            std::cout << assignment.base_expression->type->to_string() << std::endl;

            TreeBits base_expr_bits = base_bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, base_expr_bits, assignment.base_expression);

            // Print RHS expression
            TreeBits rhs_bits = bits.child(indent_lvl, true);
            Local::print_header(indent_lvl, rhs_bits, "RHS Expression ");
            std::cout << "to be" << std::endl;

            TreeBits rhs_expr_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, rhs_expr_bits, assignment.expression);
        }

        void print_statement(unsigned int indent_lvl, TreeBits &bits, const std::unique_ptr<StatementNode> &statement) {
            if (const auto *return_node = dynamic_cast<const ReturnNode *>(statement.get())) {
                print_return(indent_lvl, bits, *return_node);
            } else if (const auto *if_node = dynamic_cast<const IfNode *>(statement.get())) {
                print_if(indent_lvl, bits, *if_node);
            } else if (const auto *while_node = dynamic_cast<const WhileNode *>(statement.get())) {
                print_while(indent_lvl, bits, *while_node);
            } else if (const auto *for_node = dynamic_cast<const ForLoopNode *>(statement.get())) {
                print_for(indent_lvl, bits, *for_node);
            } else if (const auto *enh_for_node = dynamic_cast<const EnhForLoopNode *>(statement.get())) {
                print_enh_for(indent_lvl, bits, *enh_for_node);
            } else if (const auto *group_assignment = dynamic_cast<const GroupAssignmentNode *>(statement.get())) {
                print_group_assignment(indent_lvl, bits, *group_assignment);
            } else if (const auto *assignment = dynamic_cast<const AssignmentNode *>(statement.get())) {
                print_assignment(indent_lvl, bits, *assignment);
            } else if (const auto *group_declaration = dynamic_cast<const GroupDeclarationNode *>(statement.get())) {
                print_group_declaration(indent_lvl, bits, *group_declaration);
            } else if (const auto *declaration = dynamic_cast<const DeclarationNode *>(statement.get())) {
                print_declaration(indent_lvl, bits, *declaration);
            } else if (const auto *throw_node = dynamic_cast<const ThrowNode *>(statement.get())) {
                print_throw(indent_lvl, bits, *throw_node);
            } else if (const auto *catch_node = dynamic_cast<const CatchNode *>(statement.get())) {
                print_catch(indent_lvl, bits, *catch_node);
            } else if (const auto *call_node = dynamic_cast<const CallNodeBase *>(statement.get())) {
                print_call(indent_lvl, bits, *call_node);
            } else if (const auto *unary_op_node = dynamic_cast<const UnaryOpStatement *>(statement.get())) {
                print_unary_op(indent_lvl, bits, *unary_op_node);
            } else if (const auto *data_assignment = dynamic_cast<const DataFieldAssignmentNode *>(statement.get())) {
                print_data_field_assignment(indent_lvl, bits, *data_assignment);
            } else if (const auto *grouped_data_assignment = dynamic_cast<const GroupedDataFieldAssignmentNode *>(statement.get())) {
                print_grouped_data_field_assignment(indent_lvl, bits, *grouped_data_assignment);
            } else if (const auto *array_assignment = dynamic_cast<const ArrayAssignmentNode *>(statement.get())) {
                print_array_assignment(indent_lvl, bits, *array_assignment);
            } else if (const auto *stacked_assignment = dynamic_cast<const StackedAssignmentNode *>(statement.get())) {
                print_stacked_assignment(indent_lvl, bits, *stacked_assignment);
            } else if (const auto *switch_statement = dynamic_cast<const SwitchStatement *>(statement.get())) {
                print_switch_statement(indent_lvl, bits, *switch_statement);
            } else if (dynamic_cast<const BreakNode *>(statement.get())) {
                Local::print_header(indent_lvl, bits, "Break ");
                std::cout << std::endl;
            } else if (dynamic_cast<const ContinueNode *>(statement.get())) {
                Local::print_header(indent_lvl, bits, "Continue ");
                std::cout << std::endl;
            } else {
                assert(false);
            }
        }

        void print_body(unsigned int indent_lvl, TreeBits &bits, const std::vector<std::unique_ptr<StatementNode>> &body) {
            for (size_t i = 0; i < body.size(); i++) {
                bool is_last = (i + 1 == body.size());
                TreeBits child_bits = bits.child(indent_lvl, is_last);
                print_statement(indent_lvl, child_bits, body[i]);
            }
        }

        // --- DEFINITIONS ---

        // print_data
        //     Prints the content of the generated DataNode
        void print_data(unsigned int indent_lvl, TreeBits &bits, const DataNode &data) {
            Local::print_header(indent_lvl, bits, "Data ");
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
            for (auto field_it = data.fields.begin(); field_it != data.fields.end(); ++field_it) {
                if (field_it != data.fields.begin()) {
                    std::cout << ", ";
                }
                std::cout << std::get<0>(*field_it);
            }
            std::cout << "):" << std::endl;

            indent_lvl++;
            for (auto field = data.fields.begin(); field != data.fields.end(); ++field) {
                bool is_last = std::next(field) == data.fields.end();
                TreeBits field_bits = bits.child(indent_lvl, is_last);
                Local::print_header(indent_lvl, field_bits, "Field ");
                std::cout << std::get<1>(*field)->to_string() << " " << std::get<0>(*field) << "\n";
            }
        }

        // print_entity
        //     Prints the content of the generated EntityNode
        void print_entity([[maybe_unused]] unsigned int indent_lvl, [[maybe_unused]] TreeBits &bits,
            [[maybe_unused]] const EntityNode &entity) {
            Local::print_header(indent_lvl, bits, "Entity ");
            std::cout << typeid(entity).name() << "\n";
        }

        // print_enum
        //     Prints the content of the generated EnumNode
        void print_enum([[maybe_unused]] unsigned int indent_lvl, TreeBits &bits, const EnumNode &enum_node) {
            Local::print_header(indent_lvl, bits, "Enum ");
            std::cout << enum_node.name << std::endl;

            indent_lvl++;
            for (size_t i = 0; i < enum_node.values.size(); i++) {
                bool is_last = i + 1 == enum_node.values.size();
                TreeBits value_bits = bits.child(indent_lvl, is_last);
                Local::print_header(indent_lvl, value_bits, "Enum Value " + std::to_string(i) + " ");
                std::cout << enum_node.values[i] << std::endl;
            }
        }

        // print_error
        //     Prints the content of the generated ErrorNode
        void print_error([[maybe_unused]] unsigned int indent_lvl, [[maybe_unused]] TreeBits &bits,
            [[maybe_unused]] const ErrorNode &error) {
            Local::print_header(indent_lvl, bits, "Error ");
            std::cout << typeid(error).name() << "\n";
        }

        // print_func
        //     Prints the content of the generated FuncNode
        void print_func([[maybe_unused]] unsigned int indent_lvl, [[maybe_unused]] TreeBits &bits, [[maybe_unused]] const FuncNode &func) {
            Local::print_header(indent_lvl, bits, "Func ");
            std::cout << typeid(func).name() << "\n";
        }

        /// print_function
        ///     Prints the content of the generated FunctionNode
        void print_function(unsigned int indent_lvl, TreeBits &bits, const FunctionNode &function) {
            Local::print_header(indent_lvl, bits, "Function ");

            if (function.is_aligned) {
                std::cout << "aligned ";
            }
            if (function.is_const) {
                std::cout << "const ";
            }
            std::cout << function.name << "(";
            size_t counter = 0;
            for (const std::tuple<std::shared_ptr<Type>, std::string, bool> &param : function.parameters) {
                std::cout << (std::get<2>(param) ? "mut" : "const") << " "; // Whether the param is const or mut
                std::cout << (primitives.find(std::get<0>(param)->to_string()) == primitives.end() ? "&" : ""); // If primitive or complex
                std::cout << std::get<0>(param)->to_string() << " ";                                            // The actual type
                std::cout << std::get<1>(param);                                                                // The parameter name
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
            for (const std::shared_ptr<Type> &ret : function.return_types) {
                std::cout << ret->to_string();
                if (++counter != function.return_types.size()) {
                    std::cout << ", ";
                }
            }
            if (function.return_types.size() > 1) {
                std::cout << ")";
            }
            std::cout << " [s" << function.scope->scope_id << "]" << std::endl;

            // The function body
            print_body(indent_lvl + 1, bits, function.scope->body);
        }

        /// print_import
        ///     Prints the content of the generated ImportNode
        void print_import(unsigned int indent_lvl, TreeBits &bits, const ImportNode &import) {
            Local::print_header(indent_lvl, bits, "Import ");

            if (std::holds_alternative<std::pair<std::optional<std::string>, std::string>>(import.path)) {
                const auto &path_pair = std::get<std::pair<std::optional<std::string>, std::string>>(import.path);
                std::cout << "\"";
                if (path_pair.first.has_value()) {
                    std::cout << path_pair.first.value() << "/";
                }
                std::cout << path_pair.second << ".ft\"";
            } else if (std::holds_alternative<std::vector<std::string>>(import.path)) {
                std::vector<std::string> path_vector = std::get<std::vector<std::string>>(import.path);
                auto iterator = path_vector.begin();
                while (iterator != path_vector.end()) {
                    if (iterator != path_vector.begin()) {
                        std::cout << ".";
                    }
                    std::cout << *iterator;
                    ++iterator;
                }
            }

            if (import.alias.has_value()) {
                std::cout << " as " << import.alias.value();
            }

            std::cout << std::endl;
        }

        /// print_link
        ///     Prints the content of the generated LinkNode
        void print_link([[maybe_unused]] unsigned int indent_lvl, [[maybe_unused]] TreeBits &bits, [[maybe_unused]] const LinkNode &link) {
            Local::print_header(indent_lvl, bits, "Link ");
            std::cout << typeid(link).name() << "\n";
        }

        /// print_variant
        ///     Prints the content of the generated VariantNode
        void print_variant([[maybe_unused]] unsigned int indent_lvl, [[maybe_unused]] TreeBits &bits,
            [[maybe_unused]] const VariantNode &variant) {
            Local::print_header(indent_lvl, bits, "Variant ");
            std::cout << typeid(variant).name() << "\n";
        }

        void print_test(unsigned int indent_lvl, TreeBits &bits, const TestNode &test) {
            Local::print_header(indent_lvl, bits, "Test ");
            std::cout << test.file_name << " : \"" << test.name << "\" [s" << test.scope->scope_id << "]" << std::endl;
            print_body(indent_lvl + 1, bits, test.scope->body);
        }
    } // namespace AST
} // namespace Debug
