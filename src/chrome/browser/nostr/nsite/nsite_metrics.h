// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NSITE_NSITE_METRICS_H_
#define CHROME_BROWSER_NOSTR_NSITE_NSITE_METRICS_H_

#include "base/time/time.h"

namespace nostr {

// UMA metrics for Nsite streaming server performance monitoring
class NsiteMetrics {
 public:
  // Cache operation metrics
  static void RecordCacheHitTime(base::TimeDelta duration);
  static void RecordCacheMissTime(base::TimeDelta duration);
  static void RecordCacheHitRate(bool hit);
  static void RecordCacheSize(size_t size_mb);
  static void RecordCacheFileCount(size_t count);

  // Server operation metrics
  static void RecordServerStartupTime(base::TimeDelta duration);
  static void RecordRequestProcessingTime(base::TimeDelta duration);
  static void RecordPortAllocationTime(base::TimeDelta duration);
  static void RecordMemoryUsage(size_t memory_mb);

  // Update monitoring metrics
  static void RecordUpdateCheckTime(base::TimeDelta duration);
  static void RecordUpdateDownloadTime(base::TimeDelta duration);
  static void RecordUpdateCheckFrequency(int checks_per_hour);

  // Security metrics
  static void RecordRateLimitViolations(int violation_count);
  static void RecordSecurityValidationTime(base::TimeDelta duration);

  // Performance optimization metrics
  static void RecordThreadPoolQueueTime(base::TimeDelta duration);
  static void RecordDiskIOTime(base::TimeDelta duration);

 private:
  NsiteMetrics() = delete;
};

// RAII helper for timing operations
class ScopedNsiteTimer {
 public:
  enum class Operation {
    kCacheHit,
    kCacheMiss,
    kServerStartup,
    kRequestProcessing,
    kPortAllocation,
    kUpdateCheck,
    kUpdateDownload,
    kSecurityValidation,
    kThreadPoolQueue,
    kDiskIO
  };

  explicit ScopedNsiteTimer(Operation operation);
  ~ScopedNsiteTimer();

 private:
  Operation operation_;
  base::TimeTicks start_time_;
};

// Macro for easy timing
#define SCOPED_NSITE_TIMER(operation) \
  ScopedNsiteTimer timer(ScopedNsiteTimer::Operation::operation)

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NSITE_NSITE_METRICS_H_