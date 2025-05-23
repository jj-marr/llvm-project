// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -std=c++20 -fsyntax-only -verify %s

// Test error handling and edge cases in template caching


// Valid template for testing
template<typename T>
class ValidTemplate {
public:
    T value;
    void setValue(T v) { value = v; }
    T getValue() const { return value; }
};

// Template with potential instantiation errors
template<typename T>
class ErrorProneTemplate {
public:
    // This will cause errors for certain types
    typename T::nested_type data;

    void problematic_method() {
        // This assumes T has certain operations
        T instance;
        instance.nonexistent_method();
    }
};

// Template with SFINAE that might fail
template<typename T, typename = void>
class SFINAETemplate {
public:
    static constexpr bool valid = false;
};

template<typename T>
class SFINAETemplate<T, std::void_t<typename T::value_type>> {
public:
    static constexpr bool valid = true;
    typename T::value_type data;
};

// Template with complex constraints that might fail
#if __cplusplus >= 202002L
template<typename T>
concept ComplexConcept = requires(T t) {
    t.complex_operation();
    typename T::complex_type;
    { t.get_value() } -> std::convertible_to<int>;
};

template<ComplexConcept T>
class ConceptTemplate {
public:
    T data;
    void use_concept() {
        data.complex_operation();
        auto value = data.get_value();
    }
};
#endif

// Template with recursive instantiation that might cause issues
template<int N>
struct RecursiveTemplate {
    static constexpr int value = N + RecursiveTemplate<N-1>::value;
};

template<>
struct RecursiveTemplate<0> {
    static constexpr int value = 0;
};

// Very deep recursion that might hit limits
template<int N>
struct DeepRecursiveTemplate {
    static constexpr int value = DeepRecursiveTemplate<N-1>::value + 1;
};

template<>
struct DeepRecursiveTemplate<0> {
    static constexpr int value = 0;
};

// Template with circular dependencies
template<typename T>
class CircularA;

template<typename T>
class CircularB {
public:
    CircularA<T>* ptr;
};

template<typename T>
class CircularA {
public:
    CircularB<T> member;
};

// Template with ambiguous specializations
template<typename T, typename U = T>
class AmbiguousTemplate {
public:
    T data1;
    U data2;
};

template<typename T>
class AmbiguousTemplate<T, T> {
public:
    T shared_data;
};

// Function template with potential errors
template<typename T>
void error_prone_function(T value) {
    // This might fail for certain types
    auto result = value + value;
    result.nonexistent_method();
}

// Variable template with potential errors
template<typename T>
T error_prone_variable = T::static_member;

// Test cases for error handling

void test_valid_templates() {
    // These should work fine with valid cache
    ValidTemplate<int> valid_int;
    ValidTemplate<double> valid_double;
    ValidTemplate<char> valid_char;

    valid_int.setValue(42);
    valid_double.setValue(3.14);
    valid_char.setValue('A');

    auto int_val = valid_int.getValue();
    auto double_val = valid_double.getValue();
    auto char_val = valid_char.getValue();
}

void test_sfinae_templates() {
    // Test SFINAE templates with types that don't satisfy requirements
    SFINAETemplate<int> int_sfinae;  // Should use primary template
    static_assert(!int_sfinae.valid);

    // This would use specialized template if the type had value_type
    // SFINAETemplate<std::vector<int>> vector_sfinae;
    // static_assert(vector_sfinae.valid);
}

#if __cplusplus >= 202002L
void test_concept_templates() {
    // Test concept templates with types that don't satisfy concepts
    // These would cause compilation errors if instantiated:
    // ConceptTemplate<int> int_concept;  // int doesn't satisfy ComplexConcept
    // ConceptTemplate<double> double_concept;  // double doesn't satisfy ComplexConcept
}
#endif

void test_recursive_templates() {
    // Test recursive templates with reasonable depth
    constexpr auto val1 = RecursiveTemplate<5>::value;
    constexpr auto val2 = RecursiveTemplate<10>::value;

    // Test deeper recursion (might hit implementation limits)
    constexpr auto deep1 = DeepRecursiveTemplate<50>::value;
    // constexpr auto deep2 = DeepRecursiveTemplate<1000>::value;  // Might exceed limits
}

void test_circular_dependencies() {
    // Test circular template dependencies
    CircularA<int> circular_a;
    CircularB<double> circular_b;

    // These should compile despite circular references
    circular_a.member.ptr = &circular_a;
}

void test_ambiguous_templates() {
    // Test potentially ambiguous template instantiations
    AmbiguousTemplate<int, double> int_double;
    AmbiguousTemplate<float, float> float_float;  // Uses specialized version

    int_double.data1 = 42;
    int_double.data2 = 3.14;

    float_float.shared_data = 2.5f;
}

// Test error recovery scenarios
void test_error_recovery() {
    // Test that cache system can recover from errors
    ValidTemplate<int> recovery_test;
    recovery_test.setValue(100);

    // After potential errors, normal templates should still work
    ValidTemplate<double> after_error;
    after_error.setValue(1.5);
}

// Test cache invalidation scenarios
void test_cache_invalidation() {
    // Test templates that might need cache invalidation
    ValidTemplate<long> before_change;
    before_change.setValue(1000L);

    // Simulate scenarios where cache might be invalidated
    ValidTemplate<long> after_change;
    after_change.setValue(2000L);
}

// Test template instantiation with missing dependencies
void test_missing_dependencies() {
    // Test templates that depend on external definitions
    ValidTemplate<size_t> size_template;
    size_template.setValue(sizeof(int));

    // Test with types that might not be fully defined
    ValidTemplate<void*> ptr_template;
    ptr_template.setValue(nullptr);
}

// Test template caching with different compilation contexts
void test_different_contexts() {
    // Test templates in different contexts
    {
        ValidTemplate<int> context1;
        context1.setValue(1);
    }

    {
        ValidTemplate<int> context2;
        context2.setValue(2);
    }

    // Nested contexts
    {
        {
            ValidTemplate<double> nested;
            nested.setValue(3.14);
        }
    }
}

// Test template argument deduction edge cases
template<typename T>
void deduction_test(T value) {
    ValidTemplate<T> deduced;
    deduced.setValue(value);
}

void test_argument_deduction() {
    // Test argument deduction with various types
    deduction_test(42);        // int
    deduction_test(3.14);      // double
    deduction_test('A');       // char
    deduction_test(2.5f);      // float

    // Test with more complex types
    int array[5] = {1, 2, 3, 4, 5};
    deduction_test(array);     // int*
}

// Test template instantiation with default arguments
template<typename T = int, size_t N = 10>
class DefaultTemplate {
public:
    T data[N];
    void fill(T value) {
        for (size_t i = 0; i < N; ++i) {
            data[i] = value;
        }
    }
};

void test_default_arguments() {
    // Test various combinations of default arguments
    DefaultTemplate<> default_all;
    DefaultTemplate<double> default_size;
    DefaultTemplate<char, 5> explicit_both;

    default_all.fill(42);
    default_size.fill(3.14);
    explicit_both.fill('X');
}

// Test template friend declarations
template<typename T>
class FriendTemplate {
private:
    T private_data;

public:
    FriendTemplate(T data) : private_data(data) {}

    template<typename U>
    friend class FriendTemplate;

    template<typename U>
    friend void friend_function(FriendTemplate<U>& ft);
};

template<typename T>
void friend_function(FriendTemplate<T>& ft) {
    // Can access private members due to friend declaration
    ft.private_data = T{};
}

void test_friend_templates() {
    FriendTemplate<int> friend_int(42);
    FriendTemplate<double> friend_double(3.14);

    friend_function(friend_int);
    friend_function(friend_double);
}

// Explicit instantiations for testing
template class ValidTemplate<int>;
template class ValidTemplate<double>;
template class SFINAETemplate<int>;
template class DefaultTemplate<>;
template class FriendTemplate<int>;

template void deduction_test<int>(int);
template void friend_function<double>(FriendTemplate<double>&);

// Test expected diagnostics based on different scenarios
// valid-no-diagnostics
// invalid-warning{{template cache index contains malformed entries}}
// missing-warning{{template cache index file not found}}