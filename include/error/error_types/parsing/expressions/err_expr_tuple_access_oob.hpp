#pragma once

#include "colors.hpp"
#include "error/error_types/base_error.hpp"
#include "parser/type/tuple_type.hpp"

class ErrExprTupleAccessOOB : public BaseError {
  public:
    ErrExprTupleAccessOOB(                      //
        const ErrorType error_type,             //
        const std::string &file,                //
        int line,                               //
        int column,                             //
        const std::string &tuple_access,        //
        const std::shared_ptr<Type> &tuple_type //
        ) :
        BaseError(error_type, file, line, column, tuple_access.size()),
        tuple_access(tuple_access),
        tuple_type(tuple_type) {}

    [[nodiscard]]
    std::string to_string() const override {
        std::ostringstream oss;
        const TupleType *tuple_type_ptr = dynamic_cast<const TupleType *>(tuple_type.get());
        oss << BaseError::to_string() << "├─ Out of bounds access on tuple type '" << YELLOW << tuple_type->to_string() << DEFAULT << "'\n";
        oss << "└─ The tuples last element is '" << CYAN << "$" << std::to_string(tuple_type_ptr->types.size() - 1) << DEFAULT << "'";
        return oss.str();
    }

  private:
    std::string tuple_access;
    std::shared_ptr<Type> tuple_type;
};
