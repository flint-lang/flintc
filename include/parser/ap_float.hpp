#pragma once

#include "ap_int.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

/// @class `APFloat`
/// @brief Class which represents arbitrary precision floating point numbers
class APFloat {
  public:
    APFloat(const std::string &value) {
        if (value.empty()) {
            is_negative = false;
            int_digits.clear();
            frac_digits.clear();
        }
        size_t index = 0;
        if (value[0] == '-') {
            assert(value.size() > 1);
            is_negative = true;
            index++;
        }
        while (index < value.size()) {
            const char d = value[index];
            if (d == '.') {
                break;
            }
            assert(d >= '0' && d <= '9');
            int_digits.emplace_back(static_cast<uint8_t>(d - '0'));
            index++;
        }
        assert(index < value.size() && value[index] == '.');
        index++;
        while (index < value.size()) {
            const char d = value[index];
            assert(d >= '0' && d <= '9');
            frac_digits.emplace_back(static_cast<uint8_t>(d - '0'));
            index++;
        }
    }

    APFloat(const APInt &value) {
        is_negative = value.is_negative;
        assert(!value.digits.empty());
        int_digits = value.digits;
        frac_digits.emplace_back(0);
    }

    /// @function `to_string`
    /// @brief Converts the arbitrary float to a string
    ///
    /// @return `std::string` The converted string
    std::string to_string() const {
        std::string result;
        if (is_negative) {
            result += '-';
        }
        for (uint8_t digit : int_digits) {
            result += static_cast<char>('0' + digit);
        }
        result += '.';
        for (uint8_t digit : frac_digits) {
            result += static_cast<char>('0' + digit);
        }
        return result;
    }

    /// @function `to_fN`
    /// @brief Converts the arbitrary float to a discrete floating point number of type T
    ///
    /// @tparam `T` The floating point type to convert this APFloat to, like `float`, `double`, `long double` etc.
    /// @return `T` The result value truncated to the target precision
    template <typename T> T to_fN() const {
        T result = 0;
        for (uint8_t d : int_digits) {
            result = result * 10 + d;
        }

        // Add up fractional part: divide by 10^n
        T frac = 0;
        for (uint8_t d : frac_digits) {
            frac = frac * 10 + d;
        }
        T divisor = std::pow(T(10), frac_digits.size());
        result += frac / divisor;

        return is_negative ? -result : result;
    }

    /// @function `to_apint`
    /// @brief Converts the arbitrary float to an arbitrary integer
    ///
    /// @return `APInt` The converted APInt
    APInt to_apint() const {
        APInt apint("0");
        apint.is_negative = is_negative;
        apint.digits = int_digits;
        return apint;
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
        *this = *this + other;
    }

    /// @function `operator+` (APInt version)
    /// @brief Adds an APInt to this APFloat
    ///
    /// @param `other` The APInt to add
    /// @return `APFloat` The sum
    APFloat operator+(const APInt &other) const {
        // Convert APInt to APFloat and add
        APFloat other_float = apint_to_apfloat(other);
        return *this + other_float;
    }

    /// @function `operator+`
    /// @brief Adds two APFloat values
    ///
    /// @param `other` The APFloat to add
    /// @return `APFloat` The sum
    APFloat operator+(const APFloat &other) const {
        // Convert to same sign operations if needed
        if (is_negative != other.is_negative) {
            if (is_negative) {
                // -a + b = b - a
                APFloat temp = *this;
                temp.is_negative = false;
                return other - temp;
            } else {
                // a + (-b) = a - b
                APFloat temp = other;
                temp.is_negative = false;
                return *this - temp;
            }
        }

        // Both have same sign
        APFloat result("0.0");
        result.is_negative = is_negative;

        // Align fractional parts (pad with zeros)
        size_t max_frac = std::max(frac_digits.size(), other.frac_digits.size());
        std::vector<uint8_t> a_frac = frac_digits;
        std::vector<uint8_t> b_frac = other.frac_digits;
        while (a_frac.size() < max_frac)
            a_frac.push_back(0);
        while (b_frac.size() < max_frac)
            b_frac.push_back(0);

        // Add fractional parts from right to left
        std::vector<uint8_t> result_frac;
        uint8_t carry = 0;
        for (int i = max_frac - 1; i >= 0; i--) {
            uint8_t sum = a_frac[i] + b_frac[i] + carry;
            result_frac.push_back(sum % 10);
            carry = sum / 10;
        }
        std::reverse(result_frac.begin(), result_frac.end());

        // Add integer parts from right to left
        std::vector<uint8_t> result_int;
        int i = int_digits.size() - 1;
        int j = other.int_digits.size() - 1;
        while (i >= 0 || j >= 0 || carry > 0) {
            uint8_t sum = carry;
            if (i >= 0) {
                sum += int_digits[i];
                i--;
            }
            if (j >= 0) {
                sum += other.int_digits[j];
                j--;
            }
            result_int.push_back(sum % 10);
            carry = sum / 10;
        }
        std::reverse(result_int.begin(), result_int.end());

        result.int_digits = result_int;
        result.frac_digits = result_frac;
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
    /// @brief Subtracts another APInt from APFloat one in place
    ///
    /// @param `other` The APInt to subtract
    void operator-=(const APInt &other) {
        *this = *this - other;
    }

    /// @function `operator-` (APInt version)
    /// @brief Subtracts an APInt from this APFloat
    ///
    /// @param `other` The APInt to subtract
    /// @return `APFloat` The difference
    APFloat operator-(const APInt &other) const {
        // Convert APInt to APFloat and subtract
        APFloat other_float = apint_to_apfloat(other);
        return *this - other_float;
    }

    /// @function `operator-`
    /// @brief Subtracts two APFloat values
    ///
    /// @param `other` The APFloat to subtract
    /// @return `APFloat` The difference
    APFloat operator-(const APFloat &other) const {
        // Convert to same sign operations if needed
        if (is_negative != other.is_negative) {
            if (is_negative) {
                // -a - b = -(a + b)
                APFloat temp1 = *this;
                temp1.is_negative = false;
                APFloat temp2 = other;
                temp2.is_negative = false;
                APFloat result = temp1 + temp2;
                result.is_negative = true;
                return result;
            } else {
                // a - (-b) = a + b
                APFloat temp = other;
                temp.is_negative = false;
                return *this + temp;
            }
        }

        // Both have same sign
        if (is_negative) {
            // -a - (-b) = b - a
            APFloat temp1 = other;
            temp1.is_negative = false;
            APFloat temp2 = *this;
            temp2.is_negative = false;
            return temp1 - temp2;
        }

        // Both positive: determine which is larger
        bool a_is_larger = is_larger_than(other);

        APFloat result("0.0");
        result.is_negative = !a_is_larger;

        const APFloat &larger = a_is_larger ? *this : other;
        const APFloat &smaller = a_is_larger ? other : *this;

        // Align fractional parts
        size_t max_frac = std::max(larger.frac_digits.size(), smaller.frac_digits.size());
        std::vector<uint8_t> a_frac = larger.frac_digits;
        std::vector<uint8_t> b_frac = smaller.frac_digits;
        while (a_frac.size() < max_frac)
            a_frac.push_back(0);
        while (b_frac.size() < max_frac)
            b_frac.push_back(0);

        // Subtract fractional parts from right to left
        std::vector<uint8_t> result_frac;
        int8_t borrow = 0;
        for (int i = max_frac - 1; i >= 0; i--) {
            int8_t diff = a_frac[i] - b_frac[i] - borrow;
            if (diff < 0) {
                diff += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }
            result_frac.push_back(static_cast<uint8_t>(diff));
        }
        std::reverse(result_frac.begin(), result_frac.end());

        // Subtract integer parts from right to left
        std::vector<uint8_t> result_int;
        int i = larger.int_digits.size() - 1;
        int j = smaller.int_digits.size() - 1;
        while (i >= 0) {
            int8_t diff = larger.int_digits[i] - borrow;
            if (j >= 0) {
                diff -= smaller.int_digits[j];
                j--;
            }
            if (diff < 0) {
                diff += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }
            result_int.push_back(static_cast<uint8_t>(diff));
            i--;
        }
        std::reverse(result_int.begin(), result_int.end());

        // Remove leading zeros from integer part
        while (result_int.size() > 1 && result_int[0] == 0) {
            result_int.erase(result_int.begin());
        }

        result.int_digits = result_int;
        result.frac_digits = result_frac;
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
    /// @brief Multiplies this APFloat by another APInt in place
    ///
    /// @param `other` The APInt to multiply by
    void operator*=(const APInt &other) {
        *this = *this * other;
    }

    /// @function `operator*` (APInt version)
    /// @brief Multiplies this APFloat by an APInt
    ///
    /// @param `other` The APInt to multiply by
    /// @return `APFloat` The product
    APFloat operator*(const APInt &other) const {
        APFloat other_float = apint_to_apfloat(other);
        return *this * other_float;
    }

    /// @function `operator*`
    /// @brief Multiplies two APFloat values
    ///
    /// @param `other` The APFloat to multiply by
    /// @return `APFloat` The product
    APFloat operator*(const APFloat &other) const {
        APFloat result("0.0");
        result.is_negative = (is_negative != other.is_negative);

        // Combine all digits (treat as integers)
        std::vector<uint8_t> a_all = int_digits;
        a_all.insert(a_all.end(), frac_digits.begin(), frac_digits.end());
        std::vector<uint8_t> b_all = other.int_digits;
        b_all.insert(b_all.end(), other.frac_digits.begin(), other.frac_digits.end());

        // Total decimal places in result
        size_t total_frac = frac_digits.size() + other.frac_digits.size();

        // Multiply as integers
        std::vector<uint8_t> product(a_all.size() + b_all.size(), 0);
        for (int i = a_all.size() - 1; i >= 0; i--) {
            for (int j = b_all.size() - 1; j >= 0; j--) {
                int pos = i + j + 1;
                uint8_t mul = a_all[i] * b_all[j] + product[pos];
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

        // Split into integer and fractional parts
        std::vector<uint8_t> final_product(product.begin() + start, product.end());

        if (total_frac >= final_product.size()) {
            // Result is 0.something
            result.int_digits = {0};
            result.frac_digits.assign(total_frac - final_product.size(), 0);
            result.frac_digits.insert(result.frac_digits.end(), final_product.begin(), final_product.end());
        } else {
            // Split at the decimal point
            size_t split_pos = final_product.size() - total_frac;
            result.int_digits.assign(final_product.begin(), final_product.begin() + split_pos);
            result.frac_digits.assign(final_product.begin() + split_pos, final_product.end());
        }

        // Handle zero case
        if (result.int_digits.size() == 1 && result.int_digits[0] == 0 &&
            std::all_of(result.frac_digits.begin(), result.frac_digits.end(), [](uint8_t d) { return d == 0; })) {
            result.is_negative = false;
        }

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
    /// @brief Divides this APFloat by another APInt in place
    ///
    /// @param `other` The APInt to divide by
    void operator/=(const APInt &other) {
        *this = *this / other;
    }

    /// @function `operator/` (APInt version)
    /// @brief Divides this APFloat by an APInt
    ///
    /// @param `other` The APInt to divide by
    /// @return `APFloat` The quotient
    APFloat operator/(const APInt &other) const {
        APFloat other_float = apint_to_apfloat(other);
        return *this / other_float;
    }

    /// @function `operator/`
    /// @brief Divides two APFloat values
    ///
    /// @param `other` The APFloat to divide by
    /// @return `APFloat` The quotient
    APFloat operator/(const APFloat &other) const {
        // Division by zero check
        assert(!(other.int_digits.size() == 1 && other.int_digits[0] == 0 &&
            std::all_of(other.frac_digits.begin(), other.frac_digits.end(), [](uint8_t d) { return d == 0; })));

        APFloat result("0.0");
        result.is_negative = (is_negative != other.is_negative);

        // For arbitrary precision division, we'll use long division
        // Convert both numbers to integers by shifting decimal point
        size_t a_frac_len = frac_digits.size();
        size_t b_frac_len = other.frac_digits.size();

        // Create dividend (this number without decimal point)
        std::vector<uint8_t> dividend = int_digits;
        dividend.insert(dividend.end(), frac_digits.begin(), frac_digits.end());

        // Create divisor (other number without decimal point)
        std::vector<uint8_t> divisor = other.int_digits;
        divisor.insert(divisor.end(), other.frac_digits.begin(), other.frac_digits.end());

        // Remove leading zeros from divisor
        while (divisor.size() > 1 && divisor[0] == 0) {
            divisor.erase(divisor.begin());
            if (b_frac_len > 0)
                b_frac_len--;
        }

        // Perform long division
        // We'll limit significant (non-zero) fractional digits to 50
        const size_t MAX_SIGNIFICANT_FRAC_DIGITS = 50;
        std::vector<uint8_t> quotient_digits;
        std::vector<uint8_t> remainder = dividend;

        // Adjust decimal point position
        int decimal_pos = int_digits.size();

        // We need to track how many significant fractional digits we've generated
        size_t significant_frac_digits = 0;
        bool past_decimal = false;
        bool found_first_nonzero_frac = false;

        // Maximum iterations: enough to handle the integer part plus significant fractional digits
        // We add extra room for leading zeros in the fractional part
        const size_t MAX_ITERATIONS = dividend.size() + MAX_SIGNIFICANT_FRAC_DIGITS + 100;

        for (size_t iter = 0; iter < MAX_ITERATIONS; iter++) {
            // Find how many times divisor goes into current remainder
            uint8_t digit = 0;
            while (compare_vectors(remainder, divisor) >= 0) {
                remainder = subtract_vectors(remainder, divisor);
                digit++;
            }
            quotient_digits.push_back(digit);

            // Track if we're past the decimal point
            if (quotient_digits.size() > static_cast<size_t>(decimal_pos)) {
                past_decimal = true;
            }

            // If we're past decimal, track significant digits
            if (past_decimal) {
                if (digit != 0) {
                    found_first_nonzero_frac = true;
                }
                if (found_first_nonzero_frac && digit != 0) {
                    significant_frac_digits++;
                }
            }

            // Check if we're done
            bool remainder_zero = std::all_of(remainder.begin(), remainder.end(), [](uint8_t d) { return d == 0; });

            // Stop if:
            // 1. Remainder is zero (exact division), OR
            // 2. We've generated enough significant fractional digits
            if (remainder_zero && quotient_digits.size() >= static_cast<size_t>(decimal_pos) + 1) {
                break;
            }

            if (past_decimal && found_first_nonzero_frac && significant_frac_digits >= MAX_SIGNIFICANT_FRAC_DIGITS) {
                break;
            }

            // Add a zero to remainder (shift left by one decimal place)
            remainder.push_back(0);
        }

        // Calculate where decimal point should be in result
        int result_decimal_pos = decimal_pos + static_cast<int>(b_frac_len) - static_cast<int>(a_frac_len);

        // Remove leading zeros from integer part only
        size_t leading_zeros = 0;
        while (leading_zeros < quotient_digits.size() - 1 && quotient_digits[leading_zeros] == 0 &&
            leading_zeros < static_cast<size_t>(result_decimal_pos)) {
            leading_zeros++;
            result_decimal_pos--;
        }
        quotient_digits.erase(quotient_digits.begin(), quotient_digits.begin() + leading_zeros);

        // Split into integer and fractional parts
        if (result_decimal_pos <= 0) {
            result.int_digits = {0};
            // Fractional part includes leading zeros (naturally represented)
            result.frac_digits.assign(-result_decimal_pos, 0);
            result.frac_digits.insert(result.frac_digits.end(), quotient_digits.begin(), quotient_digits.end());
        } else if (static_cast<size_t>(result_decimal_pos) >= quotient_digits.size()) {
            result.int_digits = quotient_digits;
            result.int_digits.insert(result.int_digits.end(), result_decimal_pos - quotient_digits.size(), 0);
            result.frac_digits = {0};
        } else {
            result.int_digits.assign(quotient_digits.begin(), quotient_digits.begin() + result_decimal_pos);
            result.frac_digits.assign(quotient_digits.begin() + result_decimal_pos, quotient_digits.end());
            if (result.frac_digits.empty()) {
                result.frac_digits = {0};
            }
        }

        // Remove trailing zeros from fractional part (optional, for cleaner output)
        while (result.frac_digits.size() > 1 && result.frac_digits.back() == 0) {
            result.frac_digits.pop_back();
        }

        // Handle zero case
        if (result.int_digits.size() == 1 && result.int_digits[0] == 0 &&
            std::all_of(result.frac_digits.begin(), result.frac_digits.end(), [](uint8_t d) { return d == 0; })) {
            result.is_negative = false;
        }

        return result;
    }

    /// @function `operator^=`
    /// @brief Raises this APFloat to the power of another in place
    ///
    /// @param `exponent` The exponent to raise to
    void operator^=(const APFloat &exponent) {
        *this = *this ^ exponent;
    }

    /// @function `operator^=`
    /// @brief Raises this APFloat to the power of an APInt in place
    ///
    /// @param `exponent` The exponent to raise to
    void operator^=(const APInt &exponent) {
        *this = *this ^ exponent;
    }

    /// @function `operator^` (APInt version)
    /// @brief Raises this APFloat to the power of an APInt
    ///
    /// @param `exponent` The integer exponent to raise to
    /// @return `APFloat` The result of base^exponent
    APFloat operator^(const APInt &exponent) const {
        // Convert APInt to APFloat and use the main implementation
        APFloat exp_float = apint_to_apfloat(exponent);
        return *this ^ exp_float;
    }

    /// @function `operator^`
    /// @brief Raises this APFloat to the power of another APFloat
    ///
    /// @param `exponent` The exponent to raise to
    /// @return `APFloat` The result of base^exponent
    APFloat operator^(const APFloat &exponent) const {
        // Handle base = 0
        bool base_is_zero = (int_digits.size() == 1 && int_digits[0] == 0 &&
            std::all_of(frac_digits.begin(), frac_digits.end(), [](uint8_t d) { return d == 0; }));

        bool exp_is_zero = (exponent.int_digits.size() == 1 && exponent.int_digits[0] == 0 &&
            std::all_of(exponent.frac_digits.begin(), exponent.frac_digits.end(), [](uint8_t d) { return d == 0; }));

        if (base_is_zero) {
            // 0^0 is undefined, return 1 by convention
            if (exp_is_zero) {
                return APFloat("1.0");
            }
            // 0^(positive) = 0, 0^(negative) = undefined (infinity)
            assert(!exponent.is_negative && "0 raised to negative power is undefined");
            return APFloat("0.0");
        }

        // Handle exponent = 0: anything^0 = 1
        if (exp_is_zero) {
            return APFloat("1.0");
        }

        // Handle exponent = 1: anything^1 = itself
        bool exp_is_one = (exponent.int_digits.size() == 1 && exponent.int_digits[0] == 1 && exponent.frac_digits.size() == 1 &&
            exponent.frac_digits[0] == 0);
        if (exp_is_one && !exponent.is_negative) {
            return *this;
        }

        // For integer exponents, use exponentiation by squaring
        bool exp_is_integer = (exponent.frac_digits.size() == 1 && exponent.frac_digits[0] == 0);

        if (exp_is_integer) {
            // Convert exponent to APInt for integer exponentiation
            APInt exp_int("0");
            exp_int.is_negative = exponent.is_negative;
            exp_int.digits = exponent.int_digits;

            // For negative base with integer exponent, determine sign
            bool result_negative = false;
            if (is_negative) {
                // Check if exponent is odd
                if (!exp_int.digits.empty() && exp_int.digits.back() % 2 == 1) {
                    result_negative = true;
                }
            }

            APFloat base = *this;
            base.is_negative = false;

            if (exponent.is_negative) {
                // base^(-n) = 1 / (base^n)
                exp_int.is_negative = false;
                APFloat powered = base ^ APFloat(exp_int);
                APFloat result = APFloat("1.0") / powered;
                result.is_negative = result_negative;
                return result;
            }

            // Positive integer exponent - use binary exponentiation
            APFloat result("1.0");
            APInt exp_copy = exp_int;

            while (!(exp_copy.digits.size() == 1 && exp_copy.digits[0] == 0)) {
                // If exp is odd, multiply result by base
                if (exp_copy.digits.back() % 2 == 1) {
                    result = result * base;
                }

                // Square the base
                base = base * base;

                // Divide exp by 2
                exp_copy = APInt::divide_by_2(exp_copy);
            }

            result.is_negative = result_negative;
            return result;
        }

        // For fractional exponents, we'd need to implement root finding
        // This is complex and may not be needed for compile-time constants
        // For now, assert that fractional exponents are not supported
        assert(false && "Fractional exponents not yet supported for APFloat");
        return APFloat("0.0");
    }

    /// @var `is_negative`
    /// @brief Whether this arbitrary precision floating point number is negative
    bool is_negative = false;

    /// @var `int_digits`
    /// @brief The integer digits from the APFloat
    std::vector<uint8_t> int_digits;

    /// @var `frac_digits`
    /// @brief The fractional digits from the APFloat
    std::vector<uint8_t> frac_digits;

  private:
    /// @function `is_larger_than`
    /// @brief Determines if this APFloat is larger than another (ignoring sign)
    ///
    /// @param `other` The other APFloat to compare to
    /// @return `bool` True if this is larger in magnitude
    bool is_larger_than(const APFloat &other) const {
        // Compare integer parts first
        if (int_digits.size() > other.int_digits.size())
            return true;
        if (int_digits.size() < other.int_digits.size())
            return false;

        for (size_t i = 0; i < int_digits.size(); i++) {
            if (int_digits[i] > other.int_digits[i])
                return true;
            if (int_digits[i] < other.int_digits[i])
                return false;
        }

        // Integer parts equal, compare fractional parts
        size_t min_frac = std::min(frac_digits.size(), other.frac_digits.size());
        for (size_t i = 0; i < min_frac; i++) {
            if (frac_digits[i] > other.frac_digits[i])
                return true;
            if (frac_digits[i] < other.frac_digits[i])
                return false;
        }

        // If all compared digits are equal, the one with more fractional digits is larger
        return frac_digits.size() > other.frac_digits.size();
    }

    /// @function `apint_to_apfloat`
    /// @brief Converts an APInt to an APFloat
    ///
    /// @param `apint` The APInt to convert
    /// @return `APFloat` The converted value
    static APFloat apint_to_apfloat(const APInt &apint) {
        std::string int_str = apint.to_string();
        std::string float_str = int_str + ".0";
        return APFloat(float_str);
    }

    /// @function `compare_vectors`
    /// @brief Compares two digit vectors (as unsigned integers)
    ///
    /// @param `a` First vector
    /// @param `b` Second vector
    /// @return `int` -1 if a < b, 0 if a == b, 1 if a > b
    static int compare_vectors(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b) {
        // Remove leading zeros for comparison
        size_t a_start = 0, b_start = 0;
        while (a_start < a.size() - 1 && a[a_start] == 0)
            a_start++;
        while (b_start < b.size() - 1 && b[b_start] == 0)
            b_start++;

        size_t a_len = a.size() - a_start;
        size_t b_len = b.size() - b_start;

        if (a_len > b_len)
            return 1;
        if (a_len < b_len)
            return -1;

        for (size_t i = 0; i < a_len; i++) {
            if (a[a_start + i] > b[b_start + i])
                return 1;
            if (a[a_start + i] < b[b_start + i])
                return -1;
        }
        return 0;
    }

    /// @function `subtract_vectors`
    /// @brief Subtracts vector b from vector a (assumes a >= b)
    ///
    /// @param `a` The minuend
    /// @param `b` The subtrahend
    /// @return `std::vector<uint8_t>` The difference
    static std::vector<uint8_t> subtract_vectors(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b) {
        std::vector<uint8_t> result = a;
        int8_t borrow = 0;

        for (int i = result.size() - 1; i >= 0; i--) {
            int j = i - static_cast<int>(result.size() - b.size());
            int8_t diff = result[i] - borrow;
            if (j >= 0 && j < static_cast<int>(b.size())) {
                diff -= b[j];
            }
            if (diff < 0) {
                diff += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }
            result[i] = static_cast<uint8_t>(diff);
        }

        return result;
    }
};
