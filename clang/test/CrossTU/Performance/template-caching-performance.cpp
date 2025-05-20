// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: %clang_cc1 -std=c++14 %s -o %t/without_cache.o -time-passes 2>&1 | FileCheck %s --check-prefix=WITHOUT-CACHE
// RUN: %clang_cc1 -std=c++14 -ftemplate-caching -ftemplate-cache-path=%t %s -o %t/with_cache_first.o -time-passes 2>&1 | FileCheck %s --check-prefix=WITH-CACHE-FIRST
// RUN: %clang_cc1 -std=c++14 -ftemplate-caching -ftemplate-cache-path=%t %s -o %t/with_cache_second.o -time-passes 2>&1 | FileCheck %s --check-prefix=WITH-CACHE-SECOND

// WITHOUT-CACHE: CodeGen
// WITH-CACHE-FIRST: CodeGen
// WITH-CACHE-SECOND: CodeGen

// Test performance impact of template caching

// Define a complex template with many instantiations
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

  T product() const {
    T result = T(1);
    for (int i = 0; i < N; ++i) {
      result *= data[i];
    }
    return result;
  }

  T average() const {
    return sum() / N;
  }

  T max() const {
    T result = data[0];
    for (int i = 1; i < N; ++i) {
      if (data[i] > result) {
        result = data[i];
      }
    }
    return result;
  }

  T min() const {
    T result = data[0];
    for (int i = 1; i < N; ++i) {
      if (data[i] < result) {
        result = data[i];
      }
    }
    return result;
  }
};

// Create many instantiations of the template
void test_performance() {
  // Integer instantiations
  ComplexTemplate<int, 10> t1;
  ComplexTemplate<int, 20> t2;
  ComplexTemplate<int, 30> t3;
  ComplexTemplate<int, 40> t4;
  ComplexTemplate<int, 50> t5;

  // Float instantiations
  ComplexTemplate<float, 10> t6;
  ComplexTemplate<float, 20> t7;
  ComplexTemplate<float, 30> t8;
  ComplexTemplate<float, 40> t9;
  ComplexTemplate<float, 50> t10;

  // Double instantiations
  ComplexTemplate<double, 10> t11;
  ComplexTemplate<double, 20> t12;
  ComplexTemplate<double, 30> t13;
  ComplexTemplate<double, 40> t14;
  ComplexTemplate<double, 50> t15;

  // Use all instantiations to prevent optimization
  int sum = t1.sum() + t2.sum() + t3.sum() + t4.sum() + t5.sum();
  float fsum = t6.sum() + t7.sum() + t8.sum() + t9.sum() + t10.sum();
  double dsum = t11.sum() + t12.sum() + t13.sum() + t14.sum() + t15.sum();

  (void)sum;
  (void)fsum;
  (void)dsum;
}

int main() {
  test_performance();
  return 0;
}
