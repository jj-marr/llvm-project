// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -std=c++20 -fsyntax-only -verify %s

// Test template specialization caching functionality

// Define size_t for the test
using size_t = unsigned long;

// Define basic string class for the test
class string {
public:
    const char* data_;
    size_t size_;

    string() : data_(""), size_(0) {}
    string(const char* s) : data_(s), size_(0) {
        while (s[size_]) ++size_;
    }

    const char* c_str() const { return data_; }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
};

// Define type traits for the test
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

    using string = ::string;

    template<typename T> struct numeric_limits {
        static constexpr T max() { return T{}; }
        static constexpr T min() { return T{}; }
    };
    template<> struct numeric_limits<int> {
        static constexpr int max() { return 2147483647; }
        static constexpr int min() { return -2147483648; }
    };
    template<> struct numeric_limits<double> {
        static constexpr double max() { return 1.7976931348623157e+308; }
        static constexpr double min() { return 2.2250738585072014e-308; }
    };
}

// Primary template
template<typename T>
class SpecializedTemplate {
public:
    T value;
    void process() {
        // Generic implementation
    }

    static constexpr const char* getTypeName() {
        return "generic";
    }
};

// Full specialization for int
template<>
class SpecializedTemplate<int> {
public:
    int value;
    void process() {
        // Specialized implementation for int
        value *= 2;
    }

    static constexpr const char* getTypeName() {
        return "int";
    }

    // Additional method only for int specialization
    void increment() {
        ++value;
    }
};

// Full specialization for double
template<>
class SpecializedTemplate<double> {
public:
    double value;
    void process() {
        // Specialized implementation for double
        value += 1.0;
    }

    static constexpr const char* getTypeName() {
        return "double";
    }

    // Additional method only for double specialization
    void normalize() {
        if (value > 1.0) value = 1.0;
        if (value < 0.0) value = 0.0;
    }
};

// Partial specialization for pointer types
template<typename T>
class SpecializedTemplate<T*> {
public:
    T* value;
    void process() {
        // Specialized implementation for pointers
        if (value) {
            // Process pointed-to value
        }
    }

    static constexpr const char* getTypeName() {
        return "pointer";
    }

    bool isNull() const {
        return value == nullptr;
    }
};

// Partial specialization for const types
template<typename T>
class SpecializedTemplate<const T> {
public:
    const T value;

    SpecializedTemplate(const T& v) : value(v) {}

    void process() {
        // Specialized implementation for const types
        // Cannot modify value
    }

    static constexpr const char* getTypeName() {
        return "const";
    }

    const T& getValue() const {
        return value;
    }
};

// Test function template specializations
template<typename T>
void processValue(T value) {
    // Generic implementation
    SpecializedTemplate<T> processor;
    processor.value = value;
    processor.process();
}

// Function template specialization for bool
template<>
void processValue<bool>(bool value) {
    // Specialized implementation for bool
    SpecializedTemplate<bool> processor;
    processor.value = value;
    if (value) {
        processor.process();
    }
}

// Function template specialization for const char*
template<>
void processValue<const char*>(const char* value) {
    // Specialized implementation for strings
    if (value && *value) {
        SpecializedTemplate<const char*> processor;
        processor.value = value;
        processor.process();
    }
}

// Test variable template specializations
template<typename T>
constexpr T defaultValue = T{};

template<>
constexpr int defaultValue<int> = 42;

template<>
constexpr double defaultValue<double> = 3.14159;

template<>
constexpr const char* defaultValue<const char*> = "default";

// Test partial specialization with multiple template parameters
template<typename T, typename U>
class TwoParamTemplate {
public:
    T first;
    U second;

    void combine() {
        // Generic implementation
    }

    static constexpr const char* getSpecialization() {
        return "generic";
    }
};

// Partial specialization when both types are the same
template<typename T>
class TwoParamTemplate<T, T> {
public:
    T first;
    T second;

    void combine() {
        // Specialized implementation for same types
        first = second;
    }

    static constexpr const char* getSpecialization() {
        return "same_types";
    }

    bool areEqual() const {
        return first == second;
    }
};

// Partial specialization for pointer and value
template<typename T>
class TwoParamTemplate<T*, T> {
public:
    T* first;
    T second;

    void combine() {
        // Specialized implementation for pointer and value
        if (first) {
            *first = second;
        }
    }

    static constexpr const char* getSpecialization() {
        return "pointer_value";
    }
};

// Test SFINAE-based specialization
template<typename T, typename Enable = void>
class SFINAETemplate {
public:
    static constexpr bool isIntegral = false;
    T value;
};

template<typename T>
class SFINAETemplate<T, typename std::enable_if_t<std::is_integral_v<T>>> {
public:
    static constexpr bool isIntegral = true;
    T value;

    void increment() {
        ++value;
    }
};

template<typename T>
class SFINAETemplate<T, typename std::enable_if_t<std::is_floating_point_v<T>>> {
public:
    static constexpr bool isIntegral = false;
    T value;

    void addEpsilon() {
        value += std::numeric_limits<T>::epsilon();
    }
};

// Test usage of all specializations
void test_specializations() {
    // Test class template specializations
    SpecializedTemplate<int> intSpec;
    SpecializedTemplate<double> doubleSpec;
    SpecializedTemplate<char> charSpec;
    SpecializedTemplate<int*> intPtrSpec;
    SpecializedTemplate<const float> constFloatSpec(3.14f);

    intSpec.value = 10;
    intSpec.process();
    intSpec.increment();

    doubleSpec.value = 2.5;
    doubleSpec.process();
    doubleSpec.normalize();

    charSpec.value = 'A';
    charSpec.process();

    int x = 42;
    intPtrSpec.value = &x;
    intPtrSpec.process();
    bool isNull = intPtrSpec.isNull();

    constFloatSpec.process();
    auto constValue = constFloatSpec.getValue();

    // Test function template specializations
    processValue<int>(100);
    processValue<bool>(true);
    processValue<const char*>("test");
    processValue<float>(2.5f);

    // Test variable template specializations
    auto intDefault = defaultValue<int>;
    auto doubleDefault = defaultValue<double>;
    auto stringDefault = defaultValue<const char*>;
    auto floatDefault = defaultValue<float>;

    // Test two-parameter template specializations
    TwoParamTemplate<int, double> intDouble;
    TwoParamTemplate<float, float> floatFloat;
    TwoParamTemplate<int*, int> ptrInt;

    intDouble.combine();
    floatFloat.combine();
    bool equal = floatFloat.areEqual();
    ptrInt.combine();

    // Test SFINAE specializations
    SFINAETemplate<int> intSFINAE;
    SFINAETemplate<double> doubleSFINAE;
    SFINAETemplate<std::string> stringSFINAE;

    static_assert(SFINAETemplate<int>::isIntegral);
    static_assert(!SFINAETemplate<double>::isIntegral);
    static_assert(!SFINAETemplate<std::string>::isIntegral);
}

// Test explicit instantiation of specializations
template class SpecializedTemplate<long>;
template class SpecializedTemplate<long*>;
template class TwoParamTemplate<int, int>;
template void processValue<short>(short);

// Test explicit instantiation declaration
extern template class SpecializedTemplate<unsigned int>;
extern template void processValue<unsigned long>(unsigned long);

// expected-no-diagnostics