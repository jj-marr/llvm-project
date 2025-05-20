// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: %clang_cc1 -std=c++14 -ftemplate-caching -ftemplate-cache-path=%t %s -verify

// Test handling of template dependencies

// Base template
template<typename T>
struct BaseTemplate {
  T value;

  T getValue() const {
    return value;
  }
};

// Derived template that depends on BaseTemplate
template<typename T>
struct DerivedTemplate : public BaseTemplate<T> {
  T getDoubleValue() const {
    return this->getValue() * 2;
  }
};

// Template that uses another template as a member
template<typename T>
struct ContainerTemplate {
  BaseTemplate<T> base;

  T getValueFromBase() const {
    return base.getValue();
  }
};

// Template with nested template parameter
template<typename T>
struct OuterTemplate {
  template<typename U>
  struct InnerTemplate {
    T outerValue;
    U innerValue;

    T getOuterValue() const {
      return outerValue;
    }

    U getInnerValue() const {
      return innerValue;
    }
  };

  InnerTemplate<int> inner;
};

// Test all templates
void test_dependencies() {
  // Test BaseTemplate
  BaseTemplate<int> base;
  base.value = 42;
  int baseVal = base.getValue();
  (void)baseVal;

  // Test DerivedTemplate
  DerivedTemplate<int> derived;
  derived.value = 42;
  int derivedVal = derived.getDoubleValue();
  (void)derivedVal;

  // Test ContainerTemplate
  ContainerTemplate<int> container;
  container.base.value = 42;
  int containerVal = container.getValueFromBase();
  (void)containerVal;

  // Test OuterTemplate with InnerTemplate
  OuterTemplate<float> outer;
  outer.inner.outerValue = 3.14f;
  outer.inner.innerValue = 42;
  float outerVal = outer.inner.getOuterValue();
  int innerVal = outer.inner.getInnerValue();
  (void)outerVal;
  (void)innerVal;
}

int main() {
  test_dependencies();
  return 0;
}
