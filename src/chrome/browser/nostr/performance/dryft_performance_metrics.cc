// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/performance/dryft_performance_metrics.h"

#include <map>
#include <memory>
#include <string>

#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/memory/process_memory_linux.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "testing/perf/perf_result_reporter.h"

namespace dryft {

namespace {

// UMA histogram names for performance metrics
const char kStartupMetricPrefix[] = "dryft.Performance.Startup.";
const char kNip07MetricPrefix[] = "dryft.Performance.NIP07.";
const char kRelayMetricPrefix[] = "dryft.Performance.Relay.";
const char kBlossomMetricPrefix[] = "dryft.Performance.Blossom.";
const char kLibraryMetricPrefix[] = "dryft.Performance.Library.";
const char kMemoryMetricPrefix[] = "dryft.Performance.Memory.";
const char kCacheMetricPrefix[] = "dryft.Performance.Cache.";
const char kNetworkMetricPrefix[] = "dryft.Performance.Network.";

// Performance baseline storage
base::Lock g_baseline_lock;
std::map<std::string, double> g_performance_baselines;

// Memory tracking
base::Lock g_memory_lock;
size_t g_peak_memory_usage_mb = 0;

// Helper function to create performance reporter
perf_test::PerfResultReporter CreatePerfReporter(const std::string& story_name,
                                                 const std::string& metric_name) {
  perf_test::PerfResultReporter reporter(story_name, metric_name);
  reporter.RegisterImportantMetric("", "ms");
  return reporter;
}

// Helper function to log performance metric
void LogPerformanceMetric(const std::string& metric_name, 
                         base::TimeDelta duration) {
  VLOG(1) << "dryft Performance: " << metric_name << " = " 
          << duration.InMilliseconds() << "ms";
  
  // Report to performance testing framework
  auto reporter = CreatePerfReporter("dryft", metric_name);
  reporter.AddResult("", duration.InMilliseconds());
}

// Helper function to log memory metric
void LogMemoryMetric(const std::string& metric_name, size_t memory_mb) {
  VLOG(1) << "dryft Memory: " << metric_name << " = " << memory_mb << "MB";
  
  perf_test::PerfResultReporter reporter("dryft", metric_name);
  reporter.RegisterImportantMetric("", "MB");
  reporter.AddResult("", memory_mb);
}

}  // namespace

// Startup metrics
void DryftPerformanceMetrics::RecordBrowserStartupTime(base::TimeDelta duration) {
  LogPerformanceMetric(kStartupMetricPrefix + std::string("BrowserStartup"), duration);
}

void DryftPerformanceMetrics::RecordNostrServiceInitTime(base::TimeDelta duration) {
  LogPerformanceMetric(kStartupMetricPrefix + std::string("NostrServiceInit"), duration);
}

void DryftPerformanceMetrics::RecordLocalRelayStartupTime(base::TimeDelta duration) {
  LogPerformanceMetric(kStartupMetricPrefix + std::string("LocalRelayStartup"), duration);
}

void DryftPerformanceMetrics::RecordBlossomServerStartupTime(base::TimeDelta duration) {
  LogPerformanceMetric(kStartupMetricPrefix + std::string("BlossomServerStartup"), duration);
}

// NIP-07 operation metrics
void DryftPerformanceMetrics::RecordGetPublicKeyTime(base::TimeDelta duration) {
  LogPerformanceMetric(kNip07MetricPrefix + std::string("GetPublicKey"), duration);
}

void DryftPerformanceMetrics::RecordSignEventTime(base::TimeDelta duration) {
  LogPerformanceMetric(kNip07MetricPrefix + std::string("SignEvent"), duration);
}

void DryftPerformanceMetrics::RecordEncryptionTime(base::TimeDelta duration) {
  LogPerformanceMetric(kNip07MetricPrefix + std::string("Encryption"), duration);
}

void DryftPerformanceMetrics::RecordDecryptionTime(base::TimeDelta duration) {
  LogPerformanceMetric(kNip07MetricPrefix + std::string("Decryption"), duration);
}

void DryftPerformanceMetrics::RecordGetRelaysTime(base::TimeDelta duration) {
  LogPerformanceMetric(kNip07MetricPrefix + std::string("GetRelays"), duration);
}

// Local relay metrics
void DryftPerformanceMetrics::RecordEventQueryTime(base::TimeDelta duration) {
  LogPerformanceMetric(kRelayMetricPrefix + std::string("EventQuery"), duration);
}

void DryftPerformanceMetrics::RecordEventInsertTime(base::TimeDelta duration) {
  LogPerformanceMetric(kRelayMetricPrefix + std::string("EventInsert"), duration);
}

void DryftPerformanceMetrics::RecordSubscriptionTime(base::TimeDelta duration) {
  LogPerformanceMetric(kRelayMetricPrefix + std::string("Subscription"), duration);
}

void DryftPerformanceMetrics::RecordDatabaseSize(size_t size_mb) {
  LogMemoryMetric(kRelayMetricPrefix + std::string("DatabaseSize"), size_mb);
}

// Library loading metrics
void DryftPerformanceMetrics::RecordLibraryLoadTime(const std::string& library_name, 
                                                      base::TimeDelta duration) {
  LogPerformanceMetric(kLibraryMetricPrefix + library_name + ".LoadTime", duration);
}

// Memory metrics
void DryftPerformanceMetrics::RecordTotalMemoryUsage(size_t memory_mb) {
  LogMemoryMetric(kMemoryMetricPrefix + std::string("TotalUsage"), memory_mb);
  
  // Update peak tracking
  {
    base::AutoLock lock(g_memory_lock);
    g_peak_memory_usage_mb = std::max(g_peak_memory_usage_mb, memory_mb);
  }
}

void DryftPerformanceMetrics::RecordNostrMemoryUsage(size_t memory_mb) {
  LogMemoryMetric(kMemoryMetricPrefix + std::string("NostrUsage"), memory_mb);
}

void DryftPerformanceMetrics::RecordRelayMemoryUsage(size_t memory_mb) {
  LogMemoryMetric(kMemoryMetricPrefix + std::string("RelayUsage"), memory_mb);
}

void DryftPerformanceMetrics::RecordBlossomMemoryUsage(size_t memory_mb) {
  LogMemoryMetric(kMemoryMetricPrefix + std::string("BlossomUsage"), memory_mb);
}

// Blossom server metrics
void DryftPerformanceMetrics::RecordBlossomUploadTime(base::TimeDelta duration, 
                                                        size_t file_size_kb) {
  LogPerformanceMetric(kBlossomMetricPrefix + std::string("Upload"), duration);
  
  if (file_size_kb > 0) {
    double throughput_mbps = (file_size_kb * 8.0) / (duration.InMilliseconds() * 1024.0);
    perf_test::PerfResultReporter reporter("dryft", "BlossomUploadThroughput");
    reporter.RegisterImportantMetric("", "Mbps");
    reporter.AddResult("", throughput_mbps);
  }
}

void DryftPerformanceMetrics::RecordBlossomDownloadTime(base::TimeDelta duration, 
                                                          size_t file_size_kb) {
  LogPerformanceMetric(kBlossomMetricPrefix + std::string("Download"), duration);
  
  if (file_size_kb > 0) {
    double throughput_mbps = (file_size_kb * 8.0) / (duration.InMilliseconds() * 1024.0);
    perf_test::PerfResultReporter reporter("dryft", "BlossomDownloadThroughput");
    reporter.RegisterImportantMetric("", "Mbps");
    reporter.AddResult("", throughput_mbps);
  }
}

void DryftPerformanceMetrics::RecordBlossomAuthTime(base::TimeDelta duration) {
  LogPerformanceMetric(kBlossomMetricPrefix + std::string("Auth"), duration);
}

// Cache metrics
void DryftPerformanceMetrics::RecordCacheHitTime(base::TimeDelta duration) {
  LogPerformanceMetric(kCacheMetricPrefix + std::string("Hit"), duration);
}

void DryftPerformanceMetrics::RecordCacheMissTime(base::TimeDelta duration) {
  LogPerformanceMetric(kCacheMetricPrefix + std::string("Miss"), duration);
}

void DryftPerformanceMetrics::RecordCacheSize(size_t size_mb) {
  LogMemoryMetric(kCacheMetricPrefix + std::string("Size"), size_mb);
}

// Network metrics
void DryftPerformanceMetrics::RecordRelayConnectionTime(base::TimeDelta duration) {
  LogPerformanceMetric(kNetworkMetricPrefix + std::string("RelayConnection"), duration);
}

void DryftPerformanceMetrics::RecordEventPublishTime(base::TimeDelta duration) {
  LogPerformanceMetric(kNetworkMetricPrefix + std::string("EventPublish"), duration);
}

void DryftPerformanceMetrics::RecordEventFetchTime(base::TimeDelta duration) {
  LogPerformanceMetric(kNetworkMetricPrefix + std::string("EventFetch"), duration);
}

// ScopedDryftTimer implementation
ScopedDryftTimer::ScopedDryftTimer(Operation operation) 
    : operation_(operation), timer_() {}

ScopedDryftTimer::ScopedDryftTimer(Operation operation, const std::string& context)
    : operation_(operation), context_(context), timer_() {}

ScopedDryftTimer::~ScopedDryftTimer() {
  base::TimeDelta elapsed = timer_.Elapsed();
  
  switch (operation_) {
    case Operation::kBrowserStartup:
      DryftPerformanceMetrics::RecordBrowserStartupTime(elapsed);
      break;
    case Operation::kNostrServiceInit:
      DryftPerformanceMetrics::RecordNostrServiceInitTime(elapsed);
      break;
    case Operation::kLocalRelayStartup:
      DryftPerformanceMetrics::RecordLocalRelayStartupTime(elapsed);
      break;
    case Operation::kBlossomServerStartup:
      DryftPerformanceMetrics::RecordBlossomServerStartupTime(elapsed);
      break;
    case Operation::kGetPublicKey:
      DryftPerformanceMetrics::RecordGetPublicKeyTime(elapsed);
      break;
    case Operation::kSignEvent:
      DryftPerformanceMetrics::RecordSignEventTime(elapsed);
      break;
    case Operation::kEncryption:
      DryftPerformanceMetrics::RecordEncryptionTime(elapsed);
      break;
    case Operation::kDecryption:
      DryftPerformanceMetrics::RecordDecryptionTime(elapsed);
      break;
    case Operation::kGetRelays:
      DryftPerformanceMetrics::RecordGetRelaysTime(elapsed);
      break;
    case Operation::kEventQuery:
      DryftPerformanceMetrics::RecordEventQueryTime(elapsed);
      break;
    case Operation::kEventInsert:
      DryftPerformanceMetrics::RecordEventInsertTime(elapsed);
      break;
    case Operation::kSubscription:
      DryftPerformanceMetrics::RecordSubscriptionTime(elapsed);
      break;
    case Operation::kLibraryLoad:
      DryftPerformanceMetrics::RecordLibraryLoadTime(context_, elapsed);
      break;
    case Operation::kBlossomUpload:
      DryftPerformanceMetrics::RecordBlossomUploadTime(elapsed, 0);
      break;
    case Operation::kBlossomDownload:
      DryftPerformanceMetrics::RecordBlossomDownloadTime(elapsed, 0);
      break;
    case Operation::kBlossomAuth:
      DryftPerformanceMetrics::RecordBlossomAuthTime(elapsed);
      break;
    case Operation::kCacheHit:
      DryftPerformanceMetrics::RecordCacheHitTime(elapsed);
      break;
    case Operation::kCacheMiss:
      DryftPerformanceMetrics::RecordCacheMissTime(elapsed);
      break;
    case Operation::kRelayConnection:
      DryftPerformanceMetrics::RecordRelayConnectionTime(elapsed);
      break;
    case Operation::kEventPublish:
      DryftPerformanceMetrics::RecordEventPublishTime(elapsed);
      break;
    case Operation::kEventFetch:
      DryftPerformanceMetrics::RecordEventFetchTime(elapsed);
      break;
  }
}

base::TimeDelta ScopedDryftTimer::GetElapsedTime() const {
  return timer_.Elapsed();
}

// PerformanceRegressionDetector implementation
bool PerformanceRegressionDetector::CheckPerformanceRegression(
    const std::string& metric_name,
    double current_value,
    double baseline_value,
    double tolerance_percent) {
  
  double tolerance_ratio = tolerance_percent / 100.0;
  double max_acceptable_value = baseline_value * (1.0 + tolerance_ratio);
  
  bool is_acceptable = current_value <= max_acceptable_value;
  
  if (!is_acceptable) {
    LOG(WARNING) << "Performance regression detected for " << metric_name
                 << ": current=" << current_value
                 << ", baseline=" << baseline_value
                 << ", tolerance=" << tolerance_percent << "%"
                 << ", max_acceptable=" << max_acceptable_value;
  }
  
  return is_acceptable;
}

void PerformanceRegressionDetector::LogPerformanceBaseline(
    const std::string& metric_name,
    double baseline_value) {
  base::AutoLock lock(g_baseline_lock);
  g_performance_baselines[metric_name] = baseline_value;
  
  VLOG(1) << "Performance baseline recorded for " << metric_name 
          << ": " << baseline_value;
}

double PerformanceRegressionDetector::GetPerformanceBaseline(
    const std::string& metric_name) {
  base::AutoLock lock(g_baseline_lock);
  auto it = g_performance_baselines.find(metric_name);
  return (it != g_performance_baselines.end()) ? it->second : 0.0;
}

void PerformanceRegressionDetector::ClearAllBaselines() {
  base::AutoLock lock(g_baseline_lock);
  g_performance_baselines.clear();
}

// MemoryUsageTracker implementation
size_t MemoryUsageTracker::GetCurrentMemoryUsageMB() {
  auto metrics = base::ProcessMetrics::CreateCurrentProcessMetrics();
  if (!metrics) {
    return 0;
  }
  
  size_t resident_memory = metrics->GetResidentSetSize();
  return resident_memory / (1024 * 1024);  // Convert to MB
}

size_t MemoryUsageTracker::GetPeakMemoryUsageMB() {
  base::AutoLock lock(g_memory_lock);
  return g_peak_memory_usage_mb;
}

void MemoryUsageTracker::ResetPeakMemoryTracking() {
  base::AutoLock lock(g_memory_lock);
  g_peak_memory_usage_mb = GetCurrentMemoryUsageMB();
}

bool MemoryUsageTracker::IsMemoryUsageAcceptable(size_t current_mb, size_t max_mb) {
  bool is_acceptable = current_mb <= max_mb;
  
  if (!is_acceptable) {
    LOG(WARNING) << "Memory usage exceeded threshold: current=" << current_mb 
                 << "MB, max=" << max_mb << "MB";
  }
  
  return is_acceptable;
}

}  // namespace dryft