#pragma once

#include <ctime>
#include <random>
#include <string>
#include <vector>

// Helper function to generate random strings in different coding styles
class TypeNameGenerator {
  private:
    std::mt19937 rng;

    // Common components for type names
    const std::vector<std::string> common_prefixes = {"get", "set", "is", "has", "create", "build", "parse", "convert", "format",
        "generate", "validate", "process", "handle", "compute", "calculate", "Err"};

    const std::vector<std::string> common_words = {"User", "Data", "File", "Text", "String", "Number", "List", "Array", "Map", "Set",
        "Tree", "Graph", "Node", "Edge", "Path", "Value", "Key", "Pair", "Class", "Struct", "Enum", "Type", "Function", "Method",
        "Variable", "Constant", "Object", "Instance", "Element", "Component", "Module", "Package", "Library"};

    const std::vector<std::string> common_suffixes = {"Handler", "Manager", "Controller", "Service", "Provider", "Factory", "Builder",
        "Helper", "Util", "Utility", "Processor", "Generator", "Parser", "Formatter", "Reader", "Writer", "Converter", "Validator",
        "Iterator", "Container", "Collection"};

    // Helper to capitalize first letter
    std::string capitalize(const std::string &s) {
        std::string result = s;
        if (!result.empty()) {
            result[0] = std::toupper(result[0]);
        }
        return result;
    }

    // Helper to lowercase first letter
    std::string lowercase(const std::string &s) {
        std::string result = s;
        if (!result.empty()) {
            result[0] = std::tolower(result[0]);
        }
        return result;
    }

  public:
    TypeNameGenerator(unsigned seed = std::time(nullptr)) :
        rng(seed) {}

    std::string generate_snake_case(int components = 3) {
        std::string result;
        std::uniform_int_distribution<int> dist_prefix(0, common_prefixes.size() - 1);
        std::uniform_int_distribution<int> dist_word(0, common_words.size() - 1);
        std::uniform_int_distribution<int> dist_suffix(0, common_suffixes.size() - 1);

        for (int i = 0; i < components; ++i) {
            if (i == 0) {
                result += lowercase(common_prefixes[dist_prefix(rng)]);
            } else if (i == components - 1) {
                result += "_" + lowercase(common_suffixes[dist_suffix(rng)]);
            } else {
                result += "_" + lowercase(common_words[dist_word(rng)]);
            }
        }
        return result;
    }

    std::string generate_camel_case(int components = 3) {
        std::string result;
        std::uniform_int_distribution<int> dist_prefix(0, common_prefixes.size() - 1);
        std::uniform_int_distribution<int> dist_word(0, common_words.size() - 1);
        std::uniform_int_distribution<int> dist_suffix(0, common_suffixes.size() - 1);

        for (int i = 0; i < components; ++i) {
            if (i == 0) {
                result += lowercase(common_prefixes[dist_prefix(rng)]);
            } else if (i == components - 1) {
                result += capitalize(common_suffixes[dist_suffix(rng)]);
            } else {
                result += capitalize(common_words[dist_word(rng)]);
            }
        }
        return result;
    }

    std::string generate_pascal_case(int components = 3) {
        std::string result;
        std::uniform_int_distribution<int> dist_prefix(0, common_prefixes.size() - 1);
        std::uniform_int_distribution<int> dist_word(0, common_words.size() - 1);
        std::uniform_int_distribution<int> dist_suffix(0, common_suffixes.size() - 1);

        for (int i = 0; i < components; ++i) {
            if (i == 0) {
                result += capitalize(common_prefixes[dist_prefix(rng)]);
            } else if (i == components - 1) {
                result += capitalize(common_suffixes[dist_suffix(rng)]);
            } else {
                result += capitalize(common_words[dist_word(rng)]);
            }
        }
        return result;
    }

    std::string generate_random_type_name() {
        std::uniform_int_distribution<int> dist_style(0, 2);
        std::uniform_int_distribution<int> dist_components(2, 4);
        int style = dist_style(rng);
        int components = dist_components(rng);

        switch (style) {
            case 0:
                return generate_snake_case(components);
            case 1:
                return generate_camel_case(components);
            case 2:
                return generate_pascal_case(components);
            default:
                return generate_pascal_case(components);
        }
    }
};
