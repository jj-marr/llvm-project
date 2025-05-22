# Template Cache Testing Framework

This directory contains comprehensive tests for the template caching functionality in Clang's Cross-Translation Unit (CTU) analysis.

## Test Files Overview

### Unit Tests
- **`clang/unittests/CrossTU/TemplateCacheTest.cpp`** - Unit tests for template cache components
  - Tests `TemplateIdentifier` functionality
  - Tests `TemplateUSRGenerator` operations
  - Tests `TemplateInstantiationInfo` handling
  - Tests error handling and edge cases
  - Tests cache index parsing and creation

### Integration Tests

#### Basic Functionality
- **`template-cache-basic.cpp`** - Basic template caching functionality
  - Simple template instantiations
  - Template with multiple parameters
  - Nested template instantiation
  - Template with default arguments
  - Variadic templates
  - C++20 concepts (when available)

#### Specialization Testing
- **`template-cache-specializations.cpp`** - Template specialization caching
  - Full class template specializations
  - Partial template specializations
  - Function template specializations
  - Variable template specializations
  - SFINAE-based specializations
  - Complex multi-parameter specializations

#### Cross-TU Testing
- **`template-cache-cross-tu.cpp`** - Cross-translation unit template caching
  - Multi-file template sharing using `split-file`
  - Template instantiation across different TUs
  - Cache hit validation across TUs
  - Overlapping instantiations between TUs
  - Complex template dependencies

#### Performance Testing
- **`template-cache-performance.cpp`** - Performance and statistics validation
  - Cache hit rate measurements
  - Complex template instantiation patterns
  - Recursive template instantiations
  - Template metaprogramming constructs
  - Stress testing with many instantiations
  - Statistics collection validation

#### Error Handling
- **`template-cache-errors.cpp`** - Error handling and edge cases
  - Invalid cache files
  - Malformed index files
  - Template instantiation errors
  - SFINAE failure handling
  - Recursive template limits
  - Cache invalidation scenarios

#### Helper-based Testing
- **`template-cache-helpers.cpp`** - Multi-TU testing with shared helpers
  - Uses `Inputs/template_helpers.h` for shared definitions
  - Tests template caching across multiple compilation units
  - Validates cache reuse and new instantiations

### Helper Files
- **`Inputs/template_helpers.h`** - Shared template definitions for multi-TU tests
  - Common template classes and functions
  - Template specializations
  - Complex template patterns for testing

## Test Execution

### Running Unit Tests
```bash
# Build and run unit tests
ninja CrossTUTests
./tools/clang/unittests/CrossTU/CrossTUTests
```

### Running Integration Tests
```bash
# Run all template cache tests
llvm-lit clang/test/CrossTU/template-cache-*.cpp

# Run specific test
llvm-lit clang/test/CrossTU/template-cache-basic.cpp

# Run with verbose output
llvm-lit -v clang/test/CrossTU/template-cache-basic.cpp
```

### Test Configuration
Tests use the following RUN commands pattern:
```bash
# Clean test directory
RUN: rm -rf %t
RUN: mkdir -p %t

# Create template index
RUN: echo 'template_usr template_file.ast' > %t/template_index.txt

# Generate AST files
RUN: %clang_cc1 -emit-ast -o %t/template_file.ast %s

# Run test with template caching
RUN: %clang_cc1 -fsyntax-only -fcrosstu-dir=%t -fcrosstu-index-name=template_index.txt -verify %s
```

## Test Categories

### 1. Correctness Tests
- Verify template instantiations are cached correctly
- Ensure cached templates behave identically to non-cached
- Validate template specialization caching
- Test cross-TU template sharing

### 2. Performance Tests
- Measure cache hit rates
- Validate compilation time improvements
- Test memory usage with caching
- Stress test with many instantiations

### 3. Error Handling Tests
- Invalid cache file handling
- Malformed index file recovery
- Template instantiation error handling
- Cache corruption detection

### 4. Edge Case Tests
- Recursive template instantiations
- Circular template dependencies
- Template argument deduction edge cases
- SFINAE and concept constraint failures

### 5. Integration Tests
- Sema integration with template cache
- ASTImporter integration for cached templates
- CrossTU context integration
- Multi-file template caching scenarios

## Expected Test Coverage

### API Coverage
- All public APIs of `TemplateInstantiationCache`
- All methods of `TemplateUSRGenerator`
- All operations of `TemplateASTUnitStorage`
- Error handling through `TemplateCacheError`

### Scenario Coverage
- Basic template instantiation caching
- Template specialization caching
- Cross-TU template sharing
- Cache invalidation and cleanup
- Performance characteristics validation

### Error Path Coverage
- Invalid USR handling
- Template instantiation failures
- Cache file corruption
- Index file parsing errors
- Dependency change detection

## Test Validation

### Positive Tests
- Template caching works correctly
- Cache hits improve performance
- Cross-TU sharing functions properly
- Statistics are accurate

### Negative Tests
- Invalid inputs are handled gracefully
- Error conditions don't crash
- Cache corruption is detected
- Recovery mechanisms work

### Performance Tests
- Cache hit rates meet expectations
- Compilation time improvements are measurable
- Memory usage is reasonable
- Statistics collection is accurate

## Debugging Tests

### Verbose Output
Use `-v` flag with `llvm-lit` for detailed test output:
```bash
llvm-lit -v clang/test/CrossTU/template-cache-basic.cpp
```

### Test-specific Debugging
Add `-ftemplate-cache-stats` to see cache statistics:
```bash
%clang_cc1 -ftemplate-cache-stats -fcrosstu-dir=%t -verify %s
```

### Manual Test Execution
Run individual RUN commands manually for debugging:
```bash
mkdir -p /tmp/test
echo 'template_usr file.ast' > /tmp/test/index.txt
clang -emit-ast -o /tmp/test/file.ast test.cpp
clang -fsyntax-only -fcrosstu-dir=/tmp/test -fcrosstu-index-name=index.txt test.cpp
```

## Test Maintenance

### Adding New Tests
1. Create test file following naming convention: `template-cache-<category>.cpp`
2. Use appropriate RUN commands for test setup
3. Include both positive and negative test cases
4. Add `expected-no-diagnostics` or specific diagnostic expectations
5. Update this README with test description

### Updating Existing Tests
1. Maintain backward compatibility when possible
2. Update expected diagnostics if API changes
3. Ensure test coverage remains comprehensive
4. Update documentation for significant changes

### Test Dependencies
- Tests depend on template cache implementation in `clang/lib/CrossTU/TemplateCache.cpp`
- Tests use Clang's lit testing framework
- Some tests require C++20 support for concepts
- Multi-TU tests use `split-file` utility

## Known Limitations

### Platform Dependencies
- File system path handling may vary between platforms
- Some tests may be sensitive to file system case sensitivity

### Compiler Dependencies
- C++20 concept tests require appropriate compiler support
- Template instantiation limits may vary between configurations

### Test Environment
- Tests require write access to temporary directories
- Some tests generate large numbers of template instantiations
- Performance tests may be sensitive to system load