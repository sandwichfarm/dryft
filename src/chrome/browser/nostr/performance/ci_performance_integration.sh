#!/bin/bash
# Copyright 2024 The dryft Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# CI/CD integration script for Tungsten performance tests

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-out/Release}"
OUTPUT_DIR="${2:-performance_results}"
CATEGORIES="${3:-startup memory nip07 relay library}"

echo "Starting Tungsten performance testing..."
echo "Build directory: $BUILD_DIR"
echo "Output directory: $OUTPUT_DIR"
echo "Test categories: $CATEGORIES"

# Validate build directory
if [[ ! -d "$BUILD_DIR" ]]; then
    echo "Error: Build directory not found: $BUILD_DIR"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Check if test binary exists
if [[ ! -f "$BUILD_DIR/unit_tests" ]]; then
    echo "Error: unit_tests binary not found in $BUILD_DIR"
    echo "Please build the performance tests first:"
    echo "  autoninja -C $BUILD_DIR chrome/browser/nostr/performance:performance_tests"
    exit 1
fi

# Run performance tests
echo "Running performance tests..."
python3 "$SCRIPT_DIR/run_performance_tests.py" \
    --build-dir "$BUILD_DIR" \
    --output-dir "$OUTPUT_DIR" \
    --categories $CATEGORIES \
    --generate-report \
    --verbose

# Check exit code
if [[ $? -ne 0 ]]; then
    echo "Performance tests failed!"
    exit 1
fi

echo "Performance tests completed successfully!"

# If running in CI, upload results
if [[ -n "$CI" ]]; then
    echo "Uploading performance results..."
    
    # Copy results to CI artifacts directory
    if [[ -n "$ARTIFACTS_DIR" ]]; then
        cp -r "$OUTPUT_DIR" "$ARTIFACTS_DIR/"
        echo "Results uploaded to CI artifacts"
    fi
    
    # Post results to GitHub (if running on GitHub Actions)
    if [[ -n "$GITHUB_TOKEN" && -n "$GITHUB_REPOSITORY" ]]; then
        echo "Posting performance results to GitHub..."
        
        # Create comment with performance summary
        if [[ -f "$OUTPUT_DIR/performance_report.md" ]]; then
            gh api repos/$GITHUB_REPOSITORY/issues/$GITHUB_PR_NUMBER/comments \
                -f body="$(cat $OUTPUT_DIR/performance_report.md)" \
                --silent || echo "Failed to post GitHub comment"
        fi
    fi
fi

echo "Performance testing completed!"