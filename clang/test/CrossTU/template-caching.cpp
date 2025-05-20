// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: %clang_cc1 -std=c++14 -ftemplate-caching -ftemplate-cache-path=%t %s -verify

// Test basic template caching functionality

template<typename T>
struct BasicTemplate {
  T value;

  T getValue() const {
    return value;
  }
};

// First instantiation should be cached
void test_basic_int() {
  BasicTemplate<int> t;
  t.value = 42;
  int val = t.getValue();
  (void)val;
}

// This should use the cached instantiation
void test_basic_int_again() {
  BasicTemplate<int> t;
  t.value = 100;
  int val = t.getValue();
  (void)val;
}

// Different template parameter should create a new instantiation
void test_basic_float() {
  BasicTemplate<float> t;
  t.value = 3.14f;
  float val = t.getValue();
  (void)val;
}

// Test with a more complex template

template<typename T, int N>
struct ComplexTemplate {
  T data[N];

  T sum() const {
    T result = T();
    for (int i = 0; i < N; ++i) {
      result += data[i];
    }
    return result;
  }
};

// First instantiation should be cached
void test_complex() {
  ComplexTemplate<int, 3> t;
  t.data[0] = 1;
  t.data[1] = 2;
  t.data[2] = 3;
  int val = t.sum();
  (void)val;
}

// This should use the cached instantiation
void test_complex_again() {
  ComplexTemplate<int, 3> t;
  t.data[0] = 4;
  t.data[1] = 5;
  t.data[2] = 6;
  int val = t.sum();
  (void)val;
}

// Different template parameter should create a new instantiation
void test_complex_different() {
  ComplexTemplate<int, 4> t;
  t.data[0] = 1;
  t.data[1] = 2;
  t.data[2] = 3;
  t.data[3] = 4;
  int val = t.sum();
  (void)val;
}

int main() {
  test_basic_int();
  test_basic_int_again();
  test_basic_float();
  test_complex();
  test_complex_again();
  test_complex_different();
  return 0;
}
