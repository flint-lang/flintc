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

/// @enum `TrieAffinity`
/// @brief Describes in which direction of a bidirectional trie a pattern prefers to be matched and recorded
enum class TrieAffinity { FORWARD, BACKWARD };

/// @class `Trie`
/// @brief This class is an abstraction over the `Matcher` class. It essentially caches high-frequency patterns at runtime and based on
/// their frequency chooses different patterns based on the first couple of tokens to match. This (hopefully) makes parsing a lot faster
/// since less time is spent in the matching functions. The matching functions like `Matcher::tokens_contain` are called hundreds of
/// thousand times and this entire class is an effort to reduce the number of those calls through caching.
///
/// The trie has a single root which holds both a forward and a backward trie, both of which build up dynamically. Every match walks
/// both directions to their deepest node and the direction which went deeper holds the more specific patterns, so its candidates are
/// checked first. When a pattern is only found through the cold path, branches are added along the still-unwalked tokens of the
/// pattern's affinity direction. The root's candidate list is kept in global frequency order and doubles as the cold-path scan order.
///
/// @info The trie is meant to be extended via CRTP: `class MyTrie : public Trie<MyTrie>`. The deriving class has to define:
/// - a nested `enum class Pattern` holding every pattern it wants to match
/// - a static `matchers` map from each pattern to its `PatternInfo` (a verifying function plus its trie affinity)
/// - a static `init(RootNode<Pattern> &)` which seeds the root with the trie's pattern range by calling `RootNode::init`, for example
///     static void init(Trie<MyTrie>::RootNode<Pattern> &root) {
///         Trie<MyTrie>::RootNode<Pattern>::init(root, Pattern::FIRST_PATTERN_ENUM, Pattern::LAST_PATTERN_ENUM);
///     }
template <typename Derived, unsigned int Depth = 3> class Trie {
  public:
    /// @typedef `verify_fn`
    /// @brief The signature of a pattern's verifying function
    using verify_fn = std::function<bool(const token_slice &)>;

    /// @struct `PatternInfo`
    /// @brief Bundles the verifying function of a pattern with the routing hint which decides in which side of the trie the pattern is
    /// added and recorded
    struct PatternInfo {
        /// @var `affinity`
        /// @brief The side of the trie in which the pattern prefers to be matched and recorded
        TrieAffinity affinity;

        /// @var `verify`
        /// @brief Verifies whether the given token slice matches this pattern
        verify_fn verify;
    };

    /// @typedef `matchers_map`
    /// @brief The map type used by the deriving class to register its patterns
    template <typename Pattern>
        requires EnumClassPattern<Pattern>
    using matchers_map = std::unordered_map<Pattern, PatternInfo>;

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

    /// Forward-declaration of the node struct
    template <typename Pattern>
        requires EnumClassPattern<Pattern>
    struct RootNode;

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

        /// @function `insert_candidate`
        /// @brief Inserts a new candidate into the candidates list if that candidate is not present in it yet
        ///
        /// @param `node` The node in which the candidate is inserted into
        /// @param `pattern` The pattern of the candidate which is being inserted
        ///
        /// @note The caller must hold the node's mutex (shared or exclusive) when calling this function
        static void insert_candidate(Node<Pattern> *const node, const Pattern pattern) {
            for (const auto &candidate : node->candidates) {
                if (candidate->pattern == pattern) {
                    return;
                }
            }
            node->candidates.push_back(std::make_unique<Candidate<Pattern>>(pattern, 1));
        }

        /// @function `record_hit`
        /// @brief Records a hit for the given pattern in the given node: increments its count, or inserts it with a count of 1 if it
        /// is not a candidate yet, and bubbles it towards the front of the candidate list as long as it outranks its predecessor. This
        /// keeps every node's candidate list frequency-ordered, so the most frequently matched pattern is always tried first.
        ///
        /// @param `node` The node in which the hit is recorded
        /// @param `pattern` The pattern which was matched
        static void record_hit(Node<Pattern> *const node, const Pattern pattern) {
            const std::unique_lock<std::shared_mutex> lock(node->mutex);
            std::size_t index = 0;
            for (; index < node->candidates.size(); ++index) {
                if (node->candidates[index]->pattern == pattern) {
                    break;
                }
            }
            if (index == node->candidates.size()) {
                node->candidates.push_back(std::make_unique<Candidate<Pattern>>(pattern, 1));
            } else {
                node->candidates[index]->count.fetch_add(1, std::memory_order_relaxed);
            }
            const uint64_t count = node->candidates[index]->count.load(std::memory_order_relaxed);
            while (index > 0 && count > node->candidates[index - 1]->count.load(std::memory_order_relaxed)) {
                std::swap(node->candidates[index], node->candidates[index - 1]);
                --index;
            }
        }

        /// @function `check_hot_candidates`
        /// @brief Tries the candidates at the deepest reachable node of a walk. Since every node stores the hit counts of
        /// every pattern which has ever been matched at it, the most frequently matched pattern is always tried first.
        ///
        /// @param `root` The root node of the trie
        /// @param `node` The deepest node of the walk whose candidates are checked
        /// @param `matchers` The map of all pattern matching functions
        /// @param `tokens` The tokens to find a matching pattern for
        /// @return `std::optional<Pattern>` The matching pattern, nullopt if none of the node's candidates matched
        static std::optional<Pattern> check_hot_candidates( //
            RootNode<Pattern> &root,                        //
            Node<Pattern> *const node,                      //
            const matchers_map<Pattern> &matchers,          //
            const token_slice &tokens                       //
        ) {
            if (node == nullptr || node == &root) {
                return std::nullopt;
            }
            std::shared_lock<std::shared_mutex> lock(node->mutex);
            for (const auto &candidate : node->candidates) {
                hot_pattern_match_calls.fetch_add(1, std::memory_order_relaxed);
                if (!matchers.find(candidate->pattern)->second.verify(tokens)) {
                    continue;
                }
                const Pattern hit_pattern = candidate->pattern;
                lock.unlock();
                hot_hits.fetch_add(1, std::memory_order_relaxed);
                record_hit(&root, hit_pattern);
                record_hit(node, hit_pattern);
                return hit_pattern;
            }
            return std::nullopt;
        }

        /// @function `scan_cold`
        /// @brief Cold phase: iterates the root's globally frequency-ordered candidate list and returns the first pattern which
        /// verifies. Since the root is seeded with every pattern at startup and each hit re-orders the list by frequency, the most
        /// common pattern is always tried first.
        ///
        /// @param `root` The root node of the trie whose candidate list is scanned
        /// @param `matchers` The map of all pattern matching functions
        /// @param `tokens` The tokens to find a matching pattern for
        /// @return `std::optional<Pattern>` The matching pattern, nullopt if none of the candidates matched
        static std::optional<Pattern> scan_cold(   //
            Node<Pattern> &root,                   //
            const matchers_map<Pattern> &matchers, //
            const token_slice &tokens              //
        ) {
            PROFILE_CUMULATIVE("Trie::scan_cold");
            std::shared_lock<std::shared_mutex> lock(root.mutex);
            for (const auto &candidate : root.candidates) {
                cold_pattern_match_calls.fetch_add(1, std::memory_order_relaxed);
                if (!matchers.at(candidate->pattern).verify(tokens)) {
                    continue;
                }
                return candidate->pattern;
            }
            return std::nullopt;
        }

        /// @function `match`
        /// @brief Identifies a matching pattern using the given `matchers` map and returns the pattern which matches the tokens, if a
        /// matching pattern was able to be found.
        ///
        /// @param `root` The root node of the trie, holding both the forward and the backward trie
        /// @param `matchers` The map of all pattern matching functions
        /// @param `tokens` The tokens to find a matching pattern for
        /// @return `std::optional<Pattern>` The matching pattern, nullopt if no pattern could be found
        static std::optional<Pattern> match(       //
            RootNode<Pattern> &root,               //
            const matchers_map<Pattern> &matchers, //
            const token_slice &tokens              //
        ) {
            PROFILE_CUMULATIVE("Trie::match");
            match_calls.fetch_add(1, std::memory_order_relaxed);

            size_t forward_depth = 0;
            Node<Pattern> *forward_node = nullptr;
            {
                const std::shared_lock<std::shared_mutex> lock(root.mutex);
                const auto forward_branch = root.branches.find(tokens.first->token);
                if (forward_branch != root.branches.end()) {
                    forward_node = forward_branch->second.get();
                    forward_depth++;
                }
            }
            for (; forward_depth < Depth && forward_node != nullptr; forward_depth++) {
                const auto &next_tok = tokens.first + forward_depth;
                if (next_tok == tokens.second) {
                    break;
                }
                Node<Pattern> *next_node = nullptr;
                {
                    const std::shared_lock<std::shared_mutex> lock(forward_node->mutex);
                    const auto branch = forward_node->branches.find(next_tok->token);
                    if (branch != forward_node->branches.end()) {
                        next_node = branch->second.get();
                    }
                }
                if (next_node == nullptr) {
                    break;
                }
                forward_node = next_node;
            }
            size_t backward_depth = 0;
            Node<Pattern> *backward_node = nullptr;
            {
                const std::shared_lock<std::shared_mutex> lock(root.mutex);
                const auto backward_branch = root.back_branches.find(std::prev(tokens.second)->token);
                if (backward_branch != root.back_branches.end()) {
                    backward_node = backward_branch->second.get();
                    backward_depth++;
                }
            }
            for (; backward_depth < Depth && backward_node != nullptr; backward_depth++) {
                const auto &next_tok = tokens.second - backward_depth;
                if (next_tok == tokens.first) {
                    break;
                }
                Node<Pattern> *next_node = nullptr;
                {
                    const std::shared_lock<std::shared_mutex> lock(backward_node->mutex);
                    const auto branch = backward_node->branches.find(std::prev(next_tok)->token);
                    if (branch != backward_node->branches.end()) {
                        next_node = branch->second.get();
                    }
                }
                if (next_node == nullptr) {
                    break;
                }
                backward_node = next_node;
            }

            // The direction which went deeper holds the more specific patterns, so its deepest node is checked first. A side which stopped
            // at the root has no specific patterns to offer and is skipped. If both reached equally far, forward depth is favoured.
            const bool forward_first = forward_depth >= backward_depth;
            Node<Pattern> *const first_node = forward_first ? forward_node : backward_node;
            Node<Pattern> *const second_node = forward_first ? backward_node : forward_node;
            if (const auto hit = check_hot_candidates(root, first_node, matchers, tokens)) {
                return hit;
            }
            if (const auto hit = check_hot_candidates(root, second_node, matchers, tokens)) {
                return hit;
            }

            // Cold phase: iterate the root's candidate list. Every pattern is seeded there (hit count 0) at startup and the list is
            // globally frequency-ordered, so it doubles as the full-scan order for non-root misses and as the candidate check for
            // root-stops.
            const std::optional<Pattern> scan_hit = scan_cold(root, matchers, tokens);
            if (!scan_hit.has_value()) {
                return std::nullopt;
            }
            const Pattern pattern = scan_hit.value();

            // Build branches along the still-unwalked tokens of the direction matching the pattern's affinity and record the pattern
            // as a candidate of every node along that path. Patterns with a `FORWARD` affinity walk the leading tokens, `BACKWARD` the
            // trailing ones. When the walk stopped at the root itself, the build starts from the root.
            const bool is_forward = matchers.find(pattern)->second.affinity == TrieAffinity::FORWARD;
            Node<Pattern> *const deepest = is_forward ? forward_node : backward_node;
            if (is_forward) {
                // Forward build: extend the forward trie with the leading tokens the walk did not consume. Every node on the path
                // becomes a candidate for the pattern.
                token_list::iterator branch_cursor = tokens.first + forward_depth;
                Node<Pattern> *build_node = forward_node != nullptr ? forward_node : &root;
                for (unsigned int created = 0; branch_cursor != tokens.second && created < Depth; ++created, ++branch_cursor) {
                    {
                        const std::unique_lock<std::shared_mutex> lock(build_node->mutex);
                        auto &branch = build_node->branches[branch_cursor->token];
                        if (!branch) {
                            branch = std::make_unique<Node<Pattern>>();
                        }
                        build_node = branch.get();
                    }
                    const std::unique_lock<std::shared_mutex> lock(build_node->mutex);
                    Node<Pattern>::insert_candidate(build_node, pattern);
                }
            } else {
                // Backward build: extend the backward trie with the trailing tokens the walk did not consume, walking backwards from
                // the first unconsumed token. Every node on the path becomes a candidate for the pattern.
                token_list::iterator branch_cursor = tokens.second - backward_depth;
                Node<Pattern> *build_node = backward_node != nullptr ? backward_node : &root;
                for (unsigned int created = 0; branch_cursor != tokens.first && created < Depth; ++created) {
                    --branch_cursor;
                    {
                        const std::unique_lock<std::shared_mutex> lock(build_node->mutex);
                        auto &branch =
                            (build_node == &root) ? root.back_branches[branch_cursor->token] : build_node->branches[branch_cursor->token];
                        if (!branch) {
                            branch = std::make_unique<Node<Pattern>>();
                        }
                        build_node = branch.get();
                    }
                    const std::unique_lock<std::shared_mutex> lock(build_node->mutex);
                    Node<Pattern>::insert_candidate(build_node, pattern);
                }
            }

            // Record the pattern on the deepest node the walk reached. If the walk consumed the whole slice (a shorter expression,
            // e.g. a bare variable, sharing trailing tokens with a longer pattern), no branches are built and the pattern would
            // otherwise never become a candidate again, permanently condemning it to the cold scan.
            if (deepest != nullptr && deepest != &root) {
                const std::unique_lock<std::shared_mutex> lock(deepest->mutex);
                Node<Pattern>::insert_candidate(deepest, pattern);
            }
            return pattern;
        }

        /// @function `print_hit_rates`
        /// @brief Prints the trie hit-rate statistics: the number of match calls, the number of matches which were satisfied by a
        /// trie node's candidate list (hot path), and the resulting hit-rate percentage
        ///
        /// @param `name` The name of the trie type
        static void print_hit_rates(const std::string &name) {
            if (!DEBUG_MODE || !PRINT_PERFORMANCE) {
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

    /// @struct `RootNode`
    /// @brief The type of the root node of the entire trie
    template <typename Pattern>
        requires EnumClassPattern<Pattern>
    struct RootNode : public Node<Pattern> {
      public:
        /// @var `back`
        /// @brief All the backward branches the root can branch off to
        std::unordered_map<Token, std::unique_ptr<Node<Pattern>>> back_branches;

        /// @function `init`
        /// @brief Initializes the given root node to contain every candidate in the inclusive range [`start`, `end`] with a hit count of 0
        ///
        /// @note Each root accessor guards its instance with its own `std::once_flag`, so this function is invoked exactly once
        ///
        /// @param `root` The root node to initialize
        /// @param `start` The first pattern to seed the root with (inclusive)
        /// @param `end` The last pattern to seed the root with (inclusive)
        static void init(RootNode<Pattern> &root, const Pattern start, const Pattern end) {
            const std::unique_lock<std::shared_mutex> lock(root.mutex);
            for (                                      //
                size_t i = static_cast<size_t>(start); //
                i <= static_cast<size_t>(end);         //
                i++                                    //
            ) {
                const Pattern p = static_cast<Pattern>(i);
                root.candidates.push_back(std::make_unique<Candidate<Pattern>>(p, 0));
            }
        }
    };

    /// @function `root`
    /// @brief Returns the lazily-initialized root node of the trie for the given pattern type. Calls the derived class's static
    /// `init(RootNode<Pattern> &)` exactly once on first access, which is responsible for seeding the root with its pattern range via
    /// `RootNode<Pattern>::init`.
    template <typename Pattern>
        requires EnumClassPattern<Pattern>
    static RootNode<Pattern> &root() {
        static RootNode<Pattern> instance = {};
        static std::once_flag initialized = {};
        std::call_once(initialized, [&]() { Derived::init(instance); });
        return instance;
    }

    /// @function `match`
    /// @brief Matches a given token slice and returns a pattern, if a pattern was able to be found
    ///
    /// @param `tokens` The tokens to search a matching pattern for
    /// @return `std::optional<Pattern>` The found pattern, nullopt if no pattern was able to be matched
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
