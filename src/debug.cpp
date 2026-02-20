#include "debug.hpp"

#include "globals.hpp"
#include "lexer/lexer_utils.hpp"
#include "parser/ast/definitions/definition_node.hpp"
#include "parser/ast/expressions/call_node_expression.hpp"
#include "parser/ast/expressions/data_access_node.hpp"
#include "parser/ast/expressions/instance_call_node_expression.hpp"
#include "parser/ast/expressions/unary_op_expression.hpp"
#include "parser/ast/scope.hpp"
#include "parser/ast/statements/call_node_statement.hpp"
#include "parser/ast/statements/do_while_node.hpp"
#include "parser/ast/statements/instance_call_node_statement.hpp"
#include "parser/ast/statements/statement_node.hpp"
#include "parser/ast/statements/unary_op_statement.hpp"
#include "parser/parser.hpp"
#include "persistent_thread_pool.hpp"

bool PRINT_TOK_STREAM = true;
bool PRINT_DEP_TREE = true;
bool PRINT_AST = true;
bool PRINT_IR_PROGRAM = true;
bool PRINT_PROFILE_RESULTS = true;
bool PRINT_CUMULATIVE_PROFILE_RESULTS = false;
bool PRINT_FILE_IR = false;
bool HARD_CRASH = false;
bool NO_BINARY = false;
bool NO_GENERATION = false;

std::string RED = "\033[31m";
std::string GREEN = "\033[32m";
std::string YELLOW = "\033[33m";
std::string BLUE = "\033[34m";
std::string CYAN = "\033[36m";
std::string WHITE = "\033[37m";
std::string DEFAULT = "\033[0m";
std::string GREY = "\033[90m";
std::string RED_UNDERLINE = "\033[4:3;58;2;255;0;0m";

unsigned int BUILTIN_LIBS_TO_PRINT = 0;
Target COMPILATION_TARGET = Target::NATIVE;
ArithmeticOverflowMode overflow_mode = ArithmeticOverflowMode::PRINT;
ArrayOutOfBoundsMode oob_mode = ArrayOutOfBoundsMode::PRINT;
OptionalUnwrapMode opt_unwrap_mode = OptionalUnwrapMode::CRASH;
VariantUnwrapMode var_unwrap_mode = VariantUnwrapMode::CRASH;

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
#ifdef DEBUG_BUILD
        if (!PRINT_TOK_STREAM) {
            return;
        }
#endif
        std::cout << YELLOW << "[Debug Info] Printing token vector of file '" << file_name << "'" << DEFAULT << std::endl;
        std::stringstream type_stream;
        for (auto tc = tokens.first; tc != tokens.second; ++tc) {
            type_stream << " | Type: '" << get_token_name(tc->token) << "' => " << std::to_string(static_cast<int>(tc->token));
            std::cout << "Line: " << tc->line << " | Column: " << tc->column << " | FileID: " << tc->file_id
                      << get_string_container(30, type_stream.str()) << " | Lexme: ";
            if (tc->token == TOK_TYPE) {
                std::cout << tc->type->to_string();
            } else if (tc->token == TOK_ALIAS) {
                if (tc->alias_namespace->file_node == nullptr) {
                    std::cout << "Core";
                } else {
                    std::cout << tc->alias_namespace->file_node->file_name;
                }
                std::cout << ":" << tc->alias_namespace->namespace_hash.to_string();
            } else {
                std::cout << tc->lexme;
            }
            std::cout << "\n";
            type_stream.str(std::string());
            type_stream.clear();
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
            for (const auto &parser : Parser::instances) {
                print_file(*parser.file_node_ptr.get());
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

            for (const std::unique_ptr<DefinitionNode> &def : file.file_namespace->public_symbols.definitions) {
                // Create fresh TreeBits for each node
                TreeBits bits;

                // Set the first bit whether this is the
                if (++counter == file.file_namespace->public_symbols.definitions.size()) {
                    bits.set(SINGLE, 0);
                } else {
                    bits.set(BRANCH, 0);
                }

                switch (def->get_variation()) {
                    case DefinitionNode::Variation::DATA: {
                        const auto *node = def->as<DataNode>();
                        print_data(0, bits, *node);
                        break;
                    }
                    case DefinitionNode::Variation::ENTITY: {
                        const auto *node = def->as<EntityNode>();
                        print_entity(0, bits, *node);
                        break;
                    }
                    case DefinitionNode::Variation::ENUM: {
                        const auto *node = def->as<EnumNode>();
                        print_enum(0, bits, *node);
                        break;
                    }
                    case DefinitionNode::Variation::ERROR: {
                        const auto *node = def->as<ErrorNode>();
                        print_error(0, bits, *node);
                        break;
                    }
                    case DefinitionNode::Variation::FUNC: {
                        const auto *node = def->as<FuncNode>();
                        print_func(0, bits, *node);
                        break;
                    }
                    case DefinitionNode::Variation::FUNCTION: {
                        const auto *node = def->as<FunctionNode>();
                        print_function(0, bits, *node);
                        break;
                    }
                    case DefinitionNode::Variation::IMPORT: {
                        const auto *node = def->as<ImportNode>();
                        print_import(0, bits, *node);
                        break;
                    }
                    case DefinitionNode::Variation::LINK: {
                        const auto *node = def->as<LinkNode>();
                        print_link(0, bits, *node);
                        break;
                    }
                    case DefinitionNode::Variation::TEST: {
                        const auto *node = def->as<TestNode>();
                        print_test(0, bits, *node);
                        break;
                    }
                    case DefinitionNode::Variation::VARIANT: {
                        const auto *node = def->as<VariantNode>();
                        print_variant(0, bits, *node);
                        break;
                    }
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
            if (std::holds_alternative<LitInt>(lit.value)) {
                std::cout << std::get<LitInt>(lit.value).value.to_string();
            } else if (std::holds_alternative<LitFloat>(lit.value)) {
                std::cout << std::get<LitFloat>(lit.value).value.to_string();
            } else if (std::holds_alternative<LitBool>(lit.value)) {
                std::cout << (std::get<LitBool>(lit.value).value ? "true" : "false");
            } else if (std::holds_alternative<LitU8>(lit.value)) {
                std::cout << "'" << std::string(1, std::get<LitU8>(lit.value).value) << "'";
            } else if (std::holds_alternative<LitStr>(lit.value)) {
                const std::string &lit_val = std::get<LitStr>(lit.value).value;
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
            } else if (std::holds_alternative<LitOptional>(lit.value)) {
                std::cout << "none";
            } else if (std::holds_alternative<LitEnum>(lit.value)) {
                const LitEnum &lit_enum = std::get<LitEnum>(lit.value);
                std::cout << lit_enum.enum_type->to_string() << ".";
                if (lit_enum.values.size() == 1) {
                    std::cout << lit_enum.values.front();
                } else {
                    std::cout << "(";
                    for (auto it = lit_enum.values.begin(); it != lit_enum.values.end(); ++it) {
                        if (it != lit_enum.values.begin()) {
                            std::cout << ", ";
                        }
                        std::cout << *it;
                    }
                    std::cout << ")";
                }
            } else if (std::holds_alternative<LitVariantTag>(lit.value)) {
                const LitVariantTag &lit_var = std::get<LitVariantTag>(lit.value);
                std::cout << lit_var.variant_type->to_string() << "." << lit_var.variation_type->to_string();
            } else if (std::holds_alternative<LitError>(lit.value)) {
                const LitError &lit_error = std::get<LitError>(lit.value);
                std::cout << lit_error.error_type->to_string() << "." << lit_error.value;
                if (lit_error.message.has_value()) {
                    std::cout << " with message" << std::endl;
                    TreeBits message_bits = bits.child(indent_lvl + 1, true);
                    print_expression(indent_lvl + 1, message_bits, lit_error.message.value());
                    return;
                }
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
            if (!call.error_types.empty()) {
                std::cout << "throws[";
                for (auto it = call.error_types.begin(); it != call.error_types.end(); ++it) {
                    if (it != call.error_types.begin()) {
                        std::cout << ", ";
                    }
                    std::cout << (*it)->to_string();
                }
                std::cout << "] ";
            }
            std::cout << call.function->name << "(";
            for (auto it = call.arguments.begin(); it != call.arguments.end(); ++it) {
                if (it != call.arguments.begin()) {
                    std::cout << ", ";
                }
                std::cout << (it->second ? "ref" : "val");
            }
            std::cout << ") [c" << call.call_id << "] in [s" << call.scope_id << "]";
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

        void print_instance_call(unsigned int indent_lvl, TreeBits &bits, const InstanceCallNodeBase &call) {
            Local::print_header(indent_lvl, bits, "Instance Call ");
            assert(call.instance_variable->get_variation() == ExpressionNode::Variation::VARIABLE);
            if (!call.error_types.empty()) {
                std::cout << "throws[";
                for (auto it = call.error_types.begin(); it != call.error_types.end(); ++it) {
                    if (it != call.error_types.begin()) {
                        std::cout << ", ";
                    }
                    std::cout << (*it)->to_string();
                }
                std::cout << "] ";
            }
            const auto &variable_node = call.instance_variable->as<VariableNode>();
            std::cout << variable_node->name << "." << call.function->name << "(";
            for (auto it = call.arguments.begin(); it != call.arguments.end(); ++it) {
                if (it != call.arguments.begin()) {
                    std::cout << ", ";
                }
                std::cout << (it->second ? "ref" : "val");
            }
            std::cout << ") [c" << call.call_id << "] in [s" << call.scope_id << "]";
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

        void print_type_node(unsigned int indent_lvl, TreeBits &bits, const TypeNode &type) {
            Local::print_header(indent_lvl, bits, "Type ");
            std::cout << type.type->to_string() << std::endl;
        }

        void print_initializer(unsigned int indent_lvl, TreeBits &bits, const InitializerNode &initializer) {
            Local::print_header(indent_lvl, bits, "Initializer ");
            std::cout << initializer.type->to_string() << std::endl;

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

        void print_range_expression(unsigned int indent_lvl, TreeBits &bits, const RangeExpressionNode &range) {
            Local::print_header(indent_lvl, bits, "Range Expr ");
            std::cout << range.type->to_string() << std::endl;

            indent_lvl++;
            TreeBits lower_bits = bits.child(indent_lvl, false);
            Local::print_header(indent_lvl, lower_bits, "Lower Bound ");
            std::cout << range.lower_bound->type->to_string() << std::endl;
            TreeBits lower_expr_bits = lower_bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, lower_expr_bits, range.lower_bound);

            TreeBits upper_bits = bits.child(indent_lvl, true);
            Local::print_header(indent_lvl, upper_bits, "Upper Bound ");
            std::cout << range.upper_bound->type->to_string() << std::endl;
            TreeBits upper_expr_bits = upper_bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, upper_expr_bits, range.upper_bound);
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
            std::cout << access.type->to_string() << std::endl;
            indent_lvl++;
            TreeBits base_expr_header_bits = bits.child(indent_lvl, false);
            Local::print_header(indent_lvl, base_expr_header_bits, "Base Expr ");
            std::cout << std::endl;
            TreeBits base_expr_bits = base_expr_header_bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, base_expr_bits, access.base_expr);

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
            if (access.field_name.has_value()) {
                std::cout << access.field_name.value() << " at ID " << access.field_id;
            } else {
                std::cout << "$" << access.field_id;
            }
            std::cout << " with type " << access.type->to_string() << " of base expression:" << std::endl;
            TreeBits base_expr_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, base_expr_bits, access.base_expr);
        }

        void print_grouped_data_access(unsigned int indent_lvl, TreeBits &bits, const GroupedDataAccessNode &access) {
            Local::print_header(indent_lvl, bits, "Grouped Access ");
            std::cout << ".(";
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
            std::cout << " on base expr:" << std::endl;
            TreeBits base_expr_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, base_expr_bits, access.base_expr);
        }

        void print_switch_match(unsigned int indent_lvl, TreeBits &bits, const SwitchMatchNode &match) {
            Local::print_header(indent_lvl, bits, "Match ");
            std::cout << "[" << match.id << "]: " << match.type->to_string() << " " << match.name << std::endl;
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
                    Local::print_header(indent_lvl + 2, case_match_header_bits, "CaseMatch ");
                    std::cout << "[s" << branch->scope->scope_id << "]\n";
                    TreeBits case_match_bits = case_match_header_bits.child(indent_lvl + 3, std::next(it) == branch->matches.end());
                    print_expression(indent_lvl + 3, case_match_bits, *it);
                }
                TreeBits case_expr_header_bits = case_bits.child(indent_lvl + 2, true);
                Local::print_header(indent_lvl + 2, case_expr_header_bits, "CaseExpr ");
                std::cout << "\n";
                TreeBits case_expr_bits = case_expr_header_bits.child(indent_lvl + 3, true);
                print_expression(indent_lvl + 3, case_expr_bits, branch->expr);
            }
        }

        void print_default(unsigned int indent_lvl, TreeBits &bits, const DefaultNode &default_node) {
            Local::print_header(indent_lvl, bits, "Default ");
            std::cout << "of type " << default_node.type->to_string() << std::endl;
        }

        void print_optional_chain(unsigned int indent_lvl, TreeBits &bits, const OptionalChainNode &chain_node) {
            Local::print_header(indent_lvl, bits, "OptChain ");
            if (std::holds_alternative<ChainFieldAccess>(chain_node.operation)) {
                const ChainFieldAccess &access = std::get<ChainFieldAccess>(chain_node.operation);
                if (access.field_name.has_value()) {
                    std::cout << "." << access.field_name.value() << " at id " << access.field_id;
                } else {
                    std::cout << ".$" << access.field_id;
                }
                std::cout << std::endl;
            } else if (std::holds_alternative<ChainArrayAccess>(chain_node.operation)) {
                const ChainArrayAccess &access = std::get<ChainArrayAccess>(chain_node.operation);
                std::cout << "array access at indices" << std::endl;
                for (size_t idx = 0; idx < access.indexing_expressions.size(); idx++) {
                    TreeBits idx_bits = bits.child(indent_lvl + 1, false);
                    Local::print_header(indent_lvl + 1, idx_bits, "IDX " + std::to_string(idx) + " ");
                    std::cout << std::endl;
                    TreeBits expr_bits = idx_bits.child(indent_lvl + 2, true);
                    print_expression(indent_lvl + 2, expr_bits, access.indexing_expressions.at(idx));
                }
            }
            TreeBits base_expr_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, base_expr_bits, chain_node.base_expr);
        }

        void print_optional_unwrap(unsigned int indent_lvl, TreeBits &bits, const OptionalUnwrapNode &unwrap_node) {
            Local::print_header(indent_lvl, bits, "OptUnwrap ");
            std::cout << "[" << unwrap_node.type->to_string() << "]" << std::endl;
            TreeBits base_expr_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, base_expr_bits, unwrap_node.base_expr);
        }

        void print_variant_extraction(unsigned int indent_lvl, TreeBits &bits, const VariantExtractionNode &extraction) {
            Local::print_header(indent_lvl, bits, "VariantExtract ");
            std::cout << "?(" << extraction.extracted_type->to_string() << ") -> " << extraction.type->to_string() << std::endl;
            TreeBits base_expr_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, base_expr_bits, extraction.base_expr);
        }

        void print_variant_unwrap(unsigned int indent_lvl, TreeBits &bits, const VariantUnwrapNode &unwrap_node) {
            Local::print_header(indent_lvl, bits, "VarUnwrap ");
            std::cout << "[" << unwrap_node.type->to_string() << "]" << std::endl;
            TreeBits base_expr_bits = bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, base_expr_bits, unwrap_node.base_expr);
        }

        void print_expression(unsigned int indent_lvl, TreeBits &bits, const std::unique_ptr<ExpressionNode> &expr) {
            switch (expr->get_variation()) {
                case ExpressionNode::Variation::ARRAY_ACCESS: {
                    const auto *node = expr->as<ArrayAccessNode>();
                    print_array_access(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::ARRAY_INITIALIZER: {
                    const auto *node = expr->as<ArrayInitializerNode>();
                    print_array_initializer(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::BINARY_OP: {
                    const auto *node = expr->as<BinaryOpNode>();
                    print_binary_op(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::CALL: {
                    const auto *node = expr->as<CallNodeExpression>();
                    print_call(indent_lvl, bits, *static_cast<const CallNodeBase *>(node));
                    break;
                }
                case ExpressionNode::Variation::DATA_ACCESS: {
                    const auto *node = expr->as<DataAccessNode>();
                    print_data_access(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::DEFAULT: {
                    const auto *node = expr->as<DefaultNode>();
                    print_default(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::GROUP_EXPRESSION: {
                    const auto *node = expr->as<GroupExpressionNode>();
                    print_group_expression(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::GROUPED_DATA_ACCESS: {
                    const auto *node = expr->as<GroupedDataAccessNode>();
                    print_grouped_data_access(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::INITIALIZER: {
                    const auto *node = expr->as<InitializerNode>();
                    print_initializer(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::INSTANCE_CALL: {
                    const auto *node = expr->as<InstanceCallNodeExpression>();
                    print_instance_call(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::LITERAL: {
                    const auto *node = expr->as<LiteralNode>();
                    print_literal(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::OPTIONAL_CHAIN: {
                    const auto *node = expr->as<OptionalChainNode>();
                    print_optional_chain(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::OPTIONAL_UNWRAP: {
                    const auto *node = expr->as<OptionalUnwrapNode>();
                    print_optional_unwrap(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::RANGE_EXPRESSION: {
                    const auto *node = expr->as<RangeExpressionNode>();
                    print_range_expression(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::STRING_INTERPOLATION: {
                    const auto *node = expr->as<StringInterpolationNode>();
                    print_string_interpolation(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::SWITCH_EXPRESSION: {
                    const auto *node = expr->as<SwitchExpression>();
                    print_switch_expression(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::SWITCH_MATCH: {
                    const auto *node = expr->as<SwitchMatchNode>();
                    print_switch_match(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::TYPE_CAST: {
                    const auto *node = expr->as<TypeCastNode>();
                    print_type_cast(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::TYPE: {
                    const auto *node = expr->as<TypeNode>();
                    print_type_node(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::UNARY_OP: {
                    const auto *node = expr->as<UnaryOpExpression>();
                    print_unary_op(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::VARIABLE: {
                    const auto *node = expr->as<VariableNode>();
                    print_variable(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::VARIANT_EXTRACTION: {
                    const auto *node = expr->as<VariantExtractionNode>();
                    print_variant_extraction(indent_lvl, bits, *node);
                    break;
                }
                case ExpressionNode::Variation::VARIANT_UNWRAP: {
                    const auto *node = expr->as<VariantUnwrapNode>();
                    print_variant_unwrap(indent_lvl, bits, *node);
                    break;
                }
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

        void print_do_while(unsigned int indent_lvl, TreeBits &bits, const DoWhileNode &do_while_node) {
            Local::print_header(indent_lvl, bits, "DoWhile ");
            std::cout << "do .. while" << std::endl;

            TreeBits cond_bits = bits.child(indent_lvl + 1, false);
            print_expression(indent_lvl + 1, cond_bits, do_while_node.condition);

            TreeBits do_bits = bits.child(indent_lvl, true);
            Local::print_header(indent_lvl, do_bits, "Do ");
            std::cout << "do [s" << do_while_node.scope->scope_id << "]" << std::endl;

            TreeBits body_bits = bits.child(indent_lvl + 1, true);
            print_body(indent_lvl + 1, body_bits, do_while_node.scope->body);
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
            std::cout << "catch call '";
            std::cout << catch_node.call_node->function->name;
            std::cout << "'";
            if (catch_node.var_name.has_value()) {
                std::cout << " in var '";
                std::cout << catch_node.var_name.value();
                std::cout << "' of type '";
                std::cout << catch_node.scope->variables.at(catch_node.var_name.value()).type->to_string();
                std::cout << "'";
            } else {
                std::cout << " on implicit error variable of type '";
                std::cout << catch_node.scope->variables.at("flint.value_err").type->to_string();
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

        void print_stacked_array_assignment(unsigned int indent_lvl, TreeBits &bits, const StackedArrayAssignmentNode &assignment) {
            Local::print_header(indent_lvl, bits, "Stacked Array Assignment ");
            std::cout << "on base expr" << std::endl;

            indent_lvl++;
            TreeBits base_expr_header_bits = bits.child(indent_lvl, false);
            Local::print_header(indent_lvl, base_expr_header_bits, "Base Expr ");
            std::cout << std::endl;

            TreeBits base_expr_bits = base_expr_header_bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, base_expr_bits, assignment.base_expression);

            for (size_t i = 0; i < assignment.indexing_expressions.size(); i++) {
                TreeBits index_header_bits = bits.child(indent_lvl, false);
                Local::print_header(indent_lvl, index_header_bits, "ID " + std::to_string(i) + " ");
                std::cout << std::endl;

                TreeBits index_bits = index_header_bits.child(indent_lvl + 1, true);
                print_expression(indent_lvl + 1, index_bits, assignment.indexing_expressions.at(i));
            }

            // Print RHS expression
            TreeBits rhs_expr_header_bits = bits.child(indent_lvl, true);
            Local::print_header(indent_lvl, rhs_expr_header_bits, "RHS Expression ");
            std::cout << "to be" << std::endl;

            TreeBits rhs_expr_bits = rhs_expr_header_bits.child(indent_lvl + 1, true);
            print_expression(indent_lvl + 1, rhs_expr_bits, assignment.expression);
        }

        void print_stacked_grouped_assignment(unsigned int indent_lvl, TreeBits &bits, const StackedGroupedAssignmentNode &assignment) {
            Local::print_header(indent_lvl, bits, "Stacked Grouped Assignment ");
            std::cout << "Fields (";
            for (auto field_it = assignment.field_names.begin(); field_it != assignment.field_names.end(); ++field_it) {
                if (field_it != assignment.field_names.begin()) {
                    std::cout << ", ";
                }
                std::cout << *field_it;
            }
            std::cout << ") at ids (";
            for (auto field_it = assignment.field_ids.begin(); field_it != assignment.field_ids.end(); ++field_it) {
                if (field_it != assignment.field_ids.begin()) {
                    std::cout << ", ";
                }
                std::cout << *field_it;
            }
            std::cout << ") of types (";
            for (auto field_it = assignment.field_types.begin(); field_it != assignment.field_types.end(); ++field_it) {
                if (field_it != assignment.field_types.begin()) {
                    std::cout << ", ";
                }
                std::cout << (*field_it)->to_string();
            }
            std::cout << ") on base" << std::endl;

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
            switch (statement->get_variation()) {
                case StatementNode::Variation::ARRAY_ASSIGNMENT: {
                    const auto *node = statement->as<ArrayAssignmentNode>();
                    print_array_assignment(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::ASSIGNMENT: {
                    const auto *node = statement->as<AssignmentNode>();
                    print_assignment(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::BREAK:
                    Local::print_header(indent_lvl, bits, "Break ");
                    std::cout << std::endl;
                    break;
                case StatementNode::Variation::CALL: {
                    const auto *node = statement->as<CallNodeStatement>();
                    print_call(indent_lvl, bits, *static_cast<const CallNodeBase *>(node));
                    break;
                }
                case StatementNode::Variation::CATCH: {
                    const auto *node = statement->as<CatchNode>();
                    print_catch(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::CONTINUE:
                    Local::print_header(indent_lvl, bits, "Continue ");
                    std::cout << std::endl;
                    break;
                case StatementNode::Variation::DATA_FIELD_ASSIGNMENT: {
                    const auto *node = statement->as<DataFieldAssignmentNode>();
                    print_data_field_assignment(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::DECLARATION: {
                    const auto *node = statement->as<DeclarationNode>();
                    print_declaration(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::DO_WHILE: {
                    const auto *node = statement->as<DoWhileNode>();
                    print_do_while(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::ENHANCED_FOR_LOOP: {
                    const auto *node = statement->as<EnhForLoopNode>();
                    print_enh_for(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::FOR_LOOP: {
                    const auto *node = statement->as<ForLoopNode>();
                    print_for(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::GROUP_ASSIGNMENT: {
                    const auto *node = statement->as<GroupAssignmentNode>();
                    print_group_assignment(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::GROUP_DECLARATION: {
                    const auto *node = statement->as<GroupDeclarationNode>();
                    print_group_declaration(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::GROUPED_DATA_FIELD_ASSIGNMENT: {
                    const auto *node = statement->as<GroupedDataFieldAssignmentNode>();
                    print_grouped_data_field_assignment(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::IF: {
                    const auto *node = statement->as<IfNode>();
                    print_if(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::INSTANCE_CALL: {
                    const auto *node = statement->as<InstanceCallNodeStatement>();
                    print_instance_call(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::RETURN: {
                    const auto *node = statement->as<ReturnNode>();
                    print_return(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::STACKED_ASSIGNMENT: {
                    const auto *node = statement->as<StackedAssignmentNode>();
                    print_stacked_assignment(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::STACKED_ARRAY_ASSIGNMENT: {
                    const auto *node = statement->as<StackedArrayAssignmentNode>();
                    print_stacked_array_assignment(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::STACKED_GROUPED_ASSIGNMENT: {
                    const auto *node = statement->as<StackedGroupedAssignmentNode>();
                    print_stacked_grouped_assignment(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::SWITCH: {
                    const auto *node = statement->as<SwitchStatement>();
                    print_switch_statement(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::THROW: {
                    const auto *node = statement->as<ThrowNode>();
                    print_throw(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::UNARY_OP: {
                    const auto *node = statement->as<UnaryOpStatement>();
                    print_unary_op(indent_lvl, bits, *node);
                    break;
                }
                case StatementNode::Variation::WHILE: {
                    const auto *node = statement->as<WhileNode>();
                    print_while(indent_lvl, bits, *node);
                    break;
                }
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
                std::cout << field_it->name;
            }
            std::cout << "):" << std::endl;

            indent_lvl++;
            for (auto field = data.fields.begin(); field != data.fields.end(); ++field) {
                bool is_last = std::next(field) == data.fields.end();
                TreeBits field_bits = bits.child(indent_lvl, is_last);
                Local::print_header(indent_lvl, field_bits, "Field ");
                std::cout << field->type->to_string() << " " << field->name << "\n";
                if (field->initializer.has_value()) {
                    TreeBits init_bits = field_bits.child(indent_lvl + 1, true);
                    Local::print_header(indent_lvl + 1, init_bits, "Initializer ");
                    std::cout << field->initializer.value()->type->to_string();
                    std::cout << std::endl;

                    TreeBits value_bits = field_bits.child(indent_lvl + 2, true);
                    print_expression(indent_lvl + 2, value_bits, field->initializer.value());
                }
            }
        }

        // print_entity
        //     Prints the content of the generated EntityNode
        void print_entity(unsigned int indent_lvl, TreeBits &bits, const EntityNode &entity) {
            Local::print_header(indent_lvl, bits, "Entity ");
            std::cout << entity.name << "(";
            for (size_t i = 0; i < entity.constructor_order.size(); i++) {
                if (i > 0) {
                    std::cout << ", ";
                }
                std::cout << entity.data_modules.at(entity.constructor_order.at(i))->name;
            }
            std::cout << ")";
            if (!entity.parent_entities.empty()) {
                std::cout << " extends(";
                for (size_t i = 0; i < entity.parent_entities.size(); i++) {
                    if (i > 0) {
                        std::cout << ", ";
                    }
                    const auto &pe = entity.parent_entities.at(i);
                    std::cout << pe.first->to_string() << " " << pe.second;
                }
                std::cout << ")";
            }
            std::cout << "\n";
            TreeBits data_bits = bits.child(indent_lvl + 1, false);
            Local::print_header(indent_lvl + 1, data_bits, "Data ");
            std::cout << "\n";
            for (size_t i = 0; i < entity.data_modules.size(); i++) {
                TreeBits data_module_bits = data_bits.child(indent_lvl + 2, i + 1 == entity.data_modules.size());
                Local::print_header(indent_lvl + 2, data_module_bits, entity.data_modules.at(i)->name + " ");
                std::cout << "\n";
            }

            TreeBits func_bits = bits.child(indent_lvl + 1, true);
            Local::print_header(indent_lvl + 1, func_bits, "Func ");
            std::cout << "\n";
            for (size_t i = 0; i < entity.func_modules.size(); i++) {
                TreeBits func_module_bits = func_bits.child(indent_lvl + 2, i + 1 == entity.func_modules.size());
                Local::print_header(indent_lvl + 2, func_module_bits, entity.func_modules.at(i)->name + " ");
                std::cout << "\n";
            }
        }

        // print_enum
        //     Prints the content of the generated EnumNode
        void print_enum(unsigned int indent_lvl, TreeBits &bits, const EnumNode &enum_node) {
            Local::print_header(indent_lvl, bits, "Enum ");
            std::cout << enum_node.name << std::endl;

            indent_lvl++;
            for (size_t i = 0; i < enum_node.values.size(); i++) {
                bool is_last = i + 1 == enum_node.values.size();
                TreeBits value_bits = bits.child(indent_lvl, is_last);
                Local::print_header(indent_lvl, value_bits, "Enum Value " + std::to_string(i) + " ");
                std::cout << enum_node.values[i].first << " = " << enum_node.values[i].second << std::endl;
            }
        }

        // print_error
        //     Prints the content of the generated ErrorNode
        void print_error(unsigned int indent_lvl, TreeBits &bits, const ErrorNode &error) {
            Local::print_header(indent_lvl, bits, "Error ");
            std::cout << error.name << "(" << error.parent_error << ") ID [" << error.error_id << "]" << std::endl;

            indent_lvl++;
            for (size_t i = 0; i < error.values.size(); i++) {
                bool is_last = i + 1 == error.values.size();
                TreeBits value_bits = bits.child(indent_lvl, is_last);
                const unsigned int error_value = error.get_id_msg_pair_of_value(error.values.at(i)).value().first;
                Local::print_header(indent_lvl, value_bits, "Error Value " + std::to_string(error_value) + " ");
                std::cout << error.values[i] << "(\"" << error.default_messages[i] << "\")" << std::endl;
            }
        }

        // print_func
        //     Prints the content of the generated FuncNode
        void print_func(unsigned int indent_lvl, TreeBits &bits, const FuncNode &func) {
            Local::print_header(indent_lvl, bits, "Func ");
            std::cout << func.name;
            if (!func.required_data.empty()) {
                std::cout << " requires(";
                for (size_t i = 0; i < func.required_data.size(); i++) {
                    if (i > 0) {
                        std::cout << ", ";
                    }
                    const auto &rd = func.required_data.at(i);
                    std::cout << rd.first->to_string() << " " << rd.second;
                }
                std::cout << ")";
            }
            std::cout << "\n";
            for (size_t i = 0; i < func.functions.size(); i++) {
                const auto *function = func.functions.at(i);
                TreeBits func_body_bits = bits.child(indent_lvl + 1, i + 1 == func.functions.size());
                Local::print_header(indent_lvl + 1, func_body_bits, "Function ");
                std::cout << function->name << "(";
                for (size_t j = 0; j < function->parameters.size(); j++) {
                    const auto &param = function->parameters.at(j);
                    if (j > 0) {
                        std::cout << ", ";
                    }
                    if (std::get<2>(param)) {
                        std::cout << "mut ";
                    } else {
                        std::cout << "const ";
                    }
                    std::cout << std::get<0>(param)->to_string() << " " << std::get<1>(param);
                }
                std::cout << ")\n";
            }
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
            if (function.is_extern) {
                std::cout << "extern ";
            }
            std::cout << function.name;
            std::cout << "(";
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
            if (!function.error_types.empty()) {
                std::cout << " {";
                for (auto it = function.error_types.begin(); it != function.error_types.end(); ++it) {
                    if (it != function.error_types.begin()) {
                        std::cout << ", ";
                    }
                    std::cout << (*it)->to_string();
                }
                std::cout << "}";
            }
            if (!function.scope.has_value()) {
                std::cout << ";" << std::endl;
                return;
            }
            std::cout << " [s" << function.scope.value()->scope_id << "]" << std::endl;

            // The function body
            print_body(indent_lvl + 1, bits, function.scope.value()->body);
        }

        /// print_import
        ///     Prints the content of the generated ImportNode
        void print_import(unsigned int indent_lvl, TreeBits &bits, const ImportNode &import) {
            Local::print_header(indent_lvl, bits, "Import ");

            if (std::holds_alternative<Hash>(import.path)) {
                const auto &file_hash = import.file_hash;
                const auto &import_hash = std::get<Hash>(import.path);
                std::cout << "\"" << std::filesystem::relative(import_hash.path, file_hash.path.parent_path()).string() << "\"";
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
        void print_variant(unsigned int indent_lvl, TreeBits &bits, const VariantNode &variant) {
            Local::print_header(indent_lvl, bits, "Variant ");
            std::cout << variant.name << "\n";

            indent_lvl++;
            for (auto type = variant.possible_types.begin(); type != variant.possible_types.end(); ++type) {
                bool is_last = std::next(type) == variant.possible_types.end();
                TreeBits field_bits = bits.child(indent_lvl, is_last);
                Local::print_header(indent_lvl, field_bits, "Type ");
                if (type->first.has_value()) {
                    std::cout << type->first.value() << "(" << type->second->to_string() << ")\n";
                } else {
                    std::cout << type->second->to_string() << "\n";
                }
            }
        }

        void print_test(unsigned int indent_lvl, TreeBits &bits, const TestNode &test) {
            Local::print_header(indent_lvl, bits, "Test ");
            const std::string &file_name = test.file_hash.path.filename().string();
            std::cout << file_name << " : \"" << test.name << "\" [s" << test.scope->scope_id << "]" << std::endl;
            print_annotations(indent_lvl + 1, bits, test.annotations);
            print_body(indent_lvl + 1, bits, test.scope->body);
        }

        void print_annotations(unsigned int indent_lvl, TreeBits &bits, const std::vector<AnnotationNode> &annotations) {
            if (annotations.empty()) {
                return;
            }
            TreeBits header_bits = bits.child(indent_lvl, false);
            Local::print_header(indent_lvl, header_bits, "Annotations ");
            std::cout << std::endl;

            for (size_t i = 0; i < annotations.size(); i++) {
                const auto &annotation = annotations.at(i);
                TreeBits annotation_bits = header_bits.child(indent_lvl + 1, i + 1 == annotations.size());
                Local::print_header(indent_lvl + 1, annotation_bits, "Annotation ");
                std::cout << annotation_map_rev.at(annotation.kind);
                if (!annotation.arguments.empty()) {
                    std::cout << "(";
                    for (size_t j = 0; j < annotation.arguments.size(); j++) {
                        if (j > 0) {
                            std::cout << ", ";
                        }
                        std::cout << annotation.arguments.at(i);
                    }
                    std::cout << ")";
                }
                std::cout << std::endl;
            }
        }
    } // namespace AST
} // namespace Debug
