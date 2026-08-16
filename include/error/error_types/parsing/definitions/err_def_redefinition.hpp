#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/definition_node.hpp"

class ErrDefRedefinition : public BaseError {
  public:
    ErrDefRedefinition(                 //
        const ErrorType error_type,     //
        const Hash &file_file,          //
        const unsigned int line,        //
        const unsigned int column,      //
        const DefinitionNode *original, //
        const std::string &name         //
        ) :
        BaseError(error_type, file_file, line, column, name.size()),
        original(original),
        name(name) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Redefinition of type: " << YELLOW << name << DEFAULT << "\n";
        oss << "└─ First defined at: " << GREEN << cwd_relative(original->file_hash, original->line, original->column) << DEFAULT;
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Redefinition of type '" + name + "'";
        return d;
    }

  private:
    const DefinitionNode *original;
    const std::string &name;
};
