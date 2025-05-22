//===--- template_helpers.h - Helper templates for testing -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef TEMPLATE_HELPERS_H
#define TEMPLATE_HELPERS_H

#include <cstddef>
#include <type_traits>

// Helper template for cross-TU testing
template<typename T>
class HelperTemplate {
public:
    T value;
    size_t usage_count;

    HelperTemplate() : value{}, usage_count(0) {}
    explicit HelperTemplate(const T& v) : value(v), usage_count(1) {}

    void increment_usage() { ++usage_count; }
    T get_value() const { return value; }
    void set_value(const T& v) { value = v; ++usage_count; }

    template<typename U>
    auto combine_with(const U& other) -> decltype(value + other) {
        increment_usage();
        return value + other;
    }
};

// Helper function template
template<typename T>
T helper_function(T input) {
    HelperTemplate<T> helper(input);
    helper.increment_usage();
    return helper.get_value();
}

// Helper variable template
template<typename T>
constexpr T helper_default_value = T{};

// Specializations
template<>
constexpr int helper_default_value<int> = 42;

template<>
constexpr double helper_default_value<double> = 3.14159;

// Complex helper template
template<typename T, size_t N>
class ArrayHelper {
private:
    T data[N];
    size_t size_;

public:
    ArrayHelper() : size_(0) {}

    bool add(const T& item) {
        if (size_ < N) {
            data[size_++] = item;
            return true;
        }
        return false;
    }

    T* begin() { return data; }
    T* end() { return data + size_; }
    const T* begin() const { return data; }
    const T* end() const { return data + size_; }

    size_t size() const { return size_; }
    size_t capacity() const { return N; }
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == N; }

    T& operator[](size_t index) { return data[index]; }
    const T& operator[](size_t index) const { return data[index]; }
};

#endif // TEMPLATE_HELPERS_H