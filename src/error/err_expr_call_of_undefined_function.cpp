#include "error/error_types/parsing/expressions/err_expr_call_of_undefined_function.hpp"
#include "colors.hpp"
#include "parser/parser.hpp"

#include <set>

std::string ErrExprCallOfUndefinedFunction::to_string() const {
    std::ostringstream oss;
    oss << BaseError::to_string();
    // Check if there exists any function with that name and that argument's
    {
        std::lock_guard<std::mutex> lock_guard((Parser::parsed_functions_mutex));
        const auto &parsed_functions = Parser::parsed_functions;
        std::vector<std::pair<const FunctionNode *, std::string>> possible_functions;
        for (const auto &[parsed_function, file] : parsed_functions) {
            if (parsed_function->name == function_name && parsed_function->parameters.size() == arg_types.size()) {
                possible_functions.emplace_back(parsed_function, file);
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
                oss << CYAN << function_name << "(";
                for (auto arg_it = fn_it->first->parameters.begin(); arg_it != fn_it->first->parameters.end(); ++arg_it) {
                    if (arg_it != fn_it->first->parameters.begin()) {
                        oss << ", ";
                    }
                    if (std::get<2>(*arg_it)) {
                        oss << "mut ";
                    }
                    oss << std::get<0>(*arg_it)->to_string() << " ";
                    oss << std::get<1>(*arg_it);
                }
                oss << ")" << DEFAULT << " from file '" << YELLOW << fn_it->second << DEFAULT << "'";
                if ((fn_it + 1) != possible_functions.end()) {
                    oss << "\n";
                }
            }
            return oss.str();
        }
    }
    // Check if the function is part of any Core module, if it is we can suggest using the core module's functions
    std::vector<std::pair<std::vector<std::shared_ptr<Type>>, std::string>> found_functions;
    for (const auto &[module_name, function_list] : core_module_functions) {
        for (const auto &[fn_name, overloads] : function_list) {
            if (fn_name == function_name) {
                for (const auto &overload : overloads) {
                    std::vector<std::shared_ptr<Type>> fn_args;
                    for (const auto &arg_type_name : std::get<0>(overload)) {
                        fn_args.emplace_back(Type::get_primitive_type(std::string(arg_type_name)));
                    }
                    found_functions.emplace_back(fn_args, module_name);
                }
            }
        }
    }
    if (!found_functions.empty()) {
        const std::optional<const Parser *> parser = Parser::get_instance_from_filename(file_name);
        assert(parser.has_value());
        const auto &imported_core_modules = parser.value()->file_node_ptr->imported_core_modules;
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
            oss << CYAN << function_name << "(";
            for (auto arg_it = fn_it->first.begin(); arg_it != fn_it->first.end(); ++arg_it) {
                if (arg_it != fn_it->first.begin()) {
                    oss << ", ";
                }
                oss << (*arg_it)->to_string();
            }
            oss << ")" << DEFAULT << " from Core." << YELLOW << fn_it->second << DEFAULT << "\n";
            module_names.emplace(fn_it->second);
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
