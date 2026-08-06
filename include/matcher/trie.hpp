#pragma once

#include "globals.hpp"
#include "profiler.hpp"

#include <atomic>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <type_traits>
#include <unordered_map>
#include <vector>

/// @concept `EnumClassPattern`
/// @brief Requires a type to be an `enum class`. Scoped enums never convert implicitly to their underlying integral type, whereas
/// unscoped enums do, which is exactly how the two are told apart.
template <typename P>
concept EnumClassPattern = std::is_enum_v<P> && //
    !std::is_convertible_v<P, std::underlying_type_t<P>>;

/// @class `Trie`
/// @brief This class is an abstraction over the `Matcher` class. It essentially caches high-frequency patterns at runtime and based on
/// their frequency chooses different patterns based on the first couple of tokens to match. This (hopefully) makes parsing a lot faster
/// since less time is spent in the matching functions. The matching functions like `Matcher::tokens_contain` are called hundreds of
/// thousand times and this entire class is an effor to reduce the number of those calls through caching.
///
/// @info The trie is meant to be extended via CRTP: `class MyTrie : public Trie<MyTrie>`. The deriving class has to define:
/// - a nested `enum class Pattern` holding every pattern it wants to match
/// - a static `matchers` map from each pattern to its verifying
/// - a static `init(Node<Pattern> &)` which seeds the root with the trie's pattern range by calling `init_root`, for example
///     static void init(Trie<MyTrie>::Node<Pattern> &root) {
///         Trie<MyTrie>::Node<Pattern>::init_root(root, Pattern::FIRST_PATTERN_ENUM, Pattern::LAST_PATTERN_ENUM);
///     }
template <typename Derived> class Trie {
  public:
    using verify_fn = std::function<bool(const token_slice &)>;

    /// @var `MAX_BRANCH_DEPTH`
    /// @brief Absolute cap on how deep a chain of branch nodes may grow below the root
    static constexpr unsigned int MAX_BRANCH_DEPTH = 3;

    /// @struct `Candidate`
    /// @brief A single candidate of a trie node. Its success-count drives the self-organizing order of the flat candidate list
    template <typename Pattern>
        requires EnumClassPattern<Pattern>
    struct Candidate {
        /// @var `pattern`
        /// @brief The pattern of this candidate
        Pattern pattern;

        /// @var `count`
        /// @brief How often this candidate has been matched correctly already
        std::atomic<uint64_t> count;

        Candidate(Pattern p, uint64_t c) :
            pattern(p),
            count(c) {}
    };

    /// @struct `Node`
    /// @brief A single node in the trie
    template <typename Pattern>
        requires EnumClassPattern<Pattern>
    struct Node {
        /// @var `mutex`
        /// @brief Every node needs its own mutex when adding new candidates or branches, as then these values are not allowed to be
        /// accessed by other threads
        std::shared_mutex mutex;

        /// @var `branches`
        /// @brief All the branches this trie node can branch off to
        std::unordered_map<Token, std::unique_ptr<Node<Pattern>>> branches;

        /// @var `candidates`
        /// @brief All the candidates at this trie depth. Every pattern which has ever been matched through this node is present in
        /// this list, so that its hit count can be tracked at every depth level
        std::vector<std::unique_ptr<Candidate<Pattern>>> candidates;

        /// @var `match_calls`
        /// @brief Total number of `match` calls performed, used for the hit-rate calculation
        static inline std::atomic<uint64_t> match_calls = 0;

        /// @var `hot_hits`
        /// @brief Number of `match` calls which were satisfied by a trie node's candidate list (hot path), used for the hit-rate
        /// calculation
        static inline std::atomic<uint64_t> hot_hits = 0;

        /// @var `hot_pattern_match_calls`
        /// @brief Total number of pattern matching calls executed in the hot path. This is needed to track the number of pattern matching
        /// attempts (calling the matcher functions) per actual match function call. Lower numbers are better. This number should optimally
        /// approach the number of `match_calls`
        static inline std::atomic<uint64_t> hot_pattern_match_calls = 0;

        /// @var `cold_pattern_match_calls`
        /// @brief Total number of pattern matching calls executed in the cold path. This is needed to track the number of pattern matching
        /// attempts (calling the matcher functions) per actual match function call. Lower numbers are better. This number should optimally
        /// approach the number of `match_calls`
        static inline std::atomic<uint64_t> cold_pattern_match_calls = 0;

        /// @function `init_root`
        /// @brief Initializes the root node to contain every candidate in the inclusive range [`start`, `end`] with a hit count of 0
        ///
        /// @param `root` The root node to initialize
        /// @param `start` The first pattern to seed the root with (inclusive)
        /// @param `end` The last pattern to seed the root with (inclusive)
        static void init_root(Node<Pattern> &root, const Pattern start, const Pattern end) {
            static std::once_flag initialized = {};
            std::call_once(initialized, [&]() {
                const std::unique_lock<std::shared_mutex> lock(root.mutex);
                for (                                      //
                    size_t i = static_cast<size_t>(start); //
                    i <= static_cast<size_t>(end);         //
                    i++                                    //
                ) {
                    const Pattern p = static_cast<Pattern>(i);
                    root.candidates.push_back(std::make_unique<Candidate<Pattern>>(p, 0));
                }
            });
        }

        /// @struct `WalkResult`
        /// @brief The result of a trie walk: the deepest reachable node and the cursor position at which the walk stopped
        struct WalkResult {
            Node<Pattern> *deepest;
            token_list::iterator cursor;
        };

        /// @function `walk`
        /// @brief Follows the branches of the trie based on the leading tokens of the given slice. Stops as soon as a branch is
        /// missing for the current token or the token list is exhausted.
        ///
        /// @param `root` The root node of the trie from wich we start searching
        /// @param `tokens` The tokens whose leading tokens we use to branch the trie
        /// @return `WalkResult` The deepest reachable node and the cursor position at which the walk stopped
        static WalkResult walk(Node<Pattern> &root, const token_slice &tokens) {
            PROFILE_CUMULATIVE("Trie::walk");
            Node<Pattern> *node = &root;
            token_list::iterator cursor = tokens.first;
            while (cursor != tokens.second) {
                std::shared_lock<std::shared_mutex> lock(node->mutex);
                const auto branch = node->branches.find(cursor->token);
                if (branch == node->branches.end()) {
                    break;
                }
                node = branch->second.get();
                ++cursor;
            }
            return WalkResult{node, cursor};
        }

        /// @function `find_candidate`
        /// @brief Finds the candidate with the given pattern in the candidates list of the given node
        ///
        /// @param `node` The node in which to look for the candidate
        /// @param `pattern` The pattern of the candidate which is being searched for
        /// @return `Candidate<Pattern> *` The candidate with the given pattern, nullptr if it is not present
        ///
        /// @note The caller must hold the node's mutex (shared or exclusive) when calling this function
        static Candidate<Pattern> *find_candidate(Node<Pattern> *const node, const Pattern pattern) {
            PROFILE_CUMULATIVE("Trie::find_candidate");
            for (const auto &candidate : node->candidates) {
                if (candidate->pattern == pattern) {
                    return candidate.get();
                }
            }
            return nullptr;
        }

        /// @function `insert_candidate`
        /// @brief Inserts a new candidate into the candidates list if that candidate is not present in it yet
        ///
        /// @param `node` The node in which the candidate is inserted into
        /// @param `pattern` The pattern of the candidate which is being inserted
        /// @return `std::pair<bool Candidate<Pattern> *>` Whether the pattern was already present in the node (true) + the just-inserted
        /// pattern candidate, or the already-present candidate
        static std::pair<bool, Candidate<Pattern> *> insert_candidate(Node<Pattern> *const node, const Pattern pattern) {
            PROFILE_CUMULATIVE("Trie::insert_candidate");
            Candidate<Pattern> *candidate = Node<Pattern>::find_candidate(node, pattern);
            if (candidate != nullptr) {
                return {true, candidate};
            }
            node->candidates.push_back(std::make_unique<Candidate<Pattern>>(pattern, 1));
            return {false, node->candidates.back().get()};
        }

        /// @function `increment_candidate_in`
        /// @brief Increements the hit count of a given pattern in the given noode and re-orders the candidates
        static void increment_candidate_in(Node<Pattern> *const node, const Pattern pattern) {
            PROFILE_CUMULATIVE("Trie::increment_candidate_in");
            const std::unique_lock<std::shared_mutex> lock(node->mutex);
            const auto &candidate = Node<Pattern>::insert_candidate(node, pattern);
            if (candidate.first) {
                candidate.second->count.fetch_add(1, std::memory_order_relaxed);
            }
            Node<Pattern>::bubble_up(node, candidate.second);
        }

        /// @function `increment_candidates`
        /// @brief Increments the hit count of the given pattern in the root and the deepest node at which it was matched, and
        /// re-orders the candidates of each based on the updated counts. This keeps the candidate lists frequency-ordered, so hits
        /// land on the first candidate. The root's candidates additionally serve as the globally frequency-ordered full-scan order
        /// used by cold paths.
        ///
        /// @param `root` The root node of the trie
        /// @param `node` The deepest node at which the pattern was matched
        /// @param `pattern` The pattern which was matched
        static void increment_candidates(Node<Pattern> &root, Node<Pattern> *const node, const Pattern pattern) {
            PROFILE_CUMULATIVE("Trie::increment_candidates");
            increment_candidate_in(&root, pattern);
            increment_candidate_in(node, pattern);
        }

        /// @function `bubble_up`
        /// @brief Re-orders the candidate vector of the given node if the given candidates match count has surpassed its leading candidate.
        /// Bubbles up the given candidate as long as it surpasses the match counts of the other candidates.
        ///
        /// @param `node` The node in which to potentially reorder the candidates
        /// @param `candidate` The candidate which may have surpassed the other candidates
        ///
        /// @note The caller must hold the node's exclusive mutex when calling this function
        static void bubble_up(Node<Pattern> *const node, Candidate<Pattern> *const candidate) {
            PROFILE_CUMULATIVE("Trie::bubble_up");
            std::size_t index = node->candidates.size();
            for (std::size_t i = 0; i < node->candidates.size(); ++i) {
                if (node->candidates[i].get() == candidate) {
                    index = i;
                    break;
                }
            }
            ASSERT(index < node->candidates.size());
            if (index == 0) {
                return;
            }
            const uint64_t count = candidate->count.load(std::memory_order_relaxed);
            while (index > 0 && count > node->candidates[index - 1]->count.load(std::memory_order_relaxed)) {
                std::swap(node->candidates[index], node->candidates[index - 1]);
                --index;
            }
        }

        /// @function `match`
        /// @brief Identifies a matching pattern using the given `matchers` map and returns the pattern which matches the tokens, if a
        /// matching pattern was able to be found.
        ///
        /// @param `root` The root of the trie to search through
        /// @param `matchers` The map of all pattern matching functions
        /// @param `tokens` The tokens to find a matching pattern for
        /// @return `std::optional<Pattern>` The matching pattern, nullopt if no pattern could be found
        static std::optional<Pattern> match(                        //
            Node<Pattern> &root,                                    //
            const std::unordered_map<Pattern, verify_fn> &matchers, //
            const token_slice &tokens                               //
        ) {
            PROFILE_CUMULATIVE("Trie::match");
            match_calls.fetch_add(1, std::memory_order_relaxed);
            const WalkResult result = Node<Pattern>::walk(root, tokens);
            Node<Pattern> *node = result.deepest;
            const bool at_root = (node == &root);

            // Try the candidates at the deepest reachable node. Since every node stores the hit counts of every pattern which has
            // ever been matched at it, the most frequently matched pattern is always tried first.
            if (!at_root) {
                std::shared_lock<std::shared_mutex> lock(node->mutex);
                for (const auto &candidate : node->candidates) {
                    hot_pattern_match_calls.fetch_add(1, std::memory_order_relaxed);
                    if (!matchers.at(candidate->pattern)(tokens)) {
                        continue;
                    }
                    const Pattern hit_pattern = candidate->pattern;
                    lock.unlock();
                    hot_hits.fetch_add(1, std::memory_order_relaxed);
                    Node<Pattern>::increment_candidates(root, node, hit_pattern);
                    return hit_pattern;
                }
            }

            // Iterate the root's candidate list. Every pattern is seeded there (hit count 0) at startup and the list is globally
            // frequency-ordered, so it doubles as the full-scan order for non-root misses and as the candidate check for root-stops.
            std::optional<Pattern> scan_hit = std::nullopt;
            {
                std::shared_lock<std::shared_mutex> scan_lock(root.mutex);
                for (const auto &candidate : root.candidates) {
                    cold_pattern_match_calls.fetch_add(1, std::memory_order_relaxed);
                    if (matchers.at(candidate->pattern)(tokens)) {
                        scan_hit = candidate->pattern;
                        break;
                    }
                }
            }
            if (!scan_hit.has_value()) {
                return std::nullopt;
            }
            const Pattern pattern = scan_hit.value();
            std::vector<Node<Pattern> *> new_nodes;
            token_list::iterator branch_cursor = result.cursor;
            for (unsigned int created = 0; branch_cursor != tokens.second && created < Trie::MAX_BRANCH_DEPTH; ++created, ++branch_cursor) {
                const std::unique_lock<std::shared_mutex> lock(node->mutex);
                auto &branch = node->branches[branch_cursor->token];
                if (!branch) {
                    branch = std::make_unique<Node<Pattern>>();
                }
                node = branch.get();
                new_nodes.push_back(node);
            }

            for (Node<Pattern> *const new_node : new_nodes) {
                const std::unique_lock<std::shared_mutex> lock(new_node->mutex);
                Node<Pattern>::insert_candidate(new_node, pattern);
            }
            return pattern;
        }

        /// @function `print_hit_rates`
        /// @brief Prints the trie hit-rate statistics: the number of match calls, the number of matches which were satisfied by a
        /// trie node's candidate list (hot path), and the resulting hit-rate percentage
        ///
        /// @param `name` The name of the trie type
        static void print_hit_rates(const std::string &name) {
            if (!DEBUG_MODE) {
                return;
            }
            const uint64_t calls = match_calls.load(std::memory_order_relaxed);
            const uint64_t hits = hot_hits.load(std::memory_order_relaxed);
            const uint64_t hot_matches = hot_pattern_match_calls.load(std::memory_order_relaxed);
            const uint64_t cold_matches = cold_pattern_match_calls.load(std::memory_order_relaxed);
            const uint64_t matches = hot_matches + cold_matches;
            const double hit_rate = (calls == 0) ? 0.0 : (100.0 * static_cast<double>(hits)) / static_cast<double>(calls);
            const double hot_match_rate = (calls == 0) ? 0.0 : (static_cast<double>(hot_matches)) / static_cast<double>(calls);
            const double total_match_rate = (calls == 0) ? 0.0 : (static_cast<double>(matches)) / static_cast<double>(calls);
            const std::ios::fmtflags flags = std::cout.flags();
            std::cout << YELLOW << "[Debug Info] " << name << " Trie performance\n"
                      << DEFAULT << "-- Total match calls: " << calls << "\n"
                      << "-- Total hot matches: " << hits << "\n"
                      << "-- Hit rate: " << std::fixed << std::setprecision(2) << hit_rate << "%\n"
                      << "-- Tried matches per match call (hot): " << std::fixed << std::setprecision(2) << hot_match_rate << "\n"
                      << "-- Tried matches per match call (total): " << std::fixed << std::setprecision(2) << total_match_rate << "\n"
                      << std::endl;
            std::cout.flags(flags);
        }
    };

    /// @function `root`
    /// @brief Returns the lazily-initialized root node of the trie for the given pattern type. Calls the derived class's static
    /// `init(Node<Pattern> &)` exactly once on first access, which is responsible for seeding the root with its pattern range via
    /// `Node<Pattern>::init_root`.
    template <typename Pattern>
        requires EnumClassPattern<Pattern>
    static Node<Pattern> &root() {
        static Node<Pattern> instance = {};
        static std::once_flag initialized = {};
        std::call_once(initialized, [&]() { Derived::init(instance); });
        return instance;
    }

    /// @function `match`
    /// @brief Matches a given token slice and returns a pattern, if a pattern was able to be found
    ///
    /// @param `tokens` The tokens to search a matching pattern for
    /// @return `std::optional<Pattern>` The found pattern, nullopt if not pattern was able to be matched
    static auto match(const token_slice &tokens) {
        using Pattern = typename Derived::Pattern;
        return Node<Pattern>::match(root<Pattern>(), Derived::matchers, tokens);
    }

    /// @function `print_hit_rates`
    /// @brief Prints the trie hit-rate statistics
    ///
    /// @param `name` The name of the trie type
    static void print_hit_rates(const std::string &name) {
        using Pattern = typename Derived::Pattern;
        Node<Pattern>::print_hit_rates(name);
    }

  public:
    Trie() = delete;
};
