// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: %clang_cc1 -std=c++14 -ftemplate-caching -ftemplate-cache-path=%t -ftemplate-cache-prefix=test- %s -verify
// RUN: ls %t | grep test- | count 2

// Test command-line options for template caching

template<typename T>
struct TestTemplate {
  T value;

  T getValue() const {
    return value;
  }
};

// This should create cache files with the specified prefix
void test_template_int() {
  TestTemplate<int> t;
  t.value = 42;
  int val = t.getValue();
  (void)val;
}

// This should create another cache file
void test_template_float() {
  TestTemplate<float> t;
  t.value = 3.14f;
  float val = t.getValue();
  (void)val;
}

int main() {
  test_template_int();
  test_template_float();
  return 0;
}
