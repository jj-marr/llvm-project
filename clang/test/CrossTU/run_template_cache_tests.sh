#!/bin/bash

# Template Cache Test Runner
# This script runs all template caching tests and provides a summary

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LLVM_BUILD_DIR="${LLVM_BUILD_DIR:-build}"
LIT_TOOL="${LIT_TOOL:-llvm-lit}"

echo -e "${BLUE}Template Cache Testing Framework${NC}"
echo "=================================="
echo

# Function to run a test and capture results
run_test() {
    local test_name="$1"
    local test_file="$2"

    echo -n "Running $test_name... "

    if $LIT_TOOL -v "$test_file" > "/tmp/test_${test_name}.log" 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        echo "  Log: /tmp/test_${test_name}.log"
        return 1
    fi
}

# Function to run unit tests
run_unit_tests() {
    echo -e "${YELLOW}Running Unit Tests${NC}"
    echo "==================="

    if [ -f "$LLVM_BUILD_DIR/tools/clang/unittests/CrossTU/CrossTUTests" ]; then
        echo -n "Running CrossTU unit tests... "
        if "$LLVM_BUILD_DIR/tools/clang/unittests/CrossTU/CrossTUTests" --gtest_filter="*TemplateCache*" > /tmp/unit_tests.log 2>&1; then
            echo -e "${GREEN}PASS${NC}"
            unit_tests_passed=1
        else
            echo -e "${RED}FAIL${NC}"
            echo "  Log: /tmp/unit_tests.log"
            unit_tests_passed=0
        fi
    else
        echo -e "${YELLOW}Unit tests not built${NC}"
        echo "  Run: ninja CrossTUTests"
        unit_tests_passed=0
    fi
    echo
}

# Function to run integration tests
run_integration_tests() {
    echo -e "${YELLOW}Running Integration Tests${NC}"
    echo "========================="

    local tests_passed=0
    local tests_total=0

    # List of integration tests
    declare -a tests=(
        "basic:template-cache-basic.cpp"
        "specializations:template-cache-specializations.cpp"
        "cross-tu:template-cache-cross-tu.cpp"
        "performance:template-cache-performance.cpp"
        "errors:template-cache-errors.cpp"
        "helpers:template-cache-helpers.cpp"
    )

    for test_entry in "${tests[@]}"; do
        IFS=':' read -r test_name test_file <<< "$test_entry"
        tests_total=$((tests_total + 1))

        if run_test "$test_name" "$SCRIPT_DIR/$test_file"; then
            tests_passed=$((tests_passed + 1))
        fi
    done

    echo
    echo "Integration Tests: $tests_passed/$tests_total passed"
    echo

    return $((tests_total - tests_passed))
}

# Function to check prerequisites
check_prerequisites() {
    echo -e "${YELLOW}Checking Prerequisites${NC}"
    echo "======================"

    # Check for lit tool
    if ! command -v $LIT_TOOL &> /dev/null; then
        echo -e "${RED}Error: $LIT_TOOL not found${NC}"
        echo "Please ensure LLVM is built and $LIT_TOOL is in PATH"
        exit 1
    fi

    # Check for clang
    if ! command -v clang &> /dev/null; then
        echo -e "${RED}Error: clang not found${NC}"
        echo "Please ensure clang is built and in PATH"
        exit 1
    fi

    echo -e "${GREEN}Prerequisites OK${NC}"
    echo
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  -h, --help          Show this help message"
    echo "  -u, --unit-only     Run only unit tests"
    echo "  -i, --integration-only  Run only integration tests"
    echo "  -v, --verbose       Show verbose output"
    echo "  --build-dir DIR     Specify LLVM build directory (default: build)"
    echo "  --lit-tool TOOL     Specify lit tool path (default: llvm-lit)"
    echo
    echo "Environment Variables:"
    echo "  LLVM_BUILD_DIR      LLVM build directory"
    echo "  LIT_TOOL           Path to lit tool"
    echo
}

# Parse command line arguments
unit_only=0
integration_only=0
verbose=0

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_usage
            exit 0
            ;;
        -u|--unit-only)
            unit_only=1
            shift
            ;;
        -i|--integration-only)
            integration_only=1
            shift
            ;;
        -v|--verbose)
            verbose=1
            shift
            ;;
        --build-dir)
            LLVM_BUILD_DIR="$2"
            shift 2
            ;;
        --lit-tool)
            LIT_TOOL="$2"
            shift 2
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            show_usage
            exit 1
            ;;
    esac
done

# Main execution
main() {
    local total_failures=0

    check_prerequisites

    if [[ $integration_only -eq 0 ]]; then
        run_unit_tests
        if [[ $unit_tests_passed -eq 0 ]]; then
            total_failures=$((total_failures + 1))
        fi
    fi

    if [[ $unit_only -eq 0 ]]; then
        run_integration_tests
        integration_failures=$?
        total_failures=$((total_failures + integration_failures))
    fi

    # Summary
    echo -e "${BLUE}Test Summary${NC}"
    echo "============"

    if [[ $total_failures -eq 0 ]]; then
        echo -e "${GREEN}All tests passed!${NC}"
        echo
        echo "Template caching functionality is working correctly."
    else
        echo -e "${RED}$total_failures test(s) failed${NC}"
        echo
        echo "Check the log files in /tmp/ for detailed error information:"
        ls -la /tmp/test_*.log /tmp/unit_tests.log 2>/dev/null || true
    fi

    return $total_failures
}

# Run main function
main "$@"