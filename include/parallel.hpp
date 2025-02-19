#ifndef __PARALLEL_HPP__
#define __PARALLEL_HPP__

#include "thread_pool.hpp"

#include <iterator>
#include <optional>
#include <type_traits>

/// @brief Namespace containing parallel processing utilities
namespace Parallel {
    /// @function `run_on_all`
    /// @brief Executes a void function on all elements of a range in parallel
    ///
    /// @tparam `Func` The type of the function to execute (must return void)
    /// @tparam `Iter` The iterator type
    ///
    /// @param `func` The function to execute on each element
    /// @param `begin` Iterator to the start of the range
    /// @param `end` Iterator to the end of the range
    template <                                                                              //
        typename Func,                                                                      //
        typename Iter,                                                                      //
        typename = std::enable_if_t<                                                        //
            std::is_void_v<                                                                 //
                std::invoke_result_t<Func, typename std::iterator_traits<Iter>::value_type> //
                >                                                                           //
            >>                                                                              //
    static void run_on_all(Func &&func, Iter begin, Iter end) {
        ThreadPool pool;
        std::vector<std::future<void>> futures;

        for (auto it = begin; it != end; ++it) {
            futures.push_back(pool.enqueue(func, *it));
        }

        for (auto &fut : futures) {
            fut.get();
        }
    }

    /// @function `reduce_on_all`
    /// @brief Reduces elements of a range in parallel using a reducer function
    ///
    /// @tparam `Func` The type of the function to execute on each element
    /// @tparam `Iter` The iterator type
    /// @tparam `Reducer` The type of the reducer function
    /// @tparam `Init` The type of the initializer function
    ///
    /// @param `func` The function to execute on each element
    /// @param `begin` Iterator to the start of the range
    /// @param `end` Iterator to the end of the range
    /// @param `reducer` Function to combine two results
    /// @param `init` Function to create the initial value
    /// @return The final reduced value
    template <typename Func, typename Iter, typename Reducer, typename Init,                                                         //
        typename = std::enable_if_t<                                                                                                 //
            std::is_same_v<                                                                                                          //
                std::invoke_result_t<Func, std::remove_reference_t<typename std::iterator_traits<Iter>::reference> &>,               //
                typename std::invoke_result_t<Init>                                                                                  //
                > &&                                                                                                                 //
            std::is_same_v<                                                                                                          //
                typename std::invoke_result_t<Reducer,                                                                               //
                    typename std::invoke_result_t<Func, std::remove_reference_t<typename std::iterator_traits<Iter>::reference> &>,  //
                    typename std::invoke_result_t<Func, std::remove_reference_t<typename std::iterator_traits<Iter>::reference> &>>, //
                typename std::invoke_result_t<Func, std::remove_reference_t<typename std::iterator_traits<Iter>::reference> &>       //
                >                                                                                                                    //
            >>                                                                                                                       //
    static auto reduce_on_all(Func &&func, Iter begin, Iter end, Reducer &&reducer, Init &&init) {
        ThreadPool pool;
        using ReturnType = std::invoke_result_t<Func, typename std::iterator_traits<Iter>::reference>;
        std::vector<std::future<ReturnType>> futures;

        for (auto it = begin; it != end; ++it) {
            futures.push_back(pool.enqueue(func, *it));
        }

        ReturnType result = init();
        for (auto &fut : futures) {
            result = reducer(result, fut.get());
        }
        return result;
    }
    // --- EXAMPLE ON HOW TO OSE THE `reduce_on_all` FUNCTION ---
    namespace {
        // class MyClass {
        //   public:
        //     MyClass(int value) :
        //         _value(value) {}
        //
        //     // Const member function
        //     int process() const {
        //         return _value * 2;
        //     }
        //
        //   private:
        //     int _value;
        // };
        //
        // void example() {
        //     std::vector<MyClass> objects = {MyClass(1), MyClass(2), MyClass(3), MyClass(4), MyClass(5)};
        //     auto result = Parallel::reduce_on_all(          //
        //         [](MyClass &obj) { return obj.process(); }, // Note the const reference
        //         objects.begin(),                            //
        //         objects.end(),                              //
        //         [](int a, int b) { return a + b; },         //
        //         []() { return 0; });
        //     std::cout << "Result: " << result << std::endl;
        // }
    } // namespace

    /// @function `filter_on_all`
    /// @brief Filters elements of a range in parallel based on a predicate
    ///
    /// @tparam `Func` The type of the predicate function (must return bool)
    /// @tparam `Iter` The iterator type
    ///
    /// @param `func` The predicate function to filter elements
    /// @param `begin` Iterator to the start of the range
    /// @param `end` Iterator to the end of the range
    /// @return Vector containing the filtered elements
    template <                                                                               //
        typename Func,                                                                       //
        typename Iter,                                                                       //
        typename = std::enable_if_t<                                                         //
            std::is_same_v<                                                                  //
                std::invoke_result_t<Func, typename std::iterator_traits<Iter>::value_type>, //
                bool                                                                         //
                > &&                                                                         //
            std::is_copy_constructible_v<typename std::iterator_traits<Iter>::value_type>    //
            >>                                                                               //
    static auto filter_on_all(Func &&func, Iter begin, Iter end) {
        ThreadPool pool;
        using ValueType = typename std::iterator_traits<Iter>::value_type;
        std::vector<std::future<std::optional<ValueType>>> futures;

        for (auto it = begin; it != end; ++it) {
            futures.push_back(pool.enqueue([func, value = *it]() { return func(value) ? std::optional<ValueType>{value} : std::nullopt; }));
        }

        std::vector<ValueType> results;
        for (auto &fut : futures) {
            if (auto value = fut.get()) {
                results.push_back(*value);
            }
        }
        return results;
    }
} // namespace Parallel

#endif
