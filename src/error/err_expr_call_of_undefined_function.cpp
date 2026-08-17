#include "error/error_types/parsing/expressions/err_expr_call_of_undefined_function.hpp"
#include "colors.hpp"
#include "parser/parser.hpp"

#include <set>

std::string ErrExprCallOfUndefinedFunction::to_string() const {
    std::ostringstream oss;
    oss << BaseError::to_string();
    // Check if there exists any function with that name and that argument's
    {
        Namespace *file_namespace = Resolver::get_namespace_from_hash(hash);
        std::vector<const FunctionNode *> possible_functions;
        for (const auto &definition : file_namespace->public_symbols.definitions) {
            if (definition->get_variation() != DefinitionNode::Variation::FUNCTION) {
                continue;
            }
            const FunctionNode *function_node = definition->as<FunctionNode>();
            if (function_node->name == function_name && function_node->parameters.size() == arg_types.size()) {
                possible_functions.push_back(function_node);
            }
        }
        if (!possible_functions.empty()) {
            oss << "├─ Call of undefined function '" << YELLOW << function_name << "(";
            for (auto arg_it = arg_types.begin(); arg_it != arg_types.end(); ++arg_it) {
                if (arg_it != arg_types.begin()) {
                    oss << ", ";
                }
                oss << (*arg_it)->to_string();
            }
            oss << ")" << DEFAULT << "'\n";
            oss << "└─ Possible functions you could mean:\n";
            for (auto fn_it = possible_functions.begin(); fn_it != possible_functions.end(); ++fn_it) {
                if ((fn_it + 1) != possible_functions.end()) {
                    oss << "    ├─ ";
                } else {
                    oss << "    └─ ";
                }
                oss << CYAN << (*fn_it)->get_signature_string(0, false, true, true, false, false) << DEFAULT;
                oss << " from file '" << YELLOW << (*fn_it)->file_hash.path.filename().string() << DEFAULT << "'";
                if ((fn_it + 1) != possible_functions.end()) {
                    oss << "\n";
                }
            }
            return oss.str();
        }
    }
    // Check if the function is part of any Core module, if it is we can suggest using the core module's functions
    const std::optional<const Parser *> parser = Parser::get_instance_from_hash(hash);
    ASSERT(parser.has_value());
    const auto &imported_core_modules = parser.value()->file_node_ptr->imported_core_modules;
    struct FoundFunction {
        std::vector<std::shared_ptr<Type>> args;
        std::string module_name;
        std::optional<std::string> alias;
    };
    std::vector<FoundFunction> found_functions;
    for (const auto &[module_name, function_list] : core_module_functions) {
        for (const auto &[fn_name, overloads] : function_list) {
            if (fn_name != function_name) {
                continue;
            }
            for (const auto &overload : overloads) {
                std::vector<std::shared_ptr<Type>> fn_args;
                for (const auto &[arg_type, arg_name] : std::get<0>(overload)) {
                    fn_args.emplace_back(Type::get_primitive_type(std::string(arg_type)));
                }
                const std::string mod_name(module_name);
                found_functions.push_back(FoundFunction{
                    .args = fn_args,
                    .module_name = mod_name,
                    .alias = imported_core_modules.at(mod_name)->alias,
                });
            }
        }
    }
    if (!found_functions.empty()) {
        oss << "├─ Call of undefined function '" << YELLOW << function_name << "(";
        for (auto arg_it = arg_types.begin(); arg_it != arg_types.end(); ++arg_it) {
            if (arg_it != arg_types.begin()) {
                oss << ", ";
            }
            oss << (*arg_it)->to_string();
        }
        oss << ")" << DEFAULT << "'\n";
        oss << "├─ Possible functions you could mean:\n";
        std::set<std::string> module_names;
        for (auto fn_it = found_functions.begin(); fn_it != found_functions.end(); ++fn_it) {
            if ((fn_it + 1) != found_functions.end()) {
                oss << "│   ├─ ";
            } else {
                oss << "│   └─ ";
            }
            oss << CYAN;
            if (fn_it->alias.has_value()) {
                oss << fn_it->alias.value() << ".";
            }
            oss << function_name << "(";
            for (auto arg_it = fn_it->args.begin(); arg_it != fn_it->args.end(); ++arg_it) {
                if (arg_it != fn_it->args.begin()) {
                    oss << ", ";
                }
                oss << (*arg_it)->to_string();
            }
            oss << ")" << DEFAULT << " from Core." << YELLOW << fn_it->module_name << DEFAULT << "\n";
            module_names.emplace(fn_it->module_name);
        }
        unsigned int module_count = 0;
        for (const auto &module_name : module_names) {
            if (imported_core_modules.find(module_name) != imported_core_modules.end()) {
                module_count++;
            }
        }
        if (module_count != module_names.size()) {
            // Suggest the import
            oss << "└─ Add the line '" << CYAN << "use Core." << YELLOW << "xxx" << DEFAULT << "' somewhere in your file";
        } else {
            // Suggest calling one of the shown functions instead
            oss << "└─ You need to cast the argument" << ((arg_types.size() > 1) ? "s" : "") << " to one of the supported types";
        }
        return oss.str();
    }
    oss << "└─ Call of undefined function '" << YELLOW << function_name << "(";
    for (auto arg_it = arg_types.begin(); arg_it != arg_types.end(); ++arg_it) {
        if (arg_it != arg_types.begin()) {
            oss << ", ";
        }
        oss << (*arg_it)->to_string();
    }
    oss << ")" << DEFAULT << "'";
    return oss.str();
}
