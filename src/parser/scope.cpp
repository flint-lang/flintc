#include "parser/ast/scope.hpp"
#include "parser/ast/statements/catch_node.hpp"
#include "parser/ast/statements/do_while_node.hpp"
#include "parser/ast/statements/enhanced_for_loop_node.hpp"
#include "parser/ast/statements/for_loop_node.hpp"
#include "parser/ast/statements/if_node.hpp"
#include "parser/ast/statements/switch_statement.hpp"
#include "parser/ast/statements/while_node.hpp"
#include <iterator>

bool Scope::clone_variables(const std::shared_ptr<Scope> other) {
    for (const auto &[name, variable] : other->variables) {
        if (!add_variable(name, variable)) {
            // Duplicate definition / shadowing
            return false;
        }
    }
    return true;
}

bool Scope::add_variable(const std::string &var_name, const Variable &variable) {
    return variables.insert({var_name, variable}).second;
}

std::optional<std::shared_ptr<Type>> Scope::get_variable_type(const std::string &var_name) {
    if (variables.find(var_name) == variables.end()) {
        return std::nullopt;
    }
    return variables.at(var_name).type;
}

std::unordered_map<std::string, Scope::Variable> Scope::get_unique_variables(const unsigned int segment) const {
    auto unique_variables = variables;
    if (parent_scope != nullptr) {
        for (const auto &variable : parent_scope->variables) {
            unique_variables.erase(variable.first);
        }
    }
    for (auto it = unique_variables.begin(); it != unique_variables.end();) {
        if (it->second.scope_segment > segment) {
            it = unique_variables.erase(it);
        } else {
            ++it;
        }
    }
    return unique_variables;
}

void insert_variables(                                          //
    std::vector<std::pair<std::string, Scope::Variable>> &from, //
    std::vector<std::pair<std::string, Scope::Variable>> &into  //
) {
    for (const auto &into_var : into) {
        for (auto from_it = from.begin(); from_it != from.end();) {
            if (into_var == *from_it) {
                // Variable duplication, remove duplicate
                from_it = from.erase(from_it);
            } else {
                ++from_it;
            }
        }
    }
    if (from.empty()) {
        return;
    }
    into.reserve(into.size() + from.size());
    into.insert(                                                                               //
        into.end(), std::make_move_iterator(from.begin()), std::make_move_iterator(from.end()) //
    );
}

std::vector<std::pair<std::string, Scope::Variable>> Scope::get_all_variables() const {
    std::vector<std::pair<std::string, Variable>> all_variables;
    all_variables.reserve(variables.size());
    for (const auto &variable : variables) {
        all_variables.emplace_back(variable);
    }
    for (const auto &statement : body) {
        switch (statement->get_variation()) {
            default:
                break;
            case StatementNode::Variation::CATCH: {
                const auto *node = statement->as<CatchNode>();
                auto scope_variables = node->scope->get_all_variables();
                insert_variables(scope_variables, all_variables);
                break;
            }
            case StatementNode::Variation::DO_WHILE: {
                const auto *node = statement->as<DoWhileNode>();
                auto scope_variables = node->scope->get_all_variables();
                insert_variables(scope_variables, all_variables);
                break;
            }
            case StatementNode::Variation::ENHANCED_FOR_LOOP: {
                const auto *node = statement->as<EnhForLoopNode>();
                auto scope_variables = node->body->get_all_variables();
                insert_variables(scope_variables, all_variables);
                break;
            }
            case StatementNode::Variation::FOR_LOOP: {
                const auto *node = statement->as<ForLoopNode>();
                auto scope_variables = node->body->get_all_variables();
                insert_variables(scope_variables, all_variables);
                break;
            }
            case StatementNode::Variation::IF: {
                const auto *node = statement->as<IfNode>();
                auto scope_variables = node->then_scope->get_all_variables();
                insert_variables(scope_variables, all_variables);
                std::optional<std::variant<std::unique_ptr<IfNode>, std::shared_ptr<Scope>> const *> else_scope;
                if (node->else_scope.has_value()) {
                    else_scope = &node->else_scope.value();
                }
                while (else_scope.has_value()) {
                    if (std::holds_alternative<std::unique_ptr<IfNode>>(*else_scope.value())) {
                        const auto *if_node = std::get<std::unique_ptr<IfNode>>(*else_scope.value()).get();
                        scope_variables = if_node->then_scope->get_all_variables();
                        insert_variables(scope_variables, all_variables);
                        if (if_node->else_scope.has_value()) {
                            else_scope = &if_node->else_scope.value();
                        } else {
                            else_scope = std::nullopt;
                        }
                    } else {
                        const auto &last_scope = std::get<std::shared_ptr<Scope>>(*else_scope.value());
                        scope_variables = last_scope->get_all_variables();
                        insert_variables(scope_variables, all_variables);
                        else_scope = std::nullopt;
                    }
                }
                break;
            }
            case StatementNode::Variation::SWITCH: {
                const auto *node = statement->as<SwitchStatement>();
                for (const auto &branch : node->branches) {
                    auto scope_variables = branch.body->get_all_variables();
                    insert_variables(scope_variables, all_variables);
                }
                break;
            }
            case StatementNode::Variation::WHILE: {
                const auto *node = statement->as<WhileNode>();
                auto scope_variables = node->scope->get_all_variables();
                insert_variables(scope_variables, all_variables);
                break;
            }
        }
    }
    return all_variables;
}
