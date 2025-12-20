#pragma once

#include "parser/hash.hpp"
#include "type_name_generator.hpp"

#include <iomanip>
#include <iostream>
#include <set>
#include <string>

static void test_fuzzy(const unsigned long fuzzy_count) {
    TypeNameGenerator generator;
    std::unordered_map<uint32_t, std::vector<std::string>> hash_map;
    std::set<std::string> unique_strings;
    int zero_hashes = 0;
    Hash empty_hash(std::string(""));

    for (unsigned long i = 0; i < fuzzy_count; ++i) {
        std::string type_name = generator.generate_random_type_name();

        // Ensure we don't test the same string twice
        if (unique_strings.find(type_name) != unique_strings.end()) {
            --i; // Try again
            continue;
        }
        unique_strings.insert(type_name);

        uint32_t hash = empty_hash.get_type_id_from_str(type_name);

        if (hash == 0) {
            zero_hashes++;
            std::cout << "WARNING: Zero hash for: " << type_name << std::endl;
        }

        hash_map[hash].push_back(type_name);

        // Print progress
        if ((i + 1) % 100000 == 0) {
            std::cout << "Processed " << (i + 1) << " type names..." << std::endl;
        }
    }

    // Count collisions
    unsigned long total_collisions = 0;
    unsigned long max_collision_count = 0;
    uint32_t worst_hash = 0;

    for (const auto &entry : hash_map) {
        if (entry.second.size() > 1) {
            total_collisions += entry.second.size() - 1;

            if (entry.second.size() > max_collision_count) {
                max_collision_count = entry.second.size();
                worst_hash = entry.first;
            }
        }
    }

    // Print results
    std::cout << "===== Hash Function Fuzzy Test Results =====" << std::endl;
    std::cout << "Total unique type names tested: " << unique_strings.size() << std::endl;
    std::cout << "Zero hashes found: " << zero_hashes << std::endl;
    std::cout << "Total collisions found: " << total_collisions << std::endl;
    std::cout << "Collision rate: " << std::fixed << std::setprecision(6)
              << (static_cast<double>(total_collisions) / unique_strings.size() * 100.0) << "%" << std::endl;

    if (max_collision_count > 1) {
        std::cout << "\nWorst collision case (" << max_collision_count << " collisions):" << std::endl;
        std::cout << "Hash value: " << worst_hash << std::endl;
        std::cout << "Colliding strings:" << std::endl;
        for (const auto &str : hash_map[worst_hash]) {
            std::cout << " - \"" << str << "\"" << std::endl;
        }
    }
}
