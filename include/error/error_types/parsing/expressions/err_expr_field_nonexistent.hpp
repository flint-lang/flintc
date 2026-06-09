#pragma once

#include "error/error_types/base_error.hpp"
#include "parser/ast/definitions/data_node.hpp"
#include "parser/type/data_type.hpp"
#include "parser/type/type.hpp"

class ErrExprFieldNonexistent : public BaseError {
  public:
    ErrExprFieldNonexistent(                                                                    //
        const ErrorType error_type,                                                             //
        const Hash &file_hash,                                                                  //
        const token_slice &tokens,                                                              //
        const std::string &field,                                                               //
        const std::shared_ptr<Type> &type,                                                      //
        const std::optional<std::vector<std::pair<std::string, std::shared_ptr<Type>>>> &fields //
        ) :
        BaseError(error_type, file_hash, tokens),
        field(field),
        type(type) {
        if (fields.has_value()) {
            this->fields = fields.value();
        } else {
            ASSERT(type->get_variation() == Type::Variation::DATA);
            const DataNode *node = type->as<DataType>()->data_node;
            for (const auto &f : node->fields) {
                this->fields.emplace_back(f.name, f.type);
            }
        }
    }

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Field '" + field + "' does not exist on type '" + type->to_string() + "'\n";
        oss << "└─ Available fields are:\n";
        size_t i = 0;
        for (const auto &[field_name, field_type] : fields) {
            if (field_name.empty()) {
                oss << "    │\n";
                i++;
                continue;
            }
            if (i + 1 < fields.size()) {
                oss << "    ├─ " << field_type->to_string() << " " << field_name << "\n";
            } else {
                oss << "    └─ " << field_type->to_string() << " " << field_name;
            }
            i++;
        }
        return oss.str();
    }

    [[nodiscard]]
    Diagnostic to_diagnostic() const override {
        Diagnostic d = BaseError::to_diagnostic();
        d.message = "Field '" + field + "' does not exist on type '" + type->to_string() + "'";
        return d;
    }

  private:
    std::string field;
    std::shared_ptr<Type> type;
    std::vector<std::pair<std::string, std::shared_ptr<Type>>> fields;
};
