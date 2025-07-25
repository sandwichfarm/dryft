# Tungsten Performance Testing Suite

This directory contains the comprehensive performance testing suite for Tungsten's Nostr features.

## Overview

The performance testing suite measures and monitors the performance of Tungsten's Nostr implementation across several key areas:

- **Startup Performance**: Browser startup impact and service initialization times
- **Memory Usage**: Memory consumption and leak detection
- **NIP-07 Operations**: Performance of core Nostr API operations
- **Local Relay**: Database operations and query performance
- **Library Loading**: JavaScript library loading and execution overhead

## Performance Targets

Based on targets from `/CLAUDE.md`:

| Metric | Target | Description |
|--------|---------|-------------|
| Startup overhead | < 50ms | Additional browser startup time |
| Memory overhead | < 50MB | Base memory usage |
| NIP-07 operations | < 20ms | Per operation (getPublicKey, signEvent, etc.) |
| Local relay queries | < 10ms | Per database query |
| CPU usage (idle) | < 0.1% | Idle CPU consumption |

## Test Categories

### 1. Startup Performance (`startup`)
- `NostrServiceInitializationPerformance`: Service initialization timing
- `LocalRelayStartupPerformance`: Database and relay startup timing

### 2. Memory Usage (`memory`)
- `BaseMemoryUsage`: Overall memory consumption
- `NostrServiceMemoryUsage`: Memory used by Nostr service
- `LocalRelayMemoryUsage`: Memory used by local relay
- `MemoryUsageWithManyEvents`: Memory scaling with event count

### 3. NIP-07 Operations (`nip07`)
- `GetPublicKeyPerformance`: Public key retrieval timing
- `SignEventPerformance`: Event signing timing
- `EncryptionPerformance`: NIP-04 encryption timing
- `DecryptionPerformance`: NIP-04 decryption timing
- `GetRelaysPerformance`: Relay list retrieval timing
- `ConcurrentOperationsPerformance`: Multi-operation throughput
- `LargeEventSigningPerformance`: Large event handling

### 4. Local Relay Performance (`relay`)
- `EventInsertPerformance`: Event insertion timing
- `EventQueryPerformance`: Event query timing
- `SubscriptionPerformance`: Subscription creation timing
- `ConcurrentQueriesPerformance`: Multi-query throughput
- `BulkEventInsertPerformance`: Bulk operation performance
- `DatabaseSizePerformance`: Storage efficiency
- `ComplexQueryPerformance`: Complex filter performance

### 5. Library Loading (`library`)
- `NDKLibraryLoadingPerformance`: NDK library loading
- `NostrToolsLibraryLoadingPerformance`: nostr-tools library loading
- `Secp256k1LibraryLoadingPerformance`: secp256k1 library loading
- `ApplesauceLibraryLoadingPerformance`: applesauce library loading
- `AlbySDKLibraryLoadingPerformance`: alby-sdk library loading
- `ConcurrentLibraryLoadingPerformance`: Multi-library loading
- `LibraryLoadingCacheEffectiveness`: Cache performance
- `LibraryBundleSizeImpact`: Size vs. performance correlation

## Files Structure

```
performance/
├── BUILD.gn                              # Build configuration
├── README.md                             # This file
├── tungsten_performance_metrics.{h,cc}   # Core metrics framework
├── tungsten_performance_test.cc          # Main performance tests
├── nip07_performance_test.cc             # NIP-07 specific tests
├── local_relay_performance_test.cc       # Local relay tests
├── library_loading_performance_test.cc   # Library loading tests
├── run_performance_tests.py              # Test runner script
├── ci_performance_integration.sh         # CI/CD integration
└── test_data/                            # Test data files
    ├── sample_events.json                # Real Nostr events
    ├── test_keys.json                    # Real key pairs
    └── benchmark_data.json               # Performance targets
```

## Running Performance Tests

### Prerequisites

1. Build Tungsten with performance tests enabled:
```bash
cd src
gn gen out/Release --args="is_official_build=true enable_nostr=true enable_local_relay=true enable_blossom_server=true"
autoninja -C out/Release chrome/browser/nostr/performance:performance_tests
```

### Running Individual Tests

```bash
# Run all performance tests
./out/Release/unit_tests --gtest_filter="*PerformanceTest*"

# Run specific category
./out/Release/unit_tests --gtest_filter="NIP07PerformanceTest*"

# Run specific test
./out/Release/unit_tests --gtest_filter="*GetPublicKeyPerformance"

# Run with performance results
./out/Release/unit_tests --gtest_filter="*PerformanceTest*" --test-launcher-print-perf-results
```

### Using the Test Runner

```bash
# Run all categories
python3 src/chrome/browser/nostr/performance/run_performance_tests.py \\
    --build-dir out/Release \\
    --output-dir performance_results \\
    --generate-report

# Run specific categories
python3 src/chrome/browser/nostr/performance/run_performance_tests.py \\
    --build-dir out/Release \\
    --categories startup memory nip07 \\
    --verbose

# CI/CD integration
./src/chrome/browser/nostr/performance/ci_performance_integration.sh out/Release
```

## Performance Metrics Framework

### Core Components

#### TungstenPerformanceMetrics
Central metrics collection class that records performance data:

```cpp
// Record startup timing
TungstenPerformanceMetrics::RecordNostrServiceInitTime(duration);

// Record NIP-07 operation timing
TungstenPerformanceMetrics::RecordGetPublicKeyTime(duration);

// Record memory usage
TungstenPerformanceMetrics::RecordTotalMemoryUsage(memory_mb);
```

#### ScopedTungstenTimer
RAII timer for automatic performance measurement:

```cpp
{
    SCOPED_DRYFT_TIMER(kGetPublicKey);
    // Code to measure
} // Automatically records timing when scope exits

// With context
{
    SCOPED_DRYFT_TIMER_WITH_CONTEXT(kLibraryLoad, "ndk");
    // Library loading code
}
```

#### PerformanceRegressionDetector
Detects performance regressions by comparing against baselines:

```cpp
// Set baseline
PerformanceRegressionDetector::LogPerformanceBaseline("GetPublicKey", 15.0);

// Check for regression
bool acceptable = PerformanceRegressionDetector::CheckPerformanceRegression(
    "GetPublicKey", current_time, baseline_time, 10.0);
```

#### MemoryUsageTracker
Tracks memory usage patterns:

```cpp
size_t current_memory = MemoryUsageTracker::GetCurrentMemoryUsageMB();
size_t peak_memory = MemoryUsageTracker::GetPeakMemoryUsageMB();
```

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Performance Tests

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]

jobs:
  performance:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    
    - name: Setup build environment
      run: |
        # Setup depot_tools, etc.
        
    - name: Build Tungsten
      run: |
        gn gen out/Release --args="is_official_build=true enable_nostr=true"
        autoninja -C out/Release chrome/browser/nostr/performance:performance_tests
        
    - name: Run performance tests
      run: |
        ./src/chrome/browser/nostr/performance/ci_performance_integration.sh out/Release
        
    - name: Upload results
      uses: actions/upload-artifact@v2
      with:
        name: performance-results
        path: performance_results/
```

### Performance Regression Detection

The CI integration automatically:
1. Runs all performance tests
2. Compares results against baselines
3. Generates performance report
4. Posts results to GitHub PR comments
5. Fails the build if critical regressions are detected

## Test Data

### Real Nostr Events
Test events are generated using `nak` for realistic performance testing:

```json
{
  "id": "42dd87b2c0ec47d8c79a4575740c3e060bd875e1f8b9f1f6894d386e2c017c09",
  "pubkey": "dadbec1864f407d2b28b4ae28f523da208003eaa234e1765ed13a4f3431d2205",
  "created_at": 1752566074,
  "kind": 1,
  "content": "This is a sample text note event for performance testing purposes.",
  "sig": "2084d382bca08f0dc3fe43e01a1a3275c5d37b8582dea446b6393faa110ae040e2a513a05e58a8ce4d6d5bc01494261834250925af39b127ad66c10603390281"
}
```

### Real Key Pairs
Test keys are real secp256k1 key pairs generated with `nak`:

```json
{
  "name": "Test Key 1",
  "private_key": "649fcb6f976cc45e809ef8918bd0ffda8808892c433653efe1e7f599c3575a40",
  "public_key": "dadbec1864f407d2b28b4ae28f523da208003eaa234e1765ed13a4f3431d2205",
  "is_default": true
}
```

## Troubleshooting

### Common Issues

1. **Test binary not found**: Ensure you've built the performance tests target
2. **Performance test failures**: Check that system is not under heavy load
3. **Memory test failures**: Close other applications to ensure clean memory state
4. **Library loading failures**: Verify that library files are present in the build

### Debug Mode

Run tests with verbose output to see detailed metrics:

```bash
./out/Release/unit_tests --gtest_filter="*PerformanceTest*" --v=1
```

### Performance Debugging

Use Chrome's built-in performance tools:

```bash
# Enable debug logging
chrome.storage.local.set({'dryft.dev.debug_logging': true});

# View performance metrics in DevTools
console.log(window.performance.getEntriesByType('measure'));
```

## Contributing

When adding new performance tests:

1. Follow the existing test patterns
2. Use realistic test data
3. Set appropriate performance baselines
4. Include both success and stress test cases
5. Add proper error handling and timeouts
6. Update this documentation

## Performance Monitoring

The performance testing suite integrates with Chromium's performance monitoring infrastructure:

- **Metrics Collection**: All metrics are reported to Chrome's performance dashboard
- **Regression Detection**: Automated baseline comparison
- **CI/CD Integration**: Automated performance testing on every commit
- **Historical Tracking**: Performance trends over time

This ensures that Tungsten maintains excellent performance characteristics while adding powerful Nostr functionality.