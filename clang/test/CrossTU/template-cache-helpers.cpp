// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: echo 'c:@ST>1#T@HelperTemplate %t/template_helpers_tu1.cpp.ast' > %t/template_index.txt
// RUN: echo 'c:@FT@>1#T@helper_function %t/template_helpers_tu2.cpp.ast' >> %t/template_index.txt
// RUN: cp %S/Inputs/template_helpers.h %t/
// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -std=c++20 -I%t -emit-ast -o %t/template_helpers_tu1.cpp.ast %s -DCOMPILE_TU1
// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -std=c++20 -I%t -emit-ast -o %t/template_helpers_tu2.cpp.ast %s -DCOMPILE_TU2
// RUN: %clang_cc1 -triple x86_64-pc-linux-gnu -std=c++20 -I%t -fsyntax-only -fcrosstu-dir=%t -fcrosstu-index-name=template_index.txt -verify %s -DCOMPILE_MAIN

#include "template_helpers.h"

#ifdef COMPILE_TU1
// First translation unit - instantiate some templates
void test_tu1() {
    HelperTemplate<int> int_helper(42);
    HelperTemplate<double> double_helper(3.14);

    int_helper.increment_usage();
    double_helper.set_value(2.71);

    auto combined1 = int_helper.combine_with(10);
    auto combined2 = double_helper.combine_with(1.0);

    ArrayHelper<int, 10> int_array;
    ArrayHelper<char, 5> char_array;

    int_array.add(1);
    int_array.add(2);
    int_array.add(3);

    char_array.add('A');
    char_array.add('B');

    auto int_size = int_array.size();
    auto char_capacity = char_array.capacity();
}

// Explicit instantiations for TU1
template class HelperTemplate<int>;
template class HelperTemplate<double>;
template class ArrayHelper<int, 10>;
template class ArrayHelper<char, 5>;

template int helper_function<int>(int);
template double helper_function<double>(double);

template const int helper_default_value<int>;
template const double helper_default_value<double>;

#endif // COMPILE_TU1

#ifdef COMPILE_TU2
// Second translation unit - different instantiations
void test_tu2() {
    HelperTemplate<float> float_helper(2.5f);
    HelperTemplate<long> long_helper(1000L);

    float_helper.set_value(3.5f);
    long_helper.increment_usage();

    auto combined1 = float_helper.combine_with(1.5f);
    auto combined2 = long_helper.combine_with(500L);

    ArrayHelper<double, 8> double_array;
    ArrayHelper<short, 15> short_array;

    double_array.add(1.1);
    double_array.add(2.2);

    short_array.add(100);
    short_array.add(200);
    short_array.add(300);

    auto double_empty = double_array.empty();
    auto short_full = short_array.full();

    // Test helper functions
    auto float_result = helper_function<float>(5.5f);
    auto long_result = helper_function<long>(2000L);

    // Test helper variables
    auto& float_default = helper_default_value<float>;
    auto& long_default = helper_default_value<long>;
}

// Explicit instantiations for TU2
template class HelperTemplate<float>;
template class HelperTemplate<long>;
template class ArrayHelper<double, 8>;
template class ArrayHelper<short, 15>;

template float helper_function<float>(float);
template long helper_function<long>(long);

template const float helper_default_value<float>;
template const long helper_default_value<long>;

#endif // COMPILE_TU2

#ifdef COMPILE_MAIN
// Main translation unit - should use cached templates
void test_main() {
    // These should use cached instantiations from TU1 and TU2
    HelperTemplate<int> cached_int(999);        // From TU1
    HelperTemplate<float> cached_float(9.99f);  // From TU2
    HelperTemplate<char> new_char('Z');         // New instantiation

    cached_int.increment_usage();
    cached_float.set_value(8.88f);
    new_char.increment_usage();

    auto int_combined = cached_int.combine_with(111);
    auto float_combined = cached_float.combine_with(1.11f);
    auto char_combined = new_char.combine_with(static_cast<char>(1));

    // Test cached array helpers
    ArrayHelper<int, 10> cached_int_array;      // From TU1
    ArrayHelper<double, 8> cached_double_array; // From TU2
    ArrayHelper<bool, 3> new_bool_array;        // New instantiation

    cached_int_array.add(777);
    cached_int_array.add(888);

    cached_double_array.add(7.77);
    cached_double_array.add(8.88);

    new_bool_array.add(true);
    new_bool_array.add(false);

    // Test iteration
    for (auto& item : cached_int_array) {
        item *= 2;
    }

    for (const auto& item : cached_double_array) {
        auto temp = item + 1.0;
    }

    // Test cached function templates
    auto cached_int_func = helper_function<int>(555);      // From TU1
    auto cached_float_func = helper_function<float>(6.66f); // From TU2
    auto new_char_func = helper_function<char>('X');        // New instantiation

    // Test cached variable templates
    auto& cached_int_var = helper_default_value<int>;      // From TU1
    auto& cached_float_var = helper_default_value<float>;  // From TU2
    auto& new_char_var = helper_default_value<char>;       // New instantiation

    // Verify values
    static_assert(helper_default_value<int> == 42);
    static_assert(helper_default_value<double> == 3.14159);
}

// Test overlapping instantiations
void test_overlapping() {
    // These should reuse cached instantiations
    HelperTemplate<int> overlap1(123);    // Should use TU1 cache
    HelperTemplate<double> overlap2(4.56); // Should use TU1 cache
    HelperTemplate<float> overlap3(7.89f); // Should use TU2 cache

    overlap1.set_value(456);
    overlap2.increment_usage();
    overlap3.set_value(1.23f);

    ArrayHelper<int, 10> overlap_array1;   // Should use TU1 cache
    ArrayHelper<char, 5> overlap_array2;   // Should use TU1 cache
    ArrayHelper<double, 8> overlap_array3; // Should use TU2 cache

    overlap_array1.add(11);
    overlap_array1.add(22);

    overlap_array2.add('P');
    overlap_array2.add('Q');

    overlap_array3.add(1.11);
    overlap_array3.add(2.22);

    // Test that methods work correctly on cached templates
    auto size1 = overlap_array1.size();
    auto capacity2 = overlap_array2.capacity();
    auto empty3 = overlap_array3.empty();

    // Test element access
    if (!overlap_array1.empty()) {
        auto first = overlap_array1[0];
    }

    if (!overlap_array2.empty()) {
        auto first = overlap_array2[0];
    }

    if (!overlap_array3.empty()) {
        auto first = overlap_array3[0];
    }
}

// Test template method instantiation caching
void test_template_methods() {
    HelperTemplate<int> method_test(100);

    // Test template method with different types
    auto result1 = method_test.combine_with(50);      // int + int
    auto result2 = method_test.combine_with(2.5);     // int + double
    auto result3 = method_test.combine_with(1.5f);    // int + float

    // These template method instantiations should also be cached
    HelperTemplate<double> method_test2(5.5);
    auto result4 = method_test2.combine_with(1.1);    // double + double
    auto result5 = method_test2.combine_with(2);      // double + int
}

// Test that new instantiations work alongside cached ones
void test_mixed_instantiations() {
    // Mix of cached and new instantiations
    HelperTemplate<int> cached(1);           // Cached from TU1
    HelperTemplate<unsigned int> new_inst(2u); // New instantiation

    cached.increment_usage();
    new_inst.increment_usage();

    auto cached_val = cached.get_value();
    auto new_val = new_inst.get_value();

    // Test that both work correctly
    cached.set_value(10);
    new_inst.set_value(20u);

    ArrayHelper<int, 10> cached_array;        // Cached from TU1
    ArrayHelper<unsigned int, 10> new_array;  // New instantiation

    cached_array.add(1);
    new_array.add(2u);

    auto cached_size = cached_array.size();
    auto new_size = new_array.size();
}

#endif // COMPILE_MAIN

// expected-no-diagnostics