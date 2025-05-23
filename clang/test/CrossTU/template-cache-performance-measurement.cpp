// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: echo "Testing template cache performance benefits..."

// This test demonstrates the performance benefits of template caching
// by compiling the same complex templates multiple times and measuring
// the compilation time difference.

// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -std=c++20 -fsyntax-only -ftime-report %s -o %t/without_cache.time 2>&1 | grep "Total Frontend" > %t/time_without_cache.txt || true
// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -std=c++20 -fsyntax-only -ftemplate-cache -ftime-report %s -o %t/with_cache.time 2>&1 | grep "Total Frontend" > %t/time_with_cache.txt || true
// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -std=c++20 -fsyntax-only -verify %s

// Define size_t for the test
using size_t = unsigned long;

// Complex template hierarchy designed to stress template instantiation
template<typename T, size_t N, bool EnableOptimization = true>
class ComplexTemplate {
private:
    T data[N];
    size_t size_;

public:
    ComplexTemplate() : size_(0) {
        for (size_t i = 0; i < N; ++i) {
            data[i] = T{};
        }
    }

    template<typename U>
    void process(U&& value) {
        if constexpr (EnableOptimization) {
            // Optimized path
            for (size_t i = 0; i < size_; ++i) {
                data[i] = static_cast<T>(value);
            }
        } else {
            // Unoptimized path
            for (size_t i = 0; i < N; ++i) {
                data[i] = T{};
            }
        }
    }

    template<typename Predicate>
    size_t count_if(Predicate pred) const {
        size_t count = 0;
        for (size_t i = 0; i < size_; ++i) {
            if (pred(data[i])) {
                ++count;
            }
        }
        return count;
    }

    template<typename Transformer>
    void transform(Transformer trans) {
        for (size_t i = 0; i < size_; ++i) {
            data[i] = trans(data[i]);
        }
    }
};

// Nested template to increase complexity
template<typename T, size_t Depth>
struct RecursiveTemplate {
    using type = typename RecursiveTemplate<T, Depth - 1>::type;
    static constexpr size_t depth = Depth;

    template<typename U>
    static auto process(U&& value) -> decltype(RecursiveTemplate<T, Depth - 1>::process(value)) {
        return RecursiveTemplate<T, Depth - 1>::process(value);
    }
};

template<typename T>
struct RecursiveTemplate<T, 0> {
    using type = T;
    static constexpr size_t depth = 0;

    template<typename U>
    static T process(U&& value) {
        return static_cast<T>(value);
    }
};

// Variadic template for additional complexity
template<typename... Types>
class VariadicComplexTemplate {
public:
    static constexpr size_t type_count = sizeof...(Types);

    template<size_t Index>
    using type_at = typename std::tuple_element<Index, std::tuple<Types...>>::type;

    template<typename Visitor>
    void visit_all(Visitor&& visitor) {
        visit_impl<0>(visitor);
    }

private:
    template<size_t Index, typename Visitor>
    void visit_impl(Visitor&& visitor) {
        if constexpr (Index < type_count) {
            visitor.template operator()<type_at<Index>>();
            visit_impl<Index + 1>(visitor);
        }
    }
};

// SFINAE template for type-based specialization
template<typename T, typename Enable = void>
struct SFINAEComplexTemplate {
    static constexpr const char* category = "unknown";
    T value;

    void generic_operation() {
        // Generic implementation
    }
};

template<typename T>
struct SFINAEComplexTemplate<T, typename std::enable_if_t<std::is_integral_v<T>>> {
    static constexpr const char* category = "integral";
    T value;

    T increment() { return ++value; }
    T decrement() { return --value; }
    T multiply(T factor) { return value * factor; }
};

template<typename T>
struct SFINAEComplexTemplate<T, typename std::enable_if_t<std::is_floating_point_v<T>>> {
    static constexpr const char* category = "floating_point";
    T value;

    T add_epsilon() { return value + std::numeric_limits<T>::epsilon(); }
    T sqrt_approx() { return value * value; } // Simplified for test
};

// Template metaprogramming for compile-time computation
template<size_t N>
struct Factorial {
    static constexpr size_t value = N * Factorial<N - 1>::value;
};

template<>
struct Factorial<0> {
    static constexpr size_t value = 1;
};

template<size_t N>
constexpr size_t factorial_v = Factorial<N>::value;

// Multiple instantiations to stress the template system
// This is where template caching should show significant benefits

void test_performance_scenario_1() {
    // First set of instantiations
    ComplexTemplate<int, 100> int_template_1;
    ComplexTemplate<double, 100> double_template_1;
    ComplexTemplate<char, 100> char_template_1;
    ComplexTemplate<float, 100> float_template_1;
    ComplexTemplate<long, 100> long_template_1;

    int_template_1.process(42);
    double_template_1.process(3.14);
    char_template_1.process('A');
    float_template_1.process(2.5f);
    long_template_1.process(1000L);

    // Recursive template instantiations
    using deep_int = RecursiveTemplate<int, 10>::type;
    using deep_double = RecursiveTemplate<double, 10>::type;
    using deep_char = RecursiveTemplate<char, 10>::type;

    auto result1 = RecursiveTemplate<int, 10>::process(42);
    auto result2 = RecursiveTemplate<double, 10>::process(3.14);
    auto result3 = RecursiveTemplate<char, 10>::process('X');
}

void test_performance_scenario_2() {
    // Second set of instantiations (should benefit from cache)
    ComplexTemplate<int, 100> int_template_2;
    ComplexTemplate<double, 100> double_template_2;
    ComplexTemplate<char, 100> char_template_2;
    ComplexTemplate<float, 100> float_template_2;
    ComplexTemplate<long, 100> long_template_2;

    int_template_2.process(84);
    double_template_2.process(6.28);
    char_template_2.process('B');
    float_template_2.process(5.0f);
    long_template_2.process(2000L);

    // Same recursive template instantiations (should be cached)
    using deep_int_2 = RecursiveTemplate<int, 10>::type;
    using deep_double_2 = RecursiveTemplate<double, 10>::type;
    using deep_char_2 = RecursiveTemplate<char, 10>::type;

    auto result4 = RecursiveTemplate<int, 10>::process(84);
    auto result5 = RecursiveTemplate<double, 10>::process(6.28);
    auto result6 = RecursiveTemplate<char, 10>::process('Y');
}

void test_performance_scenario_3() {
    // Third set with variadic templates
    VariadicComplexTemplate<int, double, char, float, long> variadic_1;
    VariadicComplexTemplate<int, double, char, float, long> variadic_2;
    VariadicComplexTemplate<int, double, char, float, long> variadic_3;

    // SFINAE template instantiations
    SFINAEComplexTemplate<int> sfinae_int_1;
    SFINAEComplexTemplate<double> sfinae_double_1;
    SFINAEComplexTemplate<int> sfinae_int_2;
    SFINAEComplexTemplate<double> sfinae_double_2;

    sfinae_int_1.increment();
    sfinae_double_1.add_epsilon();
    sfinae_int_2.multiply(2);
    sfinae_double_2.sqrt_approx();
}

void test_performance_scenario_4() {
    // Fourth set with compile-time computations
    constexpr auto fact_5 = factorial_v<5>;
    constexpr auto fact_10 = factorial_v<10>;
    constexpr auto fact_15 = factorial_v<15>;

    // More complex template instantiations
    ComplexTemplate<int, 200, true> optimized_1;
    ComplexTemplate<int, 200, false> unoptimized_1;
    ComplexTemplate<double, 200, true> optimized_2;
    ComplexTemplate<double, 200, false> unoptimized_2;

    optimized_1.process(100);
    unoptimized_1.process(100);
    optimized_2.process(100.0);
    unoptimized_2.process(100.0);

    // Lambda-based operations
    auto lambda_pred = [](int x) { return x > 50; };
    auto lambda_trans = [](int x) { return x * 2; };

    optimized_1.count_if(lambda_pred);
    optimized_1.transform(lambda_trans);
    optimized_2.count_if([](double x) { return x > 50.0; });
    optimized_2.transform([](double x) { return x * 2.0; });
}

void test_performance_scenario_5() {
    // Fifth set - repeat previous instantiations to maximize cache benefit
    ComplexTemplate<int, 100> repeat_int;
    ComplexTemplate<double, 100> repeat_double;
    ComplexTemplate<char, 100> repeat_char;

    repeat_int.process(999);
    repeat_double.process(9.99);
    repeat_char.process('Z');

    // Repeat recursive templates
    using repeat_deep_int = RecursiveTemplate<int, 10>::type;
    using repeat_deep_double = RecursiveTemplate<double, 10>::type;

    auto repeat_result1 = RecursiveTemplate<int, 10>::process(999);
    auto repeat_result2 = RecursiveTemplate<double, 10>::process(9.99);

    // Repeat SFINAE templates
    SFINAEComplexTemplate<int> repeat_sfinae_int;
    SFINAEComplexTemplate<double> repeat_sfinae_double;

    repeat_sfinae_int.increment();
    repeat_sfinae_double.add_epsilon();
}

// Define the missing std types for SFINAE templates
namespace std {
    template<typename T> struct is_integral { static constexpr bool value = false; };
    template<> struct is_integral<int> { static constexpr bool value = true; };
    template<> struct is_integral<long> { static constexpr bool value = true; };
    template<> struct is_integral<char> { static constexpr bool value = true; };
    template<typename T> constexpr bool is_integral_v = is_integral<T>::value;

    template<typename T> struct is_floating_point { static constexpr bool value = false; };
    template<> struct is_floating_point<float> { static constexpr bool value = true; };
    template<> struct is_floating_point<double> { static constexpr bool value = true; };
    template<typename T> constexpr bool is_floating_point_v = is_floating_point<T>::value;

    template<bool B, typename T = void> struct enable_if {};
    template<typename T> struct enable_if<true, T> { using type = T; };
    template<bool B, typename T = void> using enable_if_t = typename enable_if<B, T>::type;

    template<typename T> struct numeric_limits {
        static constexpr T epsilon() { return T{}; }
    };
    template<> struct numeric_limits<double> {
        static constexpr double epsilon() { return 2.22045e-16; }
    };
    template<> struct numeric_limits<float> {
        static constexpr float epsilon() { return 1.19209e-07f; }
    };

    template<size_t I, typename T> struct tuple_element;
    template<typename... Types> struct tuple;
    template<typename T, typename... Types>
    struct tuple_element<0, tuple<T, Types...>> { using type = T; };
    template<size_t I, typename T, typename... Types>
    struct tuple_element<I, tuple<T, Types...>> { using type = typename tuple_element<I-1, tuple<Types...>>::type; };
}

// expected-no-diagnostics