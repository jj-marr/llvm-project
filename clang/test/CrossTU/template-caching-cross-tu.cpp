// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: echo 'template<typename T> struct SharedTemplate { T getValue() const { return value; } T value; };' > %t/shared.h
// RUN: echo '#include "%t/shared.h"\nvoid use_int() { SharedTemplate<int> t; t.value = 42; int val = t.getValue(); }' > %t/first.cpp
// RUN: echo '#include "%t/shared.h"\nvoid use_int_again() { SharedTemplate<int> t; t.value = 100; int val = t.getValue(); }' > %t/second.cpp
// RUN: %clang_cc1 -std=c++14 -ftemplate-caching -ftemplate-cache-path=%t %t/first.cpp -verify
// RUN: %clang_cc1 -std=c++14 -ftemplate-caching -ftemplate-cache-path=%t %t/second.cpp -verify

// Test cross-translation unit template caching
// This file is just a placeholder for the test script above
int main() {
  return 0;
}
