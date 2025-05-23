// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: split-file %s %t
// RUN: echo 'c:@ST>1#T@SharedTemplate %t/shared_template.cpp.ast' > %t/template_index.txt
// RUN: echo 'c:@FT@>1#T@sharedFunction %t/shared_function.cpp.ast' >> %t/template_index.txt
// RUN: echo 'c:@VT>1#T@sharedVariable %t/shared_variable.cpp.ast' >> %t/template_index.txt
// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -std=c++20 -emit-ast -o %t/shared_template.cpp.ast %t/shared_template.cpp
// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -std=c++20 -emit-ast -o %t/shared_function.cpp.ast %t/shared_function.cpp
// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -std=c++20 -emit-ast -o %t/shared_variable.cpp.ast %t/shared_variable.cpp
// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -std=c++20 -fsyntax-only -fcrosstu-dir=%t -fcrosstu-index-name=template_index.txt -verify %t/main.cpp

//--- shared_template.h
#ifndef SHARED_TEMPLATE_H
#define SHARED_TEMPLATE_H


// Shared template class across translation units
template<typename T>
class SharedTemplate {
public:
    T data;
    size_t count;

    SharedTemplate() : data{}, count(0) {}
    explicit SharedTemplate(const T& value) : data(value), count(1) {}

    void setValue(const T& value) {
        data = value;
        ++count;
    }

    T getValue() const {
        return data;
    }

    size_t getCount() const {
        return count;
    }

    template<typename U>
    auto combine(const U& other) -> decltype(data + other) {
        return data + other;
    }
};

// Shared function template
template<typename T>
void sharedFunction(T value) {
    SharedTemplate<T> instance(value);
    instance.setValue(value);
}

// Shared variable template
template<typename T>
T sharedVariable = T{};

// Template with complex dependencies
template<typename T, size_t N>
class ComplexTemplate {
private:
    T buffer[N];
    size_t current_size;

public:
    ComplexTemplate() : current_size(0) {}

    bool push(const T& item) {
        if (current_size < N) {
            buffer[current_size++] = item;
            return true;
        }
        return false;
    }

    bool pop(T& item) {
        if (current_size > 0) {
            item = buffer[--current_size];
            return true;
        }
        return false;
    }

    size_t size() const { return current_size; }
    size_t capacity() const { return N; }
    bool empty() const { return current_size == 0; }
    bool full() const { return current_size == N; }

    template<typename Predicate>
    size_t count_if(Predicate pred) const {
        size_t count = 0;
        for (size_t i = 0; i < current_size; ++i) {
            if (pred(buffer[i])) {
                ++count;
            }
        }
        return count;
    }
};

// Concept-based template (C++20)
#if __cplusplus >= 202002L
template<typename T>
concept Numeric = std::is_arithmetic_v<T>;

template<Numeric T>
class NumericProcessor {
public:
    T value;

    NumericProcessor(T v) : value(v) {}

    T square() const {
        return value * value;
    }

    T abs() const {
        if constexpr (std::is_signed_v<T>) {
            return value < 0 ? -value : value;
        } else {
            return value;
        }
    }
};
#endif

#endif // SHARED_TEMPLATE_H

//--- shared_template.cpp
#include "shared_template.h"

// Explicit instantiations in first translation unit
template class SharedTemplate<int>;
template class SharedTemplate<double>;
template class SharedTemplate<char>;

template class ComplexTemplate<int, 10>;
template class ComplexTemplate<double, 5>;

template void sharedFunction<int>(int);
template void sharedFunction<double>(double);

template int sharedVariable<int>;
template double sharedVariable<double>;

#if __cplusplus >= 202002L
template class NumericProcessor<int>;
template class NumericProcessor<float>;
#endif

void test_shared_template_tu1() {
    SharedTemplate<int> intTemplate(42);
    SharedTemplate<double> doubleTemplate(3.14);
    SharedTemplate<char> charTemplate('A');

    intTemplate.setValue(100);
    doubleTemplate.setValue(2.71);
    charTemplate.setValue('B');

    auto intResult = intTemplate.combine(5);
    auto doubleResult = doubleTemplate.combine(1.0);

    ComplexTemplate<int, 10> intBuffer;
    intBuffer.push(1);
    intBuffer.push(2);
    intBuffer.push(3);

    auto evenCount = intBuffer.count_if([](int x) { return x % 2 == 0; });

    sharedFunction<int>(200);
    sharedFunction<double>(6.28);

    sharedVariable<int> = 999;
    sharedVariable<double> = 1.414;

#if __cplusplus >= 202002L
    NumericProcessor<int> intProc(10);
    NumericProcessor<float> floatProc(3.5f);

    auto intSquare = intProc.square();
    auto floatAbs = floatProc.abs();
#endif
}

//--- shared_function.cpp
#include "shared_template.h"

// Different instantiations in second translation unit
template class SharedTemplate<long>;
template class SharedTemplate<float>;

template class ComplexTemplate<char, 20>;
template class ComplexTemplate<long, 15>;

template void sharedFunction<long>(long);
template void sharedFunction<float>(float);

template long sharedVariable<long>;
template float sharedVariable<float>;

#if __cplusplus >= 202002L
template class NumericProcessor<double>;
template class NumericProcessor<long>;
#endif

void test_shared_template_tu2() {
    SharedTemplate<long> longTemplate(1000L);
    SharedTemplate<float> floatTemplate(2.5f);

    longTemplate.setValue(2000L);
    floatTemplate.setValue(5.0f);

    auto longResult = longTemplate.combine(500L);
    auto floatResult = floatTemplate.combine(1.5f);

    ComplexTemplate<char, 20> charBuffer;
    charBuffer.push('X');
    charBuffer.push('Y');
    charBuffer.push('Z');

    auto upperCount = charBuffer.count_if([](char c) { return c >= 'A' && c <= 'Z'; });

    sharedFunction<long>(3000L);
    sharedFunction<float>(7.5f);

    sharedVariable<long> = 12345L;
    sharedVariable<float> = 9.99f;

#if __cplusplus >= 202002L
    NumericProcessor<double> doubleProc(15.5);
    NumericProcessor<long> longProc(-25L);

    auto doubleSquare = doubleProc.square();
    auto longAbs = longProc.abs();
#endif
}

//--- shared_variable.cpp
#include "shared_template.h"

// Overlapping instantiations in third translation unit
template class SharedTemplate<int>;      // Same as TU1
template class SharedTemplate<short>;    // New

template class ComplexTemplate<int, 10>; // Same as TU1
template class ComplexTemplate<short, 8>; // New

template void sharedFunction<int>(int);  // Same as TU1
template void sharedFunction<short>(short); // New

template int sharedVariable<int>;        // Same as TU1
template short sharedVariable<short>;    // New

#if __cplusplus >= 202002L
template class NumericProcessor<int>;    // Same as TU1
template class NumericProcessor<short>;  // New
#endif

void test_shared_template_tu3() {
    // Test that cached instantiations work correctly
    SharedTemplate<int> intTemplate(777); // Should use cached version
    SharedTemplate<short> shortTemplate(42);

    intTemplate.setValue(888);
    shortTemplate.setValue(99);

    auto intCombined = intTemplate.combine(111);
    auto shortCombined = shortTemplate.combine(static_cast<short>(11));

    ComplexTemplate<int, 10> intBuffer; // Should use cached version
    ComplexTemplate<short, 8> shortBuffer;

    intBuffer.push(10);
    intBuffer.push(20);
    shortBuffer.push(1);
    shortBuffer.push(2);

    auto intPositiveCount = intBuffer.count_if([](int x) { return x > 0; });
    auto shortEvenCount = shortBuffer.count_if([](short x) { return x % 2 == 0; });

    sharedFunction<int>(555); // Should use cached version
    sharedFunction<short>(77);

    auto& intVar = sharedVariable<int>; // Should use cached version
    auto& shortVar = sharedVariable<short>;

#if __cplusplus >= 202002L
    NumericProcessor<int> intProc(33); // Should use cached version
    NumericProcessor<short> shortProc(7);

    auto intSquare = intProc.square();
    auto shortAbs = shortProc.abs();
#endif
}

//--- main.cpp
#include "shared_template.h"

// Main translation unit that uses cached templates
void test_cross_tu_caching() {
    // These should all use cached instantiations from other TUs
    SharedTemplate<int> cachedInt(123);
    SharedTemplate<double> cachedDouble(4.56);
    SharedTemplate<long> cachedLong(789L);
    SharedTemplate<float> cachedFloat(1.23f);

    cachedInt.setValue(456);
    cachedDouble.setValue(7.89);
    cachedLong.setValue(1011L);
    cachedFloat.setValue(4.56f);

    // Test method calls on cached templates
    auto intValue = cachedInt.getValue();
    auto doubleCount = cachedDouble.getCount();
    auto longCombined = cachedLong.combine(100L);
    auto floatCombined = cachedFloat.combine(0.5f);

    // Test complex templates
    ComplexTemplate<int, 10> cachedIntBuffer;
    ComplexTemplate<double, 5> cachedDoubleBuffer;
    ComplexTemplate<char, 20> cachedCharBuffer;

    cachedIntBuffer.push(1);
    cachedIntBuffer.push(2);
    cachedIntBuffer.push(3);

    cachedDoubleBuffer.push(1.1);
    cachedDoubleBuffer.push(2.2);

    cachedCharBuffer.push('A');
    cachedCharBuffer.push('B');
    cachedCharBuffer.push('C');

    auto intSize = cachedIntBuffer.size();
    auto doubleCapacity = cachedDoubleBuffer.capacity();
    auto charEmpty = cachedCharBuffer.empty();

    // Test function templates
    sharedFunction<int>(999);
    sharedFunction<double>(8.88);
    sharedFunction<long>(2222L);
    sharedFunction<float>(9.99f);

    // Test variable templates
    auto& intVar = sharedVariable<int>;
    auto& doubleVar = sharedVariable<double>;
    auto& longVar = sharedVariable<long>;
    auto& floatVar = sharedVariable<float>;

    intVar = 1111;
    doubleVar = 22.22;
    longVar = 3333L;
    floatVar = 44.44f;

#if __cplusplus >= 202002L
    // Test concept-based templates
    NumericProcessor<int> cachedIntProc(50);
    NumericProcessor<float> cachedFloatProc(7.5f);
    NumericProcessor<double> cachedDoubleProc(12.5);

    auto intSquare = cachedIntProc.square();
    auto floatAbs = cachedFloatProc.abs();
    auto doubleSquare = cachedDoubleProc.square();
#endif
}

// Test template instantiation with different specialization kinds
template class SharedTemplate<unsigned int>; // Explicit instantiation definition
extern template class SharedTemplate<unsigned long>; // Explicit instantiation declaration

template void sharedFunction<unsigned int>(unsigned int);
extern template void sharedFunction<unsigned long>(unsigned long);

template unsigned int sharedVariable<unsigned int>;
extern template unsigned long sharedVariable<unsigned long>;

// expected-no-diagnostics