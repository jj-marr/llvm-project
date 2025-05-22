// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: echo 'c:@ST>1#T@PerformanceTemplate template-cache-performance.cpp.ast' > %t/template_index.txt
// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -std=c++20 -ast-dump=json -o %t/template-cache-performance.cpp.ast %s
// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -std=c++20 -fsyntax-only -fcrosstu-dir=%t -fcrosstu-index-name=template_index.txt -ftemplate-cache-stats -verify %s

// Test template caching performance and statistics

#include <cstddef>
#include <type_traits>

// Template designed to test cache performance
template<typename T, size_t N = 10>
class PerformanceTemplate {
private:
    T data[N];
    size_t size_;

public:
    PerformanceTemplate() : size_(0) {
        for (size_t i = 0; i < N; ++i) {
            data[i] = T{};
        }
    }

    void push_back(const T& value) {
        if (size_ < N) {
            data[size_++] = value;
        }
    }

    T& operator[](size_t index) {
        return data[index];
    }

    const T& operator[](size_t index) const {
        return data[index];
    }

    size_t size() const { return size_; }
    size_t capacity() const { return N; }

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

    template<typename Function>
    void for_each(Function func) {
        for (size_t i = 0; i < size_; ++i) {
            func(data[i]);
        }
    }

    template<typename Function>
    void for_each(Function func) const {
        for (size_t i = 0; i < size_; ++i) {
            func(data[i]);
        }
    }
};

// Recursive template to test deep instantiation caching
template<int N>
struct RecursiveTemplate {
    static constexpr int value = N + RecursiveTemplate<N-1>::value;
    using type = typename RecursiveTemplate<N-1>::type;
};

template<>
struct RecursiveTemplate<0> {
    static constexpr int value = 0;
    using type = int;
};

// Template metaprogramming constructs for performance testing
template<typename T>
struct TypeTraits {
    static constexpr bool is_pointer = false;
    static constexpr bool is_reference = false;
    static constexpr bool is_const = false;
    static constexpr size_t size = sizeof(T);
};

template<typename T>
struct TypeTraits<T*> {
    static constexpr bool is_pointer = true;
    static constexpr bool is_reference = false;
    static constexpr bool is_const = false;
    static constexpr size_t size = sizeof(T*);
};

template<typename T>
struct TypeTraits<T&> {
    static constexpr bool is_pointer = false;
    static constexpr bool is_reference = true;
    static constexpr bool is_const = false;
    static constexpr size_t size = sizeof(T);
};

template<typename T>
struct TypeTraits<const T> {
    static constexpr bool is_pointer = false;
    static constexpr bool is_reference = false;
    static constexpr bool is_const = true;
    static constexpr size_t size = sizeof(T);
};

// Complex template with multiple parameters for cache stress testing
template<typename T, typename U, size_t N, bool Flag = true>
class ComplexPerformanceTemplate {
private:
    T primary_data[N];
    U secondary_data[N];
    size_t count_;

public:
    ComplexPerformanceTemplate() : count_(0) {}

    void add_pair(const T& t, const U& u) {
        if (count_ < N) {
            primary_data[count_] = t;
            secondary_data[count_] = u;
            ++count_;
        }
    }

    template<typename Transformer>
    auto transform_primary(Transformer trans) -> decltype(trans(T{})) {
        if constexpr (Flag) {
            return count_ > 0 ? trans(primary_data[0]) : trans(T{});
        } else {
            return trans(T{});
        }
    }

    template<typename Combiner>
    auto combine_data(Combiner comb) -> decltype(comb(T{}, U{})) {
        if (count_ > 0) {
            return comb(primary_data[0], secondary_data[0]);
        }
        return comb(T{}, U{});
    }

    size_t size() const { return count_; }

    static constexpr bool has_flag() { return Flag; }
};

// Variadic template for performance testing
template<typename... Types>
class VariadicPerformanceTemplate {
public:
    static constexpr size_t type_count = sizeof...(Types);

    template<size_t Index>
    using type_at = typename std::tuple_element<Index, std::tuple<Types...>>::type;

    template<typename T>
    static constexpr bool contains_type() {
        return (std::is_same_v<T, Types> || ...);
    }

    template<typename Visitor>
    void visit_types(Visitor visitor) {
        (visitor.template operator()<Types>(), ...);
    }
};

// SFINAE-heavy template for cache performance testing
template<typename T, typename = void>
struct SFINAEPerformanceTemplate {
    static constexpr const char* category = "unknown";
    T value;
};

template<typename T>
struct SFINAEPerformanceTemplate<T, std::enable_if_t<std::is_integral_v<T>>> {
    static constexpr const char* category = "integral";
    T value;

    T increment() { return ++value; }
};

template<typename T>
struct SFINAEPerformanceTemplate<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    static constexpr const char* category = "floating_point";
    T value;

    T add_epsilon() { return value + std::numeric_limits<T>::epsilon(); }
};

template<typename T>
struct SFINAEPerformanceTemplate<T, std::enable_if_t<std::is_pointer_v<T>>> {
    static constexpr const char* category = "pointer";
    T value;

    bool is_null() const { return value == nullptr; }
};

// Function templates for performance testing
template<typename T>
void performance_function_1(T value) {
    PerformanceTemplate<T> container;
    container.push_back(value);
}

template<typename T, size_t N>
void performance_function_2(T value) {
    PerformanceTemplate<T, N> container;
    container.push_back(value);
}

template<typename T, typename U>
auto performance_function_3(T t, U u) -> decltype(t + u) {
    return t + u;
}

template<typename... Args>
void performance_variadic_function(Args... args) {
    VariadicPerformanceTemplate<Args...> processor;
    // Process arguments
}

// Variable templates for performance testing
template<typename T>
constexpr T performance_variable = T{};

template<typename T, size_t N>
constexpr size_t performance_array_size = N * sizeof(T);

// Test functions that create many instantiations
void test_basic_performance() {
    // Test many instantiations of the same template pattern
    PerformanceTemplate<int> int_container;
    PerformanceTemplate<double> double_container;
    PerformanceTemplate<char> char_container;
    PerformanceTemplate<float> float_container;
    PerformanceTemplate<long> long_container;
    PerformanceTemplate<short> short_container;
    PerformanceTemplate<unsigned int> uint_container;
    PerformanceTemplate<unsigned long> ulong_container;

    // Different sizes
    PerformanceTemplate<int, 5> small_int_container;
    PerformanceTemplate<int, 20> medium_int_container;
    PerformanceTemplate<int, 100> large_int_container;

    // Fill containers to test method instantiations
    for (int i = 0; i < 5; ++i) {
        int_container.push_back(i);
        small_int_container.push_back(i * 2);
    }

    // Test method calls
    auto int_size = int_container.size();
    auto small_size = small_int_container.size();

    // Test template methods
    auto even_count = int_container.count_if([](int x) { return x % 2 == 0; });
    int_container.for_each([](int& x) { x *= 2; });
}

void test_recursive_performance() {
    // Test recursive template instantiations
    constexpr auto val1 = RecursiveTemplate<5>::value;
    constexpr auto val2 = RecursiveTemplate<10>::value;
    constexpr auto val3 = RecursiveTemplate<15>::value;
    constexpr auto val4 = RecursiveTemplate<20>::value;

    using type1 = RecursiveTemplate<5>::type;
    using type2 = RecursiveTemplate<10>::type;
    using type3 = RecursiveTemplate<15>::type;
    using type4 = RecursiveTemplate<20>::type;
}

void test_complex_performance() {
    // Test complex multi-parameter templates
    ComplexPerformanceTemplate<int, double, 10> int_double_container;
    ComplexPerformanceTemplate<float, char, 5> float_char_container;
    ComplexPerformanceTemplate<long, short, 15> long_short_container;
    ComplexPerformanceTemplate<double, int, 8, false> double_int_container;

    int_double_container.add_pair(42, 3.14);
    float_char_container.add_pair(2.5f, 'A');

    auto transformed = int_double_container.transform_primary([](int x) { return x * 2; });
    auto combined = float_char_container.combine_data([](float f, char c) { return f + c; });
}

void test_variadic_performance() {
    // Test variadic templates with different type combinations
    VariadicPerformanceTemplate<int> single;
    VariadicPerformanceTemplate<int, double> pair;
    VariadicPerformanceTemplate<int, double, char> triple;
    VariadicPerformanceTemplate<int, double, char, float> quad;
    VariadicPerformanceTemplate<int, double, char, float, long> penta;

    static_assert(single.type_count == 1);
    static_assert(pair.type_count == 2);
    static_assert(triple.type_count == 3);
    static_assert(quad.type_count == 4);
    static_assert(penta.type_count == 5);

    static_assert(triple.contains_type<int>());
    static_assert(triple.contains_type<double>());
    static_assert(triple.contains_type<char>());
    static_assert(!triple.contains_type<float>());
}

void test_sfinae_performance() {
    // Test SFINAE-based templates
    SFINAEPerformanceTemplate<int> int_sfinae;
    SFINAEPerformanceTemplate<double> double_sfinae;
    SFINAEPerformanceTemplate<int*> ptr_sfinae;
    SFINAEPerformanceTemplate<float> float_sfinae;
    SFINAEPerformanceTemplate<char*> char_ptr_sfinae;

    int_sfinae.value = 10;
    double_sfinae.value = 3.14;

    auto incremented = int_sfinae.increment();
    auto with_epsilon = double_sfinae.add_epsilon();
    bool is_null = ptr_sfinae.is_null();
}

void test_function_performance() {
    // Test function template instantiations
    performance_function_1<int>(42);
    performance_function_1<double>(3.14);
    performance_function_1<char>('A');
    performance_function_1<float>(2.5f);

    performance_function_2<int, 5>(10);
    performance_function_2<double, 10>(1.5);
    performance_function_2<char, 20>('B');

    auto result1 = performance_function_3(5, 10);
    auto result2 = performance_function_3(2.5, 3.7);
    auto result3 = performance_function_3(1.5f, 2.5f);

    performance_variadic_function(1);
    performance_variadic_function(1, 2.0);
    performance_variadic_function(1, 2.0, 'C');
    performance_variadic_function(1, 2.0, 'C', 4.0f);
}

void test_variable_performance() {
    // Test variable template instantiations
    auto& int_var = performance_variable<int>;
    auto& double_var = performance_variable<double>;
    auto& char_var = performance_variable<char>;
    auto& float_var = performance_variable<float>;

    constexpr auto int_array_size = performance_array_size<int, 10>;
    constexpr auto double_array_size = performance_array_size<double, 5>;
    constexpr auto char_array_size = performance_array_size<char, 100>;
}

void test_type_traits_performance() {
    // Test type traits template instantiations
    static_assert(!TypeTraits<int>::is_pointer);
    static_assert(TypeTraits<int*>::is_pointer);
    static_assert(TypeTraits<int&>::is_reference);
    static_assert(TypeTraits<const int>::is_const);

    static_assert(TypeTraits<double>::size == sizeof(double));
    static_assert(TypeTraits<char*>::size == sizeof(char*));
}

// Explicit instantiations to test cache behavior
template class PerformanceTemplate<int>;
template class PerformanceTemplate<double>;
template class PerformanceTemplate<int, 20>;

template class ComplexPerformanceTemplate<int, double, 10>;
template class ComplexPerformanceTemplate<float, char, 5>;

template class VariadicPerformanceTemplate<int, double>;
template class VariadicPerformanceTemplate<int, double, char>;

template void performance_function_1<int>(int);
template void performance_function_2<double, 15>(double);

// Variable template explicit instantiations (references to existing definitions)
template const int performance_variable<int>;
template const double performance_variable<double>;

// expected-no-diagnostics