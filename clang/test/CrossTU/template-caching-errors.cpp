// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: chmod 000 %t
// RUN: not %clang_cc1 -std=c++14 -ftemplate-caching -ftemplate-cache-path=%t %s 2>&1 | FileCheck %s
// RUN: chmod 755 %t

// CHECK: error: Failed to create cache directory

// Test error handling in template caching system

template<typename T>
struct ErrorTemplate {
  T value;

  T getValue() const {
    return value;
  }
};

void test_error_handling() {
  ErrorTemplate<int> t;
  t.value = 42;
  int val = t.getValue();
  (void)val;
}

int main() {
  test_error_handling();
  return 0;
}
