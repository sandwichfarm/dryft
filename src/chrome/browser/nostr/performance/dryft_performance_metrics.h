// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_PERFORMANCE_DRYFT_PERFORMANCE_METRICS_H_
#define CHROME_BROWSER_NOSTR_PERFORMANCE_DRYFT_PERFORMANCE_METRICS_H_

#include <string>
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"

namespace dryft {

// Centralized performance metrics collection for dryft Nostr features
class DryftPerformanceMetrics {
 public:
  // Startup metrics
  static void RecordBrowserStartupTime(base::TimeDelta duration);
  static void RecordNostrServiceInitTime(base::TimeDelta duration);
  static void RecordLocalRelayStartupTime(base::TimeDelta duration);
  static void RecordBlossomServerStartupTime(base::TimeDelta duration);
  
  // NIP-07 operation metrics
  static void RecordGetPublicKeyTime(base::TimeDelta duration);
  static void RecordSignEventTime(base::TimeDelta duration);
  static void RecordEncryptionTime(base::TimeDelta duration);
  static void RecordDecryptionTime(base::TimeDelta duration);
  static void RecordGetRelaysTime(base::TimeDelta duration);
  
  // Local relay metrics
  static void RecordEventQueryTime(base::TimeDelta duration);
  static void RecordEventInsertTime(base::TimeDelta duration);
  static void RecordSubscriptionTime(base::TimeDelta duration);
  static void RecordDatabaseSize(size_t size_mb);
  
  // Library loading metrics
  static void RecordLibraryLoadTime(const std::string& library_name, 
                                   base::TimeDelta duration);
  
  // Memory metrics
  static void RecordTotalMemoryUsage(size_t memory_mb);
  static void RecordNostrMemoryUsage(size_t memory_mb);
  static void RecordRelayMemoryUsage(size_t memory_mb);
  static void RecordBlossomMemoryUsage(size_t memory_mb);
  
  // Blossom server metrics
  static void RecordBlossomUploadTime(base::TimeDelta duration, size_t file_size_kb);
  static void RecordBlossomDownloadTime(base::TimeDelta duration, size_t file_size_kb);
  static void RecordBlossomAuthTime(base::TimeDelta duration);
  
  // Cache metrics
  static void RecordCacheHitTime(base::TimeDelta duration);
  static void RecordCacheMissTime(base::TimeDelta duration);
  static void RecordCacheSize(size_t size_mb);
  
  // Network metrics
  static void RecordRelayConnectionTime(base::TimeDelta duration);
  static void RecordEventPublishTime(base::TimeDelta duration);
  static void RecordEventFetchTime(base::TimeDelta duration);
  
  // Performance thresholds (from CLAUDE.md)
  static constexpr base::TimeDelta kMaxStartupOverhead = base::Milliseconds(50);
  static constexpr base::TimeDelta kMaxNip07OperationTime = base::Milliseconds(20);
  static constexpr base::TimeDelta kMaxLocalRelayQueryTime = base::Milliseconds(10);
  static constexpr size_t kMaxBaseMemoryUsageMB = 50;
  static constexpr double kMaxIdleCpuUsagePercent = 0.1;
};

// RAII timing helper for automatic performance measurement
class ScopedDryftTimer {
 public:
  enum class Operation {
    kBrowserStartup,
    kNostrServiceInit,
    kLocalRelayStartup,
    kBlossomServerStartup,
    kGetPublicKey,
    kSignEvent,
    kEncryption,
    kDecryption,
    kGetRelays,
    kEventQuery,
    kEventInsert,
    kSubscription,
    kLibraryLoad,
    kBlossomUpload,
    kBlossomDownload,
    kBlossomAuth,
    kCacheHit,
    kCacheMiss,
    kRelayConnection,
    kEventPublish,
    kEventFetch
  };
  
  explicit ScopedDryftTimer(Operation operation);
  explicit ScopedDryftTimer(Operation operation, const std::string& context);
  ~ScopedDryftTimer();
  
  // Get elapsed time without destroying the timer
  base::TimeDelta GetElapsedTime() const;
  
  // Disable copy and move operations
  ScopedDryftTimer(const ScopedDryftTimer&) = delete;
  ScopedDryftTimer& operator=(const ScopedDryftTimer&) = delete;
  ScopedDryftTimer(ScopedDryftTimer&&) = delete;
  ScopedDryftTimer& operator=(ScopedDryftTimer&&) = delete;
  
 private:
  Operation operation_;
  std::string context_;
  base::ElapsedTimer timer_;
};

// Convenience macros for common performance measurements
#define SCOPED_DRYFT_TIMER(operation) \
  ScopedDryftTimer timer(ScopedDryftTimer::Operation::operation)

#define SCOPED_DRYFT_TIMER_WITH_CONTEXT(operation, context) \
  ScopedDryftTimer timer(ScopedDryftTimer::Operation::operation, context)

// Performance regression detection
class PerformanceRegressionDetector {
 public:
  // Check if current performance is within acceptable bounds
  static bool CheckPerformanceRegression(const std::string& metric_name,
                                        double current_value,
                                        double baseline_value,
                                        double tolerance_percent = 10.0);
  
  // Log performance baseline for future comparisons
  static void LogPerformanceBaseline(const std::string& metric_name,
                                    double baseline_value);
  
  // Get stored baseline value
  static double GetPerformanceBaseline(const std::string& metric_name);
  
  // Clear all stored baselines (for testing)
  static void ClearAllBaselines();
};

// Memory usage tracker
class MemoryUsageTracker {
 public:
  // Get current memory usage in MB
  static size_t GetCurrentMemoryUsageMB();
  
  // Get peak memory usage since last reset
  static size_t GetPeakMemoryUsageMB();
  
  // Reset peak memory tracking
  static void ResetPeakMemoryTracking();
  
  // Check if memory usage is within acceptable bounds
  static bool IsMemoryUsageAcceptable(size_t current_mb, size_t max_mb);
};

}  // namespace dryft

#endif  // CHROME_BROWSER_NOSTR_PERFORMANCE_DRYFT_PERFORMANCE_METRICS_H_