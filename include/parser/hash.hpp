#pragma once

#include "fip.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>

/// @class `Hash`
/// @brief A small wrapper class around the `array<char, 8>` hash type, to make it copy-constructible to be used as the key of a map
///
/// The hash is pretty useful overall, but for now it simply encodes an absolute file path into a 8-byte hash which only contains the
/// characters (a-z, A-Z, 1-9) making it 61 characters in total, meaning the hash could map to 61^8 ~ 2^48 possible absolute paths
struct Hash {
  private:
    /// @function `normalize_path_for_hashing`
    /// @brief Normalizes a file path for hashing by converting it to a relative path from the current working directory.
    /// This ensures that the same source file produces the same hash regardless of where it's located on the filesystem.
    ///
    /// @param `file_path` The file path to normalize
    /// @return `std::string` The normalized relative path
    static std::string normalize_path_for_hashing(const std::filesystem::path &file_path) {
        if (file_path.empty()) {
            return "";
        }

        // Convert to absolute path first to handle relative paths correctly
        std::filesystem::path abs_path = std::filesystem::absolute(file_path);

        // Then convert to relative path from CWD
        std::filesystem::path cwd = std::filesystem::current_path();
        std::filesystem::path rel_path = std::filesystem::relative(abs_path, cwd);

        // Normalize to resolve any ".." or "." components
        std::string file_path_string = rel_path.lexically_normal().string();
        return normalize_file_path_string(file_path_string);
    }

  public:
    Hash() = default;

    explicit Hash(const std::string &hash_string) :
        path(""),
        value(string_to_hash(hash_string)) {}

    explicit Hash(const std::filesystem::path &file_path) :
        path(file_path.empty() ? file_path : std::filesystem::absolute(file_path)),
        value(string_to_hash(normalize_path_for_hashing(file_path))) {}

    /// @var `path`
    /// @brief The path this hash was used to be generated from
    std::filesystem::path path;

    /// @var `value`
    /// @brief The hash value
    std::array<char, 8> value{'0'};

    /// @function `normalize_file_path_string`
    /// @brief Normalizes the string of any file path, replacing all potential occurrences of `\\` (for example on Windows) with `/` to make
    /// the path-strings platform-agnostic and produce the same hashes
    ///
    /// @param `file_path_string` The file path string to normalize
    /// @return `std::string` The normalized file path string
    static std::string normalize_file_path_string(const std::string &file_path_string) {
        std::string local_cpy = file_path_string;
        std::replace(local_cpy.begin(), local_cpy.end(), '\\', '/');
        return local_cpy;
    }

    /// @function `string_to_hash`
    /// @brief Uses the `fip_create_hash` function implementation to create a 8-character hash from any given string input. The character
    /// hash only has 61 possible characters and roughly a variation of a 48 bit integer.
    ///
    /// @param `input` The string to hash to a small 8-byte string
    /// @return `std::array<char, 8>` The hash consisting of 8 characters from 61 possible characters (A-Z, a-z, 1-9)
    static std::array<char, 8> string_to_hash(const std::string &input) {
        char hash[8] = {'0', '0', '0', '0', '0', '0', '0', '0'};
        fip_create_hash(hash, input.data());
        std::array<char, 8> hash_arr;
        for (size_t i = 0; i < 8; i++) {
            hash_arr[i] = hash[i];
        }
        return hash_arr;
    }

    /// @function `get_type_id_from_str`
    /// @brief Gets an u32 type id from the given name of the type through hashing. Will always produce the same type ID from the same name.
    /// The value `0` is reserved and will *never* be a result from this function. All other values within the 32 bits are valid hashes
    /// though
    ///
    /// @param `name` The name of the type to get a hash id from
    /// @return `uint32_t` A u32 hash value generated from this hash + the type's name
    uint32_t get_type_id_from_str(const std::string &name) const {
        const std::string string = to_string() + "." + name;
        // 31-bit hash container
        struct Hash31 {
            uint32_t hash : 31;
            uint32_t unused : 1;
        };

        // FNV-1a hash algorithm constants
        constexpr uint32_t FNV_PRIME = 16777619u;
        constexpr uint32_t FNV_OFFSET_BASIS = 18652613u; // 2166136261 truncated to 31 bits

        // Initialize with the FNV offset basis (truncated to 31 bits automatically)
        Hash31 field;
        field.hash = FNV_OFFSET_BASIS;
        field.unused = 0;

        for (char c : string) {
            field.hash ^= static_cast<unsigned char>(c);
            field.hash *= FNV_PRIME;
        }

        // Shift left and handle zero case
        uint32_t result = static_cast<uint32_t>(field.hash) << 1;
        return (result == 0) ? 1 : result;
    }

    /// @function `empty`
    /// @brief Whether the hash is "empty", e.g. it was not initialized or default-initialized
    ///
    /// @return `bool` Whether the hash is "empty"
    bool empty() const {
        return value[0] == '0';
    }

    /// @function `to_string`
    /// @brief Returns the 8 characters as a string (I know, inefficient, who cares)
    ///
    /// @return `std::string` The converted string
    std::string to_string() const {
        std::string string(8, '0');
        for (size_t i = 0; i < 8; i++) {
            string[i] = value[i];
        }
        return string;
    }

    // copy operators
    Hash(const Hash &) = default;
    Hash &operator=(const Hash &) = default;
    // move operators
    Hash(Hash &&) = default;
    Hash &operator=(Hash &&) = default;

    friend bool operator==(const Hash &a, const Hash &b) noexcept {
        return a.value == b.value;
    }
    friend struct std::hash<Hash>;
};

template <> struct std::hash<Hash> {
    std::size_t operator()(const Hash &h) const noexcept {
        uint64_t v;
        std::memcpy(&v, h.value.data(), 8);
        return std::hash<uint64_t>{}(v);
    }
};
