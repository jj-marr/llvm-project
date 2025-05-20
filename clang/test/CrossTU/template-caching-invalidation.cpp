// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: echo 'template<typename T> struct TestTemplate { T getValue() const { return 42; } };' > %t/header1.h
// RUN: %clang_cc1 -std=c++14 -ftemplate-caching -ftemplate-cache-path=%t -include %t/header1.h %s -verify
// RUN: echo 'template<typename T> struct TestTemplate { T getValue() const { return 100; } };' > %t/header1.h
// RUN: %clang_cc1 -std=c++14 -ftemplate-caching -ftemplate-cache-path=%t -include %t/header1.h %s -verify

// Test cache invalidation when template definitions change

void test_template_int() {
  TestTemplate<int> t;
  int val = t.getValue();
  (void)val;
}

int main() {
  test_template_int();
  return 0;
}
