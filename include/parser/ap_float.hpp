#pragma once

#include "ap_int.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <string>
#include <vector>

/// @class `APFloat`
/// @brief Class which represents arbitrary precision floating point numbers
class APFloat {
  public:
    /// @function `APFloat`
    /// @brief Constructs an APFloat from a string representation
    ///
    /// @param `str` The string to parse
    explicit APFloat(const std::string str) {
        assert(!str.empty());
        std::string str_copy = str;

        if (!str_copy.empty() && str_copy[0] == '-') {
            is_negative = true;
            str_copy = str_copy.substr(1);
        } else {
            is_negative = false;
        }

        size_t dot_pos = str_copy.find('.');
        if (dot_pos == std::string::npos) {
            for (char c : str_copy) {
                if (c >= '0' && c <= '9') {
                    digits.push_back(c - '0');
                }
            }
            decimal_pos = static_cast<int>(digits.size()) - 1;
        } else {
            for (size_t i = 0; i < str_copy.size(); ++i) {
                if (i != dot_pos) {
                    char c = str_copy[i];
                    if (c >= '0' && c <= '9') {
                        digits.push_back(c - '0');
                    }
                }
            }
            decimal_pos = static_cast<int>(dot_pos) - 1;
        }

        while (digits.size() > 1 && digits[0] == 0) {
            digits.erase(digits.begin());
            decimal_pos--;
        }

        if (digits.empty() || (digits.size() == 1 && digits[0] == 0)) {
            digits = {0};
            decimal_pos = 0;
            is_negative = false;
        }
    }

    /// @function `APFloat`
    /// @brief Constructs an APFloat from an APInt
    ///
    /// @param `val` The APInt to convert
    explicit APFloat(const APInt &val) {
        is_negative = val.is_negative;
        digits = val.digits;

        while (digits.size() > 1 && digits[0] == 0) {
            digits.erase(digits.begin());
        }

        if (digits.empty() || (digits.size() == 1 && digits[0] == 0)) {
            digits = {0};
            decimal_pos = 0;
            is_negative = false;
        } else {
            decimal_pos = static_cast<int>(digits.size()) - 1;
        }
    }

    /// @function `to_string`
    /// @brief Converts the arbitrary float to a string
    ///
    /// @return `std::string` The converted string
    std::string to_string() const {
        if (digits.empty() || (digits.size() == 1 && digits[0] == 0)) {
            return "0.0";
        }

        std::string result;
        if (is_negative) {
            result += '-';
        }
        int total_digits = static_cast<int>(digits.size());
        int int_digits_count = decimal_pos + 1;
        if (int_digits_count <= 0) {
            result += "0.";
            for (int i = 0; i < -int_digits_count; ++i) {
                result += '0';
            }
            for (uint8_t d : digits) {
                result += static_cast<char>('0' + d);
            }
        } else if (int_digits_count >= total_digits) {
            for (uint8_t d : digits) {
                result += static_cast<char>('0' + d);
            }
            for (int i = 0; i < int_digits_count - total_digits; ++i) {
                result += '0';
            }
            result += ".0";
        } else {
            for (int i = 0; i < int_digits_count; ++i) {
                result += static_cast<char>('0' + digits[i]);
            }
            result += '.';
            for (int i = int_digits_count; i < total_digits; ++i) {
                result += static_cast<char>('0' + digits[i]);
            }
        }
        return result;
    }

    /// @function `to_fN`
    /// @brief Converts the arbitrary float to a discrete floating point number of type T
    ///
    /// @tparam `T` The floating point type to convert this APFloat to, like `float`, `double`, `long double` etc.
    /// @return `T` The result value truncated to the target precision
    template <typename T> T to_fN() const {
        return static_cast<T>(std::stod(to_string()));
    }

    /// @function `to_apint`
    /// @brief Converts the arbitrary float to an arbitrary integer
    ///
    /// @return `APInt` The converted APInt
    APInt to_apint() const {
        APInt result("0");
        result.is_negative = is_negative;
        int int_digits_count = decimal_pos + 1;
        if (int_digits_count <= 0) {
            result.digits = {0};
            result.is_negative = false;
        } else {
            int actual_count = std::min(int_digits_count, static_cast<int>(digits.size()));
            result.digits.assign(digits.begin(), digits.begin() + actual_count);
            for (int i = actual_count; i < int_digits_count; ++i) {
                result.digits.push_back(0);
            }
            if (result.digits.empty() || (result.digits.size() == 1 && result.digits[0] == 0)) {
                result.digits = {0};
                result.is_negative = false;
            }
        }
        return result;
    }

    /// @function `operator+=`
    /// @brief Adds another APFloat to this one in place
    ///
    /// @param `other` The APFloat to add
    void operator+=(const APFloat &other) {
        *this = *this + other;
    }

    /// @function `operator+=`
    /// @brief Adds another APInt to this APFloat in place
    ///
    /// @param `other` The APInt to add
    void operator+=(const APInt &other) {
        *this = *this + APFloat(other);
    }

    /// @function `operator+`
    /// @brief Adds an APInt to this APFloat
    ///
    /// @param `other` The APInt to add
    /// @return `APFloat` The result of the addition
    APFloat operator+(const APInt &other) const {
        return *this + APFloat(other);
    }

    /// @function `operator+`
    /// @brief Adds two APFloats together
    ///
    /// @param `other` The APFloat to add
    /// @return `APFloat` The result of the addition
    APFloat operator+(const APFloat &other) const {
        if (digits.size() == 1 && digits[0] == 0) {
            return other;
        }
        if (other.digits.size() == 1 && other.digits[0] == 0) {
            return *this;
        }

        if (is_negative && !other.is_negative) {
            APFloat temp = *this;
            temp.is_negative = false;
            return other - temp;
        }
        if (!is_negative && other.is_negative) {
            APFloat temp = other;
            temp.is_negative = false;
            return *this - temp;
        }

        APFloat result("0");
        result.is_negative = is_negative;
        int max_decimal = std::max(decimal_pos, other.decimal_pos);
        int this_shift = max_decimal - decimal_pos;
        int other_shift = max_decimal - other.decimal_pos;
        std::vector<uint8_t> a = digits;
        std::vector<uint8_t> b = other.digits;
        for (int i = 0; i < this_shift; ++i) {
            a.insert(a.begin(), 0);
        }
        for (int i = 0; i < other_shift; ++i) {
            b.insert(b.begin(), 0);
        }

        while (a.size() < b.size()) {
            a.push_back(0);
        }
        while (b.size() < a.size()) {
            b.push_back(0);
        }

        std::vector<uint8_t> sum;
        int carry = 0;
        for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i) {
            int s = a[i] + b[i] + carry;
            sum.insert(sum.begin(), s % 10);
            carry = s / 10;
        }
        if (carry > 0) {
            sum.insert(sum.begin(), carry);
            result.decimal_pos = max_decimal + 1;
        } else {
            result.decimal_pos = max_decimal;
        }
        result.digits = sum;
        result.normalize();
        return result;
    }

    /// @function `operator-=`
    /// @brief Subtracts another APFloat from this one in place
    ///
    /// @param `other` The APFloat to subtract
    void operator-=(const APFloat &other) {
        *this = *this - other;
    }

    /// @function `operator-=`
    /// @brief Subtracts another APInt from this APFloat in place
    ///
    /// @param `other` The APInt to subtract
    void operator-=(const APInt &other) {
        *this = *this - APFloat(other);
    }

    /// @function `operator-`
    /// @brief Subtracts an APInt from this APFloat
    ///
    /// @param `other` The APInt to subtract
    /// @return `APFloat` The result of the subtraction
    APFloat operator-(const APInt &other) const {
        return *this - APFloat(other);
    }

    /// @function `operator-`
    /// @brief Subtracts two APFloats
    ///
    /// @param `other` The APFloat to subtract
    /// @return `APFloat` The result of the subtraction
    APFloat operator-(const APFloat &other) const {
        if (other.digits.size() == 1 && other.digits[0] == 0) {
            return *this;
        }

        if (is_negative && !other.is_negative) {
            APFloat temp = other;
            temp.is_negative = true;
            return *this + temp;
        }
        if (!is_negative && other.is_negative) {
            APFloat temp = other;
            temp.is_negative = false;
            return *this + temp;
        }

        bool this_bigger = is_larger_than(other);
        if (!this_bigger && *this != other) {
            APFloat result = other - *this;
            result.is_negative = !result.is_negative;
            return result;
        }

        APFloat result("0");
        result.is_negative = is_negative;
        int max_decimal = std::max(decimal_pos, other.decimal_pos);
        int this_shift = max_decimal - decimal_pos;
        int other_shift = max_decimal - other.decimal_pos;
        std::vector<uint8_t> a = digits;
        std::vector<uint8_t> b = other.digits;
        for (int i = 0; i < this_shift; ++i) {
            a.insert(a.begin(), 0);
        }
        for (int i = 0; i < other_shift; ++i) {
            b.insert(b.begin(), 0);
        }

        while (a.size() < b.size()) {
            a.push_back(0);
        }
        while (b.size() < a.size()) {
            b.push_back(0);
        }

        std::vector<uint8_t> diff;
        int borrow = 0;
        for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i) {
            int d = a[i] - b[i] - borrow;
            if (d < 0) {
                d += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }
            diff.insert(diff.begin(), d);
        }
        result.digits = diff;
        result.decimal_pos = max_decimal;
        result.normalize();
        return result;
    }

    /// @function `operator*=`
    /// @brief Multiplies this APFloat by another in place
    ///
    /// @param `other` The APFloat to multiply by
    void operator*=(const APFloat &other) {
        *this = *this * other;
    }

    /// @function `operator*=`
    /// @brief Multiplies this APFloat by an APInt in place
    ///
    /// @param `other` The APInt to multiply by
    void operator*=(const APInt &other) {
        *this = *this * APFloat(other);
    }

    /// @function `operator*`
    /// @brief Multiplies this APFloat by an APInt
    ///
    /// @param `other` The APInt to multiply by
    /// @return `APFloat` The result of the multiplication
    APFloat operator*(const APInt &other) const {
        return *this * APFloat(other);
    }

    /// @function `operator*`
    /// @brief Multiplies two APFloats together
    ///
    /// @param `other` The APFloat to multiply by
    /// @return `APFloat` The result of the multiplication
    APFloat operator*(const APFloat &other) const {
        APFloat result("0");
        result.is_negative = is_negative != other.is_negative;
        std::vector<uint8_t> product(digits.size() + other.digits.size(), 0);
        for (size_t i = 0; i < digits.size(); ++i) {
            int carry = 0;
            for (size_t j = 0; j < other.digits.size(); ++j) {
                int prod = digits[digits.size() - 1 - i] * other.digits[other.digits.size() - 1 - j] +
                    product[product.size() - 1 - (i + j)] + carry;
                product[product.size() - 1 - (i + j)] = prod % 10;
                carry = prod / 10;
            }
            product[product.size() - 1 - (i + other.digits.size())] += carry;
        }
        result.digits = product;
        int this_frac = static_cast<int>(digits.size()) - decimal_pos - 1;
        int other_frac = static_cast<int>(other.digits.size()) - other.decimal_pos - 1;
        int total_frac = this_frac + other_frac;
        result.decimal_pos = static_cast<int>(result.digits.size()) - 1 - total_frac;
        result.normalize();
        return result;
    }

    /// @function `operator/=`
    /// @brief Divides this APFloat by another in place
    ///
    /// @param `other` The APFloat to divide by
    void operator/=(const APFloat &other) {
        *this = *this / other;
    }

    /// @function `operator/=`
    /// @brief Divides this APFloat by an APInt in place
    ///
    /// @param `other` The APInt to divide by
    void operator/=(const APInt &other) {
        *this = *this / APFloat(other);
    }

    /// @function `operator/`
    /// @brief Divides this APFloat by an APInt
    ///
    /// @param `other` The APInt to divide by
    /// @return `APFloat` The result of the division
    APFloat operator/(const APInt &other) const {
        return *this / APFloat(other);
    }

    /// @function `operator/`
    /// @brief Divides two APFloats
    ///
    /// @param `other` The APFloat to divide by
    /// @return `APFloat` The result of the division
    APFloat operator/(const APFloat &other) const {
        if (other.digits.size() == 1 && other.digits[0] == 0) {
            assert(false && "Division by zero");
        }

        APFloat result("0");
        result.is_negative = is_negative != other.is_negative;
        const int precision = 50;
        std::vector<uint8_t> dividend = digits;
        std::vector<uint8_t> divisor = other.digits;
        std::vector<uint8_t> quotient;
        for (int i = 0; i < precision; ++i) {
            dividend.push_back(0);
        }

        std::vector<uint8_t> current;
        for (size_t i = 0; i < dividend.size(); ++i) {
            current.push_back(dividend[i]);
            while (current.size() > 1 && current[0] == 0) {
                current.erase(current.begin());
            }

            int count = 0;
            while (compare_vectors(current, divisor) >= 0) {
                current = subtract_vectors(current, divisor);
                count++;
            }
            quotient.push_back(count);
        }
        result.digits = quotient;
        result.decimal_pos = decimal_pos - other.decimal_pos + static_cast<int>(divisor.size()) - 1;
        result.normalize();
        return result;
    }

    /// @function `operator^=`
    /// @brief Raises this APFloat to the power of another in place
    ///
    /// @param `other` The exponent
    void operator^=(const APFloat &other) {
        *this = *this ^ other;
    }

    /// @function `operator^=`
    /// @brief Raises this APFloat to the power of an APInt in place
    ///
    /// @param `other` The exponent
    void operator^=(const APInt &other) {
        *this = *this ^ APFloat(other);
    }

    /// @function `operator^`
    /// @brief Raises this APFloat to the power of an APInt
    ///
    /// @param `other` The exponent
    /// @return `APFloat` The result of the exponentiation
    APFloat operator^(const APInt &other) const {
        return *this ^ APFloat(other);
    }

    /// @function `operator^`
    /// @brief Raises this APFloat to the power of another APFloat
    ///
    /// @param `other` The exponent
    /// @return `APFloat` The result of the exponentiation
    APFloat operator^(const APFloat &other) const {
        double base = this->to_fN<double>();
        double exp = other.to_fN<double>();
        double result_d = std::pow(base, exp);
        return APFloat(std::to_string(result_d));
    }

    /// @function `operator==`
    /// @brief Checks if two APFloats are equal
    ///
    /// @param `other` The APFloat to compare against
    /// @return `bool` True if equal, false otherwise
    bool operator==(const APFloat &other) const {
        if (is_negative != other.is_negative) {
            return false;
        }
        if (decimal_pos != other.decimal_pos) {
            return false;
        }
        if (digits.size() != other.digits.size()) {
            return false;
        }
        return digits == other.digits;
    }

    /// @function `operator!=`
    /// @brief Checks if two APFloats are not equal
    ///
    /// @param `other` The APFloat to compare against
    /// @return `bool` True if not equal, false otherwise
    bool operator!=(const APFloat &other) const {
        return !(*this == other);
    }

    /// @function `is_larger_than`
    /// @brief Checks if this APFloat is larger than another
    ///
    /// @param `other` The APFloat to compare against
    /// @return `bool` True if this is larger, false otherwise
    bool is_larger_than(const APFloat &other) const {
        if (!is_negative && other.is_negative) {
            return true;
        }
        if (is_negative && !other.is_negative) {
            return false;
        }

        bool actual_result;
        if (decimal_pos != other.decimal_pos) {
            actual_result = decimal_pos > other.decimal_pos;
        } else {
            actual_result = compare_vectors(digits, other.digits) > 0;
        }
        return is_negative ? !actual_result : actual_result;
    }

    /// @var `is_negative`
    /// @brief Whether this floating point number is negative
    bool is_negative;

    /// @var `digits`
    /// @brief The actual digits of the floating point number, containing both integer and fractional digits
    std::vector<uint8_t> digits;

    /// @decimal_pos`
    /// @brief The position of the decimal point, like the exponent in "real" floating point numbers
    int decimal_pos;

  private:
    void normalize() {
        while (digits.size() > 1 && digits[0] == 0) {
            digits.erase(digits.begin());
            decimal_pos--;
        }

        while (digits.size() > 1 && digits.back() == 0 && decimal_pos < static_cast<int>(digits.size()) - 1) {
            digits.pop_back();
        }

        if (digits.empty() || (digits.size() == 1 && digits[0] == 0)) {
            digits = {0};
            decimal_pos = 0;
            is_negative = false;
        }
    }

    static int compare_vectors(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b) {
        if (a.size() != b.size()) {
            return static_cast<int>(a.size()) - static_cast<int>(b.size());
        }
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i] != b[i]) {
                return a[i] - b[i];
            }
        }
        return 0;
    }

    static std::vector<uint8_t> subtract_vectors(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b) {
        std::vector<uint8_t> temp_a = a;
        std::vector<uint8_t> temp_b = b;
        while (temp_b.size() < temp_a.size()) {
            temp_b.insert(temp_b.begin(), 0);
        }

        std::vector<uint8_t> result;
        int borrow = 0;
        for (int i = static_cast<int>(temp_a.size()) - 1; i >= 0; --i) {
            int diff = temp_a[i] - temp_b[i] - borrow;
            if (diff < 0) {
                diff += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }
            result.insert(result.begin(), diff);
        }

        while (result.size() > 1 && result[0] == 0) {
            result.erase(result.begin());
        }
        return result;
    }
};
