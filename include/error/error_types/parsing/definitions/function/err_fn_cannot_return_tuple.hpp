#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/type/tuple_type.hpp"
#include "types.hpp"

class ErrFnCannotReturnTuple : public BaseError {
  public:
    ErrFnCannotReturnTuple(                      //
        const ErrorType error_type,              //
        const std::string &file,                 //
        const token_slice &tokens,               //
        const std::shared_ptr<Type> &return_type //
        ) :
        BaseError(error_type, file, tokens.first->line, tokens.first->column, return_type->to_string().size()),
        return_type(return_type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        oss << BaseError::to_string() << "├─ Functions cannot return a tuple type directly.\n";
        oss << "└─ If you want to return multiple values, change the return type to '" << CYAN << "(";
        const TupleType *tuple_type = dynamic_cast<const TupleType *>(return_type.get());
        assert(tuple_type != nullptr);
        for (auto it = tuple_type->types.begin(); it != tuple_type->types.end(); ++it) {
            if (it != tuple_type->types.begin()) {
                oss << ", ";
            }
            oss << (*it)->to_string();
        }

        oss << ")" << DEFAULT << "'";
        return oss.str();
    }

  private:
    std::shared_ptr<Type> return_type;
};
