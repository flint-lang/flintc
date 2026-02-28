#pragma once

#include "resolver/resolver.hpp"
#include "types.hpp"

#include "parser/ast/definitions/data_node.hpp"
#include "parser/ast/definitions/entity_node.hpp"
#include "parser/ast/definitions/enum_node.hpp"
#include "parser/ast/definitions/func_node.hpp"
#include "parser/ast/definitions/function_node.hpp"
#include "parser/ast/definitions/import_node.hpp"
#include "parser/ast/definitions/link_node.hpp"
#include "parser/ast/definitions/test_node.hpp"
#include "parser/ast/definitions/variant_node.hpp"

#include "parser/ast/expressions/array_access_node.hpp"
#include "parser/ast/expressions/array_initializer_node.hpp"
#include "parser/ast/expressions/binary_op_node.hpp"
#include "parser/ast/expressions/data_access_node.hpp"
#include "parser/ast/expressions/default_node.hpp"
#include "parser/ast/expressions/expression_node.hpp"
#include "parser/ast/expressions/group_expression_node.hpp"
#include "parser/ast/expressions/grouped_data_access_node.hpp"
#include "parser/ast/expressions/initializer_node.hpp"
#include "parser/ast/expressions/literal_node.hpp"
#include "parser/ast/expressions/optional_chain_node.hpp"
#include "parser/ast/expressions/optional_unwrap_node.hpp"
#include "parser/ast/expressions/range_expression_node.hpp"
#include "parser/ast/expressions/string_interpolation_node.hpp"
#include "parser/ast/expressions/switch_expression.hpp"
#include "parser/ast/expressions/switch_match_node.hpp"
#include "parser/ast/expressions/type_cast_node.hpp"
#include "parser/ast/expressions/type_node.hpp"
#include "parser/ast/expressions/variable_node.hpp"
#include "parser/ast/expressions/variant_extraction_node.hpp"
#include "parser/ast/expressions/variant_unwrap_node.hpp"

#include "parser/ast/statements/array_assignment_node.hpp"
#include "parser/ast/statements/assignment_node.hpp"
#include "parser/ast/statements/catch_node.hpp"
#include "parser/ast/statements/data_field_assignment_node.hpp"
#include "parser/ast/statements/declaration_node.hpp"
#include "parser/ast/statements/do_while_node.hpp"
#include "parser/ast/statements/enhanced_for_loop_node.hpp"
#include "parser/ast/statements/for_loop_node.hpp"
#include "parser/ast/statements/group_assignment_node.hpp"
#include "parser/ast/statements/group_declaration_node.hpp"
#include "parser/ast/statements/grouped_data_field_assignment_node.hpp"
#include "parser/ast/statements/if_node.hpp"
#include "parser/ast/statements/return_node.hpp"
#include "parser/ast/statements/stacked_array_assignment.hpp"
#include "parser/ast/statements/stacked_assignment.hpp"
#include "parser/ast/statements/stacked_grouped_assignment.hpp"
#include "parser/ast/statements/statement_node.hpp"
#include "parser/ast/statements/switch_statement.hpp"
#include "parser/ast/statements/throw_node.hpp"
#include "parser/ast/statements/while_node.hpp"

#include "parser/ast/call_node_base.hpp"
#include "parser/ast/file_node.hpp"
#include "parser/ast/instance_call_node_base.hpp"
#include "parser/ast/unary_op_base.hpp"

#include <string>
#include <unordered_map>
#include <vector>

#ifdef __WIN32__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef ERROR
#undef OPTIONAL
#undef OPAQUE
#endif

namespace Debug {
    enum TreeType { NONE = 0, VERT = 1, BRANCH = 2, SINGLE = 3 };
    static const std::unordered_map<TreeType, std::string> tree_blocks = {
        {NONE, "    "},             // '    '
        {VERT, " \u2502  "},        // ' │  '
        {BRANCH, " \u251C\u2500 "}, // ' ├─ '
        {SINGLE, " \u2514\u2500 "}, // ' └─ '
    };
    static const std::unordered_map<TreeType, std::string> tree_characters = {
        {VERT, "\u2502"},   // '│'
        {BRANCH, "\u251C"}, // '├'
        {SINGLE, "\u2514"}, // '└'
    };
    static const std::string HOR = "\u2500";

    namespace TextFormat {
        const inline std::string UNDERLINE_START = "\033[4m";
        const inline std::string UNDERLINE_END = "\033[24m";
        const inline std::string BOLD_START = "\033[1m";
        const inline std::string BOLD_END = "\033[22m";
        const inline std::string RESET = "\033[0m";

        // Colored underlines (SGR 58 for underline color)
        const inline std::string RED_UNDERLINE = "\033[4;58;5;196m";
        const inline std::string GREEN_UNDERLINE = "\033[4;58;5;46m";
        const inline std::string BLUE_UNDERLINE = "\033[4;58;5;33m";
        const inline std::string YELLOW_UNDERLINE = "\033[4;58;5;226m";
        const inline std::string CYAN_UNDERLINE = "\033[4;58;5;51m";
        const inline std::string MAGENTA_UNDERLINE = "\033[4;58;5;201m";
        const inline std::string WHITE_UNDERLINE = "\033[4;58;5;255m";
        const inline std::string RESET_UNDERLINE = "\033[59m";
    } // namespace TextFormat

    std::string get_string_container(unsigned int size, const std::string &value);
    void print_in_container(unsigned int size, const std::string &str);
    std::string fill_container_with(const unsigned int &size, const char &character);
    void print_token_context_vector(const token_slice &tokens, const std::string &file_name);
    std::string create_n_str(unsigned int n, const std::string &str);

    namespace Dep {
        void print_dep_tree(unsigned int indent_lvl, const std::variant<std::shared_ptr<DepNode>, std::weak_ptr<DepNode>> &dep_node);
    } // namespace Dep

    namespace AST {
        struct TreeBits {
            // Four bit arrays, one for each branch type
            std::array<uint32_t, 4> bits = {0}; // NONE, VERT, BRANCH, SINGLE

            // Set a specific branch type at a specific level
            void set(TreeType type, unsigned int level) {
                bits[type] |= (1u << level);
            }

            // Check if a specific branch type is set at a specific level
            bool is(TreeType type, unsigned int level) const {
                return (bits[type] & (1u << level)) != 0;
            }

            // Get the branch type at a specific level
            TreeType get(unsigned int level) const {
                for (int type = 0; type < 4; type++) {
                    if (bits[type] & (1u << level)) {
                        return static_cast<TreeType>(type);
                    }
                }
                return NONE; // Default
            }

            // Create a modified copy for a child node
            TreeBits child(unsigned int parent_level, bool is_last) const {
                TreeBits result = *this;
                result.set(is_last ? SINGLE : BRANCH, parent_level);
                return result;
            }
        };

        namespace Local {
            void print_tree_line(TreeBits &bits, unsigned int max_level);
            void print_header(unsigned int indent_lvl, TreeBits &bits, const std::string &header);
        } // namespace Local

        void print_all_files();
        void print_file(const FileNode &file);

        // --- EXPRESSIONS ---
        void print_variable(unsigned int indent_lvl, TreeBits &bits, const VariableNode &var);
        void print_unary_op(unsigned int indent_lvl, TreeBits &bits, const UnaryOpBase &unary);
        void print_literal(unsigned int indent_lvl, TreeBits &bits, const LiteralNode &lit);
        void print_string_interpolation(unsigned int indent_lvl, TreeBits &bits, const StringInterpolationNode &interpol);
        void print_call(unsigned int indent_lvl, TreeBits &bits, const CallNodeBase &call);
        void print_instance_call(unsigned int indent_lvl, TreeBits &bits, const InstanceCallNodeBase &call);
        void print_binary_op(unsigned int indent_lvl, TreeBits &bits, const BinaryOpNode &bin);
        void print_type_cast(unsigned int indent_lvl, TreeBits &bits, const TypeCastNode &cast);
        void print_type_node(unsigned int indent_lvl, TreeBits &bits, const TypeNode &type);
        void print_initializer(unsigned int indent_lvl, TreeBits &bits, const InitializerNode &initializer);
        void print_group_expression(unsigned int indent_lvl, TreeBits &bits, const GroupExpressionNode &group);
        void print_range_expression(unsigned int indent_lvl, TreeBits &bits, const RangeExpressionNode &range);
        void print_array_initializer(unsigned int indent_lvl, TreeBits &bits, const ArrayInitializerNode &init);
        void print_array_access(unsigned int indent_lvl, TreeBits &bits, const ArrayAccessNode &access);
        void print_data_access(unsigned int indent_lvl, TreeBits &bits, const DataAccessNode &access);
        void print_grouped_data_access(unsigned int indent_lvl, TreeBits &bits, const GroupedDataAccessNode &access);
        void print_switch_match(unsigned int indent_lvl, TreeBits &bits, const SwitchMatchNode &match);
        void print_switch_expression(unsigned int indent_lvl, TreeBits &bits, const SwitchExpression &switch_expression);
        void print_default(unsigned int indent_lvl, TreeBits &bits, const DefaultNode &default_node);
        void print_optional_chain(unsigned int indent_lvl, TreeBits &bits, const OptionalChainNode &chain_node);
        void print_optional_unwrap(unsigned int indent_lvl, TreeBits &bits, const OptionalUnwrapNode &unwrap_node);
        void print_variant_extraction(unsigned int indent_lvl, TreeBits &bits, const VariantExtractionNode &extraction);
        void print_variant_unwrap(unsigned int indent_lvl, TreeBits &bits, const VariantUnwrapNode &unwrap_node);
        void print_expression(unsigned int indent_lvl, TreeBits &bits, const std::unique_ptr<ExpressionNode> &expr);

        // --- STATEMENTS ---
        void print_throw(unsigned int indent_lvl, TreeBits &bits, const ReturnNode &return_node);
        void print_return(unsigned int indent_lvl, TreeBits &bits, const ThrowNode &return_node);
        void print_if(unsigned int indent_lvl, TreeBits &bits, const IfNode &if_node);
        void print_do_while(unsigned int indent_lvl, TreeBits &bits, const DoWhileNode &do_while_node);
        void print_while(unsigned int indent_lvl, TreeBits &bits, const WhileNode &while_node);
        void print_for(unsigned int indent_lvl, TreeBits &bits, const ForLoopNode &for_node);
        void print_enh_for(unsigned int indent_lvl, TreeBits &bits, const EnhForLoopNode &for_node);
        void print_switch_statement(unsigned int indent_lvl, TreeBits &bits, const SwitchStatement &switch_statement);
        void print_catch(unsigned int indent_lvl, TreeBits &bits, const CatchNode &catch_node);
        void print_group_assignment(unsigned int indent_lvl, TreeBits &bits, const GroupAssignmentNode &assign);
        void print_assignment(unsigned int indent_lvl, TreeBits &bits, const AssignmentNode &assign);
        void print_array_assignment(unsigned int indent_lvl, TreeBits &bits, const ArrayAssignmentNode &assign);
        void print_group_declaration(unsigned int indent_lvl, TreeBits &bits, const GroupDeclarationNode &decl);
        void print_declaration(unsigned int indent_lvl, TreeBits &bits, const DeclarationNode &decl);
        void print_data_field_assignment(unsigned int indent_lvl, TreeBits &bits, const DataFieldAssignmentNode &assignment);
        void print_grouped_data_field_assignment(                                                     //
            unsigned int indent_lvl, TreeBits &bits, const GroupedDataFieldAssignmentNode &assignment //
        );
        void print_stacked_assignment(unsigned int indent_lvl, TreeBits &bits, const StackedAssignmentNode &assignment);
        void print_stacked_array_assignment(unsigned int indent_lvl, TreeBits &bits, const StackedArrayAssignmentNode &assignment);
        void print_stacked_grouped_assignment(unsigned int indent_lvl, TreeBits &bits, const StackedGroupedAssignmentNode &assignment);
        void print_statement(unsigned int indent_lvl, TreeBits &bits, const std::unique_ptr<StatementNode> &statement);
        void print_body(unsigned int indent_lvl, TreeBits &bits, const std::vector<std::unique_ptr<StatementNode>> &body);

        // --- DEFINITIONS ---
        void print_data(unsigned int indent_lvl, TreeBits &bits, const DataNode &data);
        void print_entity(unsigned int indent_lvl, TreeBits &bits, const EntityNode &entity);
        void print_enum(unsigned int indent_lvl, TreeBits &bits, const EnumNode &enum_node);
        void print_error(unsigned int indent_lvl, TreeBits &bits, const ErrorNode &error);
        void print_func(unsigned int indent_lvl, TreeBits &bits, const FuncNode &func);
        void print_function(unsigned int indent_lvl, TreeBits &bits, const FunctionNode &function);
        void print_import(unsigned int indent_lvl, TreeBits &bits, const ImportNode &import);
        void print_link(unsigned int indent_lvl, TreeBits &bits, const LinkNode &link);
        void print_variant(unsigned int indent_lvl, TreeBits &bits, const VariantNode &variant);
        void print_test(unsigned int indent_lvl, TreeBits &bits, const TestNode &test);
        void print_annotations(unsigned int indent_lvl, TreeBits &bits, const std::vector<AnnotationNode> &annotations);
    } // namespace AST
} // namespace Debug
