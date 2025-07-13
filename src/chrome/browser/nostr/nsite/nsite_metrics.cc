// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"

namespace nostr {

// UMA histogram names
constexpr char kCacheHitTimeHistogram[] = "Nsite.Cache.HitTime";
constexpr char kCacheMissTimeHistogram[] = "Nsite.Cache.MissTime";
constexpr char kCacheHitRateHistogram[] = "Nsite.Cache.HitRate";
constexpr char kCacheSizeHistogram[] = "Nsite.Cache.SizeMB";
constexpr char kCacheFileCountHistogram[] = "Nsite.Cache.FileCount";

constexpr char kServerStartupTimeHistogram[] = "Nsite.Server.StartupTime";
constexpr char kRequestProcessingTimeHistogram[] = "Nsite.Server.RequestProcessingTime";
constexpr char kPortAllocationTimeHistogram[] = "Nsite.Server.PortAllocationTime";
constexpr char kMemoryUsageHistogram[] = "Nsite.Server.MemoryUsageMB";

constexpr char kUpdateCheckTimeHistogram[] = "Nsite.Update.CheckTime";
constexpr char kUpdateDownloadTimeHistogram[] = "Nsite.Update.DownloadTime";
constexpr char kUpdateCheckFrequencyHistogram[] = "Nsite.Update.CheckFrequency";

constexpr char kRateLimitViolationsHistogram[] = "Nsite.Security.RateLimitViolations";
constexpr char kSecurityValidationTimeHistogram[] = "Nsite.Security.ValidationTime";

constexpr char kThreadPoolQueueTimeHistogram[] = "Nsite.Performance.ThreadPoolQueueTime";
constexpr char kDiskIOTimeHistogram[] = "Nsite.Performance.DiskIOTime";

// Cache operation metrics
void NsiteMetrics::RecordCacheHitTime(base::TimeDelta duration) {
  base::UmaHistogramTimes(kCacheHitTimeHistogram, duration);
}

void NsiteMetrics::RecordCacheMissTime(base::TimeDelta duration) {
  base::UmaHistogramTimes(kCacheMissTimeHistogram, duration);
}

void NsiteMetrics::RecordCacheHitRate(bool hit) {
  base::UmaHistogramBoolean(kCacheHitRateHistogram, hit);
}

void NsiteMetrics::RecordCacheSize(size_t size_mb) {
  base::UmaHistogramCounts1000(kCacheSizeHistogram, static_cast<int>(size_mb));
}

void NsiteMetrics::RecordCacheFileCount(size_t count) {
  base::UmaHistogramCounts10000(kCacheFileCountHistogram, static_cast<int>(count));
}

// Server operation metrics
void NsiteMetrics::RecordServerStartupTime(base::TimeDelta duration) {
  base::UmaHistogramTimes(kServerStartupTimeHistogram, duration);
}

void NsiteMetrics::RecordRequestProcessingTime(base::TimeDelta duration) {
  base::UmaHistogramTimes(kRequestProcessingTimeHistogram, duration);
}

void NsiteMetrics::RecordPortAllocationTime(base::TimeDelta duration) {
  base::UmaHistogramTimes(kPortAllocationTimeHistogram, duration);
}

void NsiteMetrics::RecordMemoryUsage(size_t memory_mb) {
  base::UmaHistogramMemoryMB(kMemoryUsageHistogram, static_cast<int>(memory_mb));
}

// Update monitoring metrics
void NsiteMetrics::RecordUpdateCheckTime(base::TimeDelta duration) {
  base::UmaHistogramLongTimes(kUpdateCheckTimeHistogram, duration);
}

void NsiteMetrics::RecordUpdateDownloadTime(base::TimeDelta duration) {
  base::UmaHistogramLongTimes(kUpdateDownloadTimeHistogram, duration);
}

void NsiteMetrics::RecordUpdateCheckFrequency(int checks_per_hour) {
  base::UmaHistogramCounts100(kUpdateCheckFrequencyHistogram, checks_per_hour);
}

// Security metrics
void NsiteMetrics::RecordRateLimitViolations(int violation_count) {
  base::UmaHistogramCounts100(kRateLimitViolationsHistogram, violation_count);
}

void NsiteMetrics::RecordSecurityValidationTime(base::TimeDelta duration) {
  base::UmaHistogramTimes(kSecurityValidationTimeHistogram, duration);
}

// Performance optimization metrics
void NsiteMetrics::RecordThreadPoolQueueTime(base::TimeDelta duration) {
  base::UmaHistogramTimes(kThreadPoolQueueTimeHistogram, duration);
}

void NsiteMetrics::RecordDiskIOTime(base::TimeDelta duration) {
  base::UmaHistogramTimes(kDiskIOTimeHistogram, duration);
}

// ScopedNsiteTimer implementation
ScopedNsiteTimer::ScopedNsiteTimer(Operation operation) 
    : operation_(operation), start_time_(base::TimeTicks::Now()) {}

ScopedNsiteTimer::~ScopedNsiteTimer() {
  base::TimeDelta duration = base::TimeTicks::Now() - start_time_;
  
  switch (operation_) {
    case Operation::kCacheHit:
      NsiteMetrics::RecordCacheHitTime(duration);
      break;
    case Operation::kCacheMiss:
      NsiteMetrics::RecordCacheMissTime(duration);
      break;
    case Operation::kServerStartup:
      NsiteMetrics::RecordServerStartupTime(duration);
      break;
    case Operation::kRequestProcessing:
      NsiteMetrics::RecordRequestProcessingTime(duration);
      break;
    case Operation::kPortAllocation:
      NsiteMetrics::RecordPortAllocationTime(duration);
      break;
    case Operation::kUpdateCheck:
      NsiteMetrics::RecordUpdateCheckTime(duration);
      break;
    case Operation::kUpdateDownload:
      NsiteMetrics::RecordUpdateDownloadTime(duration);
      break;
    case Operation::kSecurityValidation:
      NsiteMetrics::RecordSecurityValidationTime(duration);
      break;
    case Operation::kThreadPoolQueue:
      NsiteMetrics::RecordThreadPoolQueueTime(duration);
      break;
    case Operation::kDiskIO:
      NsiteMetrics::RecordDiskIOTime(duration);
      break;
  }
}

}  // namespace nostr