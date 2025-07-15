#!/bin/bash
# Copyright 2024 The Tungsten Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to run Tungsten Nostr integration tests

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
BUILD_DIR="out/Release"
FILTER="Nostr*"
VERBOSE=false
RUN_BROWSER_TESTS=true
RUN_UNIT_TESTS=true

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --filter)
      FILTER="$2"
      shift 2
      ;;
    --verbose)
      VERBOSE=true
      shift
      ;;
    --unit-only)
      RUN_BROWSER_TESTS=false
      shift
      ;;
    --browser-only)
      RUN_UNIT_TESTS=false
      shift
      ;;
    --help)
      echo "Usage: $0 [options]"
      echo "Options:"
      echo "  --build-dir <dir>    Build directory (default: out/Release)"
      echo "  --filter <pattern>   Test filter pattern (default: Nostr*)"
      echo "  --verbose           Enable verbose output"
      echo "  --unit-only         Run only unit tests"
      echo "  --browser-only      Run only browser tests"
      echo "  --help              Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
  echo -e "${RED}Error: Build directory $BUILD_DIR does not exist${NC}"
  echo "Please build Tungsten first with:"
  echo "  gn gen $BUILD_DIR --args=\"is_official_build=true enable_nostr=true\""
  echo "  autoninja -C $BUILD_DIR chrome"
  exit 1
fi

# Function to run tests
run_test() {
  local test_binary=$1
  local test_name=$2
  local extra_args=$3
  
  if [ ! -f "$BUILD_DIR/$test_binary" ]; then
    echo -e "${YELLOW}Warning: $test_binary not found, skipping${NC}"
    return 0
  fi
  
  echo -e "${GREEN}Running $test_name...${NC}"
  
  if [ "$VERBOSE" = true ]; then
    "$BUILD_DIR/$test_binary" --gtest_filter="$FILTER" $extra_args
  else
    "$BUILD_DIR/$test_binary" --gtest_filter="$FILTER" $extra_args 2>&1 | grep -E "(RUN|OK|FAILED|\[  PASSED  \]|\[  FAILED  \])"
  fi
  
  local exit_code=$?
  if [ $exit_code -ne 0 ]; then
    echo -e "${RED}$test_name failed with exit code $exit_code${NC}"
    return $exit_code
  fi
  
  echo -e "${GREEN}$test_name passed${NC}"
  echo
  return 0
}

# Track overall success
OVERALL_SUCCESS=true

# Run unit tests
if [ "$RUN_UNIT_TESTS" = true ]; then
  echo -e "${GREEN}=== Running Nostr Unit Tests ===${NC}"
  
  # Core Nostr tests
  run_test "unit_tests" "Core Nostr Tests" "" || OVERALL_SUCCESS=false
  
  # Component tests
  run_test "components_unittests" "Nostr Component Tests" "--gtest_filter=Nostr*:*NostrTest*" || OVERALL_SUCCESS=false
fi

# Run browser tests
if [ "$RUN_BROWSER_TESTS" = true ]; then
  echo -e "${GREEN}=== Running Nostr Browser Integration Tests ===${NC}"
  
  # Integration tests
  run_test "browser_tests" "Nostr Integration Tests" "--gtest_filter=Nostr*IntegrationTest*" || OVERALL_SUCCESS=false
  
  # Browser tests (E2E, permissions, etc.)
  run_test "browser_tests" "Nostr Browser Tests" "--gtest_filter=*BrowserTest*" || OVERALL_SUCCESS=false
fi

# Run performance benchmarks if requested
if [[ "$FILTER" == *"Perf"* ]] || [[ "$FILTER" == *"Benchmark"* ]]; then
  echo -e "${GREEN}=== Running Performance Benchmarks ===${NC}"
  run_test "browser_tests" "Performance Benchmarks" "--gtest_filter=*PerformanceBenchmark*:*Benchmark*" || OVERALL_SUCCESS=false
fi

# Summary
echo
echo -e "${GREEN}=== Test Summary ===${NC}"
if [ "$OVERALL_SUCCESS" = true ]; then
  echo -e "${GREEN}All tests passed!${NC}"
  exit 0
else
  echo -e "${RED}Some tests failed!${NC}"
  exit 1
fi