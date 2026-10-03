#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/definition_node.hpp"
#include "types.hpp"

class ErrFnSpecializationFailed : public BaseError {
  public:
    ErrFnSpecializationFailed(                                    //
        const ErrorType error_type,                               //
        const Hash &file_hash,                                    //
        const PosTriple &pos,                                     //
        const std::vector<DefinitionNode::ComptimeParameter> &cpl //
        ) :
        BaseError(error_type, file_hash, pos.line, pos.column, pos.length),
        cpl(cpl) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Specialization of function failed\n";
        oss << "└─ Passed-in comptime values are:\n";
        for (size_t i = 0; i < cpl.size(); i++) {
            if (i + 1 == cpl.size()) {
                oss << "    └─ ";
            } else {
                oss << "    ├─ ";
            }
            const auto &param = cpl.at(i);
            oss << "'" << YELLOW << param.type->to_string() << " " << param.name << DEFAULT << "'";
            if (param.applied_value.has_value()) {
                oss << " resolved to '" << BLUE << param.applied_value.value()->to_string() << DEFAULT << "'";
            } else {
                oss << " was unresolved";
            }
            if (i + 1 < cpl.size()) {
                oss << "\n";
            }
        }
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Specialization of function failed";
        return d;
    }

  private:
    std::vector<DefinitionNode::ComptimeParameter> cpl;
};
