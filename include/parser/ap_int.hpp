#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/// @class `APInt`
/// @brief Class which represents arbitrary precision integer numbers
class APInt {
  public:
    APInt(const std::string &value) {
        if (value.empty()) {
            is_negative = false;
            digits.clear();
        }
        size_t index = 0;
        if (value[0] == '-') {
            assert(value.size() > 1);
            is_negative = true;
            index++;
        }
        while (index < value.size()) {
            const char d = value[index];
            assert(d >= '0' && d <= '9');
            digits.emplace_back(static_cast<uint8_t>(d - '0'));
            index++;
        }
    }

    /// @function `to_string`
    /// @brief Converts the arbitrary integer to a string
    ///
    /// @return `std::string` The converted string
    std::string to_string() const {
        std::string result;
        if (is_negative) {
            result += '-';
        }
        for (uint8_t digit : digits) {
            result += static_cast<char>('0' + digit);
        }
        return result;
    }

    /// @function `to_uN`
    /// @brief Converts the arbitrary integer to fit into an unsigned integer value of type T
    ///
    /// @tparam `T` The type of the result, like `uint8_t`, `uint32_t` etc.
    /// @return `std::optional<T>` The result value, nullopt if the integer is too big to fit into the result type or if it's negative
    template <typename T> std::optional<T> to_uN() const {
        if (is_negative) {
            return std::nullopt;
        }
        const size_t N = sizeof(T) * 8;

        // Generate max value string for unsigned N-bit integer (2^N - 1)
        std::string max_value = get_max_unsigned_value(N);

        // Check if this APInt is bigger than the max value
        if (is_bigger_than(max_value)) {
            return std::nullopt;
        }

        // Convert digit by digit using bit shifting
        T result = 0;
        for (uint8_t digit : digits) {
            // result *= 10 (using shifts: 8*result + 2*result)
            result = (result << 3) + (result << 1);
            result += static_cast<T>(digit);
        }

        return result;
    }

    /// @function `to_iN`
    /// @brief Converts the arbitrary integer to fit into a signed integer value of type T
    ///
    /// @tparam `T` The type of the result, like `int8_t`, `int32_t` etc.
    /// @return `std::optional<T>` The result value, nullopt if the integer is too big or too small to fit into the result type
    template <typename T> std::optional<T> to_iN() const {
        const size_t N = sizeof(T) * 8;
        if (is_negative) {
            // Generate min value string for signed N-bit integer (-2^(N-1))
            std::string min_value = get_min_signed_value(N);

            // Check if this APInt is smaller than the min value
            if (is_smaller_than(min_value)) {
                return std::nullopt;
            }

            // Convert digit by digit using bit shifting
            T result = 0;
            for (uint8_t digit : digits) {
                // result *= 10
                result = (result << 3) + (result << 1);
                result += static_cast<T>(digit);
            }

            return -result;
        } else {
            // Generate max value string for signed N-bit integer (2^(N-1) - 1)
            std::string max_value = get_max_signed_value(N);

            // Check if this APInt is bigger than the max value
            if (is_bigger_than(max_value)) {
                return std::nullopt;
            }

            // Convert digit by digit using bit shifting
            T result = 0;
            for (uint8_t digit : digits) {
                // result *= 10
                result = (result << 3) + (result << 1);
                result += static_cast<T>(digit);
            }

            return result;
        }
    }

    void operator+=(const APInt &other) {
        *this = *this + other;
    }

    APInt operator+(const APInt &other) const {
        // If signs are different, convert to subtraction
        if (is_negative != other.is_negative) {
            if (is_negative) {
                // -a + b = b - a
                APInt temp = *this;
                temp.is_negative = false;
                return other - temp;
            } else {
                // a + (-b) = a - b
                APInt temp = other;
                temp.is_negative = false;
                return *this - temp;
            }
        }

        // Both have same sign, perform addition
        APInt result("");
        result.digits.clear();
        result.is_negative = is_negative;

        const std::vector<uint8_t> &a = digits;
        const std::vector<uint8_t> &b = other.digits;

        // Perform addition from right to left
        std::vector<uint8_t> sum;
        uint8_t carry = 0;
        int i = a.size() - 1;
        int j = b.size() - 1;

        while (i >= 0 || j >= 0 || carry > 0) {
            uint8_t digit_sum = carry;
            if (i >= 0) {
                digit_sum += a[i];
                i--;
            }
            if (j >= 0) {
                digit_sum += b[j];
                j--;
            }
            sum.push_back(digit_sum % 10);
            carry = digit_sum / 10;
        }

        // Reverse to get correct order
        std::reverse(sum.begin(), sum.end());
        result.digits = sum;

        return result;
    }

    void operator-=(const APInt &other) {
        *this = *this - other;
    }

    APInt operator-(const APInt &other) const {
        // If signs are different, convert to addition
        if (is_negative != other.is_negative) {
            if (is_negative) {
                // -a - b = -(a + b)
                APInt temp1 = *this;
                temp1.is_negative = false;
                APInt temp2 = other;
                temp2.is_negative = false;
                APInt result = temp1 + temp2;
                result.is_negative = true;
                return result;
            } else {
                // a - (-b) = a + b
                APInt temp = other;
                temp.is_negative = false;
                return *this + temp;
            }
        }

        // Both have same sign
        if (is_negative) {
            // -a - (-b) = b - a
            APInt temp1 = other;
            temp1.is_negative = false;
            APInt temp2 = *this;
            temp2.is_negative = false;
            return temp1 - temp2;
        }

        // Both positive: a - b
        const std::vector<uint8_t> &a = digits;
        const std::vector<uint8_t> &b = other.digits;

        // Determine which is larger
        bool a_is_larger = false;
        if (a.size() > b.size()) {
            a_is_larger = true;
        } else if (a.size() < b.size()) {
            a_is_larger = false;
        } else {
            // Same size, compare digit by digit
            a_is_larger = true;
            for (size_t i = 0; i < a.size(); i++) {
                if (a[i] > b[i]) {
                    a_is_larger = true;
                    break;
                } else if (a[i] < b[i]) {
                    a_is_larger = false;
                    break;
                }
            }
        }

        APInt result("");
        result.digits.clear();

        if (!a_is_larger) {
            // Result will be negative
            result.is_negative = true;
            // Swap operands
            return other - *this;
        }

        // Perform subtraction from right to left
        std::vector<uint8_t> diff;
        int8_t borrow = 0;
        int i = a.size() - 1;
        int j = b.size() - 1;

        while (i >= 0) {
            int8_t digit_diff = a[i] - borrow;
            if (j >= 0) {
                digit_diff -= b[j];
                j--;
            }
            if (digit_diff < 0) {
                digit_diff += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }
            diff.push_back(static_cast<uint8_t>(digit_diff));
            i--;
        }

        // Reverse to get correct order
        std::reverse(diff.begin(), diff.end());

        // Remove leading zeros
        while (diff.size() > 1 && diff[0] == 0) {
            diff.erase(diff.begin());
        }

        result.digits = diff;
        return result;
    }

    void operator*=(const APInt &other) {
        *this = *this * other;
    }

    APInt operator*(const APInt &other) const {
        APInt result("0");
        result.is_negative = (is_negative != other.is_negative);

        const std::vector<uint8_t> &a = digits;
        const std::vector<uint8_t> &b = other.digits;

        // Initialize result digits to all zeros
        std::vector<uint8_t> product(a.size() + b.size(), 0);

        // Multiply digit by digit
        for (int i = a.size() - 1; i >= 0; i--) {
            for (int j = b.size() - 1; j >= 0; j--) {
                int pos = i + j + 1;
                uint8_t mul = a[i] * b[j] + product[pos];
                product[pos] = mul % 10;
                product[pos - 1] += mul / 10;
            }
        }

        // Handle carries
        for (int i = product.size() - 1; i > 0; i--) {
            if (product[i] >= 10) {
                product[i - 1] += product[i] / 10;
                product[i] %= 10;
            }
        }

        // Remove leading zeros
        size_t start = 0;
        while (start < product.size() - 1 && product[start] == 0) {
            start++;
        }

        result.digits.assign(product.begin() + start, product.end());

        // If result is zero, it should not be negative
        if (result.digits.size() == 1 && result.digits[0] == 0) {
            result.is_negative = false;
        }

        return result;
    }

    void operator/=(const APInt &other) {
        *this = *this / other;
    }

    APInt operator/(const APInt &other) const {
        // Check for division by zero
        if (other.digits.size() == 1 && other.digits[0] == 0) {
            assert(false && "Division by zero");
        }

        APInt result("0");
        result.is_negative = (is_negative != other.is_negative);

        // Create positive versions for division
        APInt dividend = *this;
        dividend.is_negative = false;
        APInt divisor = other;
        divisor.is_negative = false;

        // If dividend is smaller than divisor, result is 0
        if (dividend.is_smaller_than(divisor.to_string())) {
            result.is_negative = false;
            return result;
        }

        // Long division algorithm
        APInt current("0");
        std::vector<uint8_t> quotient;

        for (size_t i = 0; i < dividend.digits.size(); i++) {
            // Bring down next digit
            if (current.digits.size() == 1 && current.digits[0] == 0) {
                current.digits[0] = dividend.digits[i];
            } else {
                current.digits.push_back(dividend.digits[i]);
            }

            // Find how many times divisor goes into current
            uint8_t count = 0;
            while (!current.is_smaller_than(divisor.to_string())) {
                current = current - divisor;
                count++;
            }
            quotient.push_back(count);
        }

        // Remove leading zeros
        size_t start = 0;
        while (start < quotient.size() - 1 && quotient[start] == 0) {
            start++;
        }

        result.digits.assign(quotient.begin() + start, quotient.end());

        // If result is zero, it should not be negative
        if (result.digits.size() == 1 && result.digits[0] == 0) {
            result.is_negative = false;
        }

        return result;
    }

    /// @function `operator^=`
    /// @brief Raises this APInt to the power of another in place
    ///
    /// @param `exponent` The exponent to raise to
    void operator^=(const APInt &exponent) {
        *this = *this ^ exponent;
    }

    /// @function `operator^`
    /// @brief Raises this APInt to the power of another APInt
    ///
    /// @param `exponent` The exponent to raise to
    /// @return `APInt` The result of base^exponent
    APInt operator^(const APInt &exponent) const {
        // Handle negative exponents (result would be fractional, so return 0 for integer division)
        if (exponent.is_negative) {
            // For integers, negative exponents result in fractional values
            // Return 0 for consistency (or could assert/error)
            return APInt("0");
        }

        // Handle base 0
        if (digits.size() == 1 && digits[0] == 0) {
            // 0^0 is undefined, but we'll return 1 by convention
            if (exponent.digits.size() == 1 && exponent.digits[0] == 0) {
                return APInt("1");
            }
            // 0^(positive) = 0
            return APInt("0");
        }

        // Handle exponent 0: anything^0 = 1
        if (exponent.digits.size() == 1 && exponent.digits[0] == 0) {
            return APInt("1");
        }

        // Handle exponent 1: anything^1 = itself
        if (exponent.digits.size() == 1 && exponent.digits[0] == 1) {
            return *this;
        }

        // For negative bases with even/odd exponents
        bool result_negative = is_negative && (exponent.digits.back() % 2 == 1);

        // Use exponentiation by squaring for efficiency
        APInt base = *this;
        base.is_negative = false;
        APInt exp = exponent;
        APInt result("1");

        // Binary exponentiation: base^exp
        while (!(exp.digits.size() == 1 && exp.digits[0] == 0)) {
            // If exp is odd, multiply result by base
            if (exp.digits.back() % 2 == 1) {
                result = result * base;
            }

            // Square the base
            base = base * base;

            // Divide exp by 2
            exp = divide_by_2(exp);
        }

        result.is_negative = result_negative;
        return result;
    }

    /// @function `divide_by_2`
    /// @brief Divides an APInt by 2 (helper for exponentiation)
    ///
    /// @param `value` The value to divide
    /// @return `APInt` The result of value / 2
    static APInt divide_by_2(const APInt &value) {
        if (value.digits.size() == 1 && value.digits[0] == 0) {
            return APInt("0");
        }

        std::vector<uint8_t> result_digits;
        uint8_t carry = 0;

        for (size_t i = 0; i < value.digits.size(); i++) {
            uint8_t current = carry * 10 + value.digits[i];
            result_digits.push_back(current / 2);
            carry = current % 2;
        }

        // Remove leading zeros
        size_t start = 0;
        while (start < result_digits.size() - 1 && result_digits[start] == 0) {
            start++;
        }

        APInt result("0");
        result.digits.assign(result_digits.begin() + start, result_digits.end());
        result.is_negative = value.is_negative;
        return result;
    }

    /// @function `operator>`
    /// @brief Compares if this APInt is greater than another APInt
    ///
    /// @param `other` The APInt to compare with
    /// @return `bool` True if this APInt is greater than the other
    bool operator>(const APInt &other) const {
        return is_bigger_than(other.to_string());
    }

    /// @function `operator>=`
    /// @brief Compares if this APInt is greater than or equal to another APInt
    ///
    /// @param `other` The APInt to compare with
    /// @return `bool` True if this APInt is greater than or equal to the other
    bool operator>=(const APInt &other) const {
        const std::string other_str = other.to_string();
        return is_bigger_than(other_str) || to_string() == other_str;
    }

    /// @var `is_negative`
    /// @brief Whether the integer is negative
    bool is_negative = false;

    /// @var `digits`
    /// @brief The digits which form the APInt
    std::vector<uint8_t> digits;

  private:
    /// @function `get_max_unsigned_value`
    /// @brief Generates the string representation of 2^N - 1
    ///
    /// @param `bit_width` The number of bits
    /// @return `std::string` The maximum unsigned value as a string
    static std::string get_max_unsigned_value(size_t bit_width) {
        // Calculate 2^N - 1 using string arithmetic
        std::vector<uint8_t> result = {1};

        // Multiply by 2, N times (to get 2^N)
        for (size_t i = 0; i < bit_width; i++) {
            multiply_by_2(result);
        }

        // Subtract 1
        subtract_one(result);

        // Convert to string
        std::string str;
        for (uint8_t digit : result) {
            str += static_cast<char>('0' + digit);
        }
        return str;
    }

    /// @function `get_max_signed_value`
    /// @brief Generates the string representation of 2^(N-1) - 1
    ///
    /// @param `bit_width` The number of bits
    /// @return `std::string` The maximum signed value as a string
    static std::string get_max_signed_value(size_t bit_width) {
        // Calculate 2^(N-1) - 1
        std::vector<uint8_t> result = {1};

        for (size_t i = 0; i < bit_width - 1; i++) {
            multiply_by_2(result);
        }

        subtract_one(result);

        std::string str;
        for (uint8_t digit : result) {
            str += static_cast<char>('0' + digit);
        }
        return str;
    }

    /// @function `get_min_signed_value`
    /// @brief Generates the string representation of -2^(N-1)
    ///
    /// @param `bit_width` The number of bits
    /// @return `std::string` The minimum signed value as a string
    static std::string get_min_signed_value(size_t bit_width) {
        // Calculate -2^(N-1)
        std::vector<uint8_t> result = {1};

        for (size_t i = 0; i < bit_width - 1; i++) {
            multiply_by_2(result);
        }

        std::string str = "-";
        for (uint8_t digit : result) {
            str += static_cast<char>('0' + digit);
        }
        return str;
    }

    /// @function `multiply_by_2`
    /// @brief Multiplies a digit vector by 2 in place
    ///
    /// @param `digits` The digit vector to multiply
    static void multiply_by_2(std::vector<uint8_t> &digits) {
        uint8_t carry = 0;
        for (int i = digits.size() - 1; i >= 0; i--) {
            uint8_t val = digits[i] * 2 + carry;
            digits[i] = val % 10;
            carry = val / 10;
        }
        if (carry > 0) {
            digits.insert(digits.begin(), carry);
        }
    }

    /// @function `subtract_one`
    /// @brief Subtracts 1 from a digit vector in place
    ///
    /// @param `digits` The digit vector to subtract from
    static void subtract_one(std::vector<uint8_t> &digits) {
        for (int i = digits.size() - 1; i >= 0; i--) {
            if (digits[i] > 0) {
                digits[i]--;
                break;
            } else {
                digits[i] = 9;
            }
        }
        // Remove leading zeros
        while (digits.size() > 1 && digits[0] == 0) {
            digits.erase(digits.begin());
        }
    }

    /// @function `is_bigger_than`
    /// @brief Whether this integer is bigger than the given integer string
    ///
    /// @param `other` The integer string to compare this APInt to
    /// @return `bool` Whether this APInt is bigger than the other integer string
    bool is_bigger_than(const std::string &other) const {
        if (is_negative && other[0] != '-') {
            return false;
        } else if (!is_negative && other[0] == '-') {
            return true;
        }
        std::string cmp;
        bool both_negative = false;
        if (is_negative && other[0] == '-') {
            cmp = other.substr(1);
            both_negative = true;
        } else {
            cmp = other;
        }
        // This is definitely bigger since it has more digits (unless both negative)
        if (digits.size() > cmp.size()) {
            return !both_negative;
        }
        // The other is definitely bigger since it has more digits (unless both negative)
        if (digits.size() < cmp.size()) {
            return both_negative;
        }
        // They have the same amount of digits, check each digit one by one
        for (size_t i = 0; i < digits.size(); i++) {
            const uint8_t cmp_i = static_cast<uint8_t>(cmp[i] - '0');
            if (digits[i] == cmp_i) {
                continue;
            }
            // For negative numbers, larger magnitude means smaller value
            return both_negative ? (digits[i] < cmp_i) : (digits[i] > cmp_i);
        }
        return false;
    }

    /// @function `is_smaller_than`
    /// @brief Whether this integer is smaller than the given integer string
    ///
    /// @param `other` The integer string to compare this APInt to
    /// @return `bool` Whether this APInt is smaller than the other integer string
    bool is_smaller_than(const std::string &other) const {
        if (is_negative && other[0] != '-') {
            return true;
        } else if (!is_negative && other[0] == '-') {
            return false;
        }
        std::string cmp;
        bool both_negative = false;
        if (is_negative && other[0] == '-') {
            cmp = other.substr(1);
            both_negative = true;
        } else {
            cmp = other;
        }
        // This is definitely smaller since it has less digits (unless both negative)
        if (digits.size() < cmp.size()) {
            return !both_negative;
        }
        // The other is definitely smaller since it has less digits (unless both negative)
        if (digits.size() > cmp.size()) {
            return both_negative;
        }
        // They have the same amount of digits, check each digit one by one
        for (size_t i = 0; i < digits.size(); i++) {
            const uint8_t cmp_i = static_cast<uint8_t>(cmp[i] - '0');
            if (digits[i] == cmp_i) {
                continue;
            }
            // For negative numbers, larger magnitude means smaller value
            return both_negative ? (digits[i] > cmp_i) : (digits[i] < cmp_i);
        }
        return false;
    }
};
