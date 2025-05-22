// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: echo 'c:@ST>1#T@TestTemplate template-cache-basic.cpp.ast' > %t/template_index.txt
// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -std=c++20 -ast-dump=json -o %t/template-cache-basic.cpp.ast %s
// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -std=c++20 -fsyntax-only -fcrosstu-dir=%t -fcrosstu-index-name=template_index.txt -verify %s

// Test basic template caching functionality

#include <cstddef>
#include <type_traits>

template<typename T>
class TestTemplate {
public:
    T value;
    void setValue(T v) { value = v; }
    T getValue() const { return value; }
};

template<typename T>
void testFunction(T param) {
    TestTemplate<T> instance;
    instance.setValue(param);
}

template<typename T>
T testVariable = T{};

// Test implicit instantiations
void test_basic_instantiation() {
    TestTemplate<int> intTemplate;
    TestTemplate<double> doubleTemplate;
    TestTemplate<char> charTemplate;

    intTemplate.setValue(42);
    doubleTemplate.setValue(3.14);
    charTemplate.setValue('A');

    testFunction<int>(10);
    testFunction<float>(2.5f);

    auto& intVar = testVariable<int>;
    auto& floatVar = testVariable<float>;
}

// Test explicit instantiation
template class TestTemplate<long>;
template void testFunction<long>(long);
template long testVariable<long>;

// Test template with multiple parameters
template<typename T, int N>
class ArrayTemplate {
private:
    T data[N];
public:
    T& operator[](int index) { return data[index]; }
    const T& operator[](int index) const { return data[index]; }
    constexpr int size() const { return N; }
};

void test_multi_param_template() {
    ArrayTemplate<int, 10> intArray;
    ArrayTemplate<double, 5> doubleArray;
    ArrayTemplate<char, 100> charArray;

    intArray[0] = 42;
    doubleArray[0] = 3.14;
    charArray[0] = 'A';
}

// Test nested template instantiation
template<typename T>
class OuterTemplate {
public:
    template<typename U>
    class InnerTemplate {
    public:
        T outer_value;
        U inner_value;

        void setValues(T t, U u) {
            outer_value = t;
            inner_value = u;
        }
    };

    InnerTemplate<int> inner_int;
    InnerTemplate<double> inner_double;
};

void test_nested_templates() {
    OuterTemplate<float> outer;
    outer.inner_int.setValues(1.5f, 42);
    outer.inner_double.setValues(2.5f, 3.14);

    OuterTemplate<char> charOuter;
    charOuter.inner_int.setValues('X', 100);
}

// Test template specialization
template<>
class TestTemplate<bool> {
public:
    bool value;
    void setValue(bool v) { value = v; }
    bool getValue() const { return value; }

    // Specialized method
    void toggle() { value = !value; }
};

void test_specialization() {
    TestTemplate<bool> boolTemplate;
    boolTemplate.setValue(true);
    boolTemplate.toggle();
}

// Test function template specialization
template<>
void testFunction<const char*>(const char* param) {
    // Specialized implementation for const char*
    TestTemplate<const char*> stringTemplate;
    stringTemplate.setValue(param);
}

void test_function_specialization() {
    testFunction<const char*>("Hello, World!");
}

// Test SFINAE and template constraints (C++20)
#if __cplusplus >= 202002L
template<typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template<Arithmetic T>
class ArithmeticTemplate {
public:
    T value;
    T add(T other) { return value + other; }
    T multiply(T other) { return value * other; }
};

void test_concepts() {
    ArithmeticTemplate<int> intArith;
    ArithmeticTemplate<double> doubleArith;

    intArith.value = 10;
    doubleArith.value = 3.14;

    auto intResult = intArith.add(5);
    auto doubleResult = doubleArith.multiply(2.0);
}
#endif

// Test template with default arguments
template<typename T = int, int N = 10>
class DefaultTemplate {
private:
    T data[N];
public:
    void fill(T value) {
        for (int i = 0; i < N; ++i) {
            data[i] = value;
        }
    }
};

void test_default_arguments() {
    DefaultTemplate<> defaultInt;        // Uses int, 10
    DefaultTemplate<double> defaultDouble; // Uses double, 10
    DefaultTemplate<char, 5> charFive;    // Uses char, 5

    defaultInt.fill(42);
    defaultDouble.fill(3.14);
    charFive.fill('A');
}

// Test variadic templates
template<typename... Args>
class VariadicTemplate {
public:
    static constexpr size_t count = sizeof...(Args);

    template<typename T>
    void process(T&& value) {
        // Process single value
    }

    template<typename T, typename... Rest>
    void process(T&& first, Rest&&... rest) {
        process(first);
        if constexpr (sizeof...(rest) > 0) {
            process(rest...);
        }
    }
};

void test_variadic_templates() {
    VariadicTemplate<int, double, char> triple;
    VariadicTemplate<int> single;
    VariadicTemplate<> empty;

    triple.process(42, 3.14, 'A');
    single.process(100);
}

// expected-no-diagnostics