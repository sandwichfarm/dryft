// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/performance/tungsten_performance_metrics.h"

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

namespace tungsten {

namespace {

// UMA histogram names for performance metrics
const char kStartupMetricPrefix[] = "Tungsten.Performance.Startup.";
const char kNip07MetricPrefix[] = "Tungsten.Performance.NIP07.";
const char kRelayMetricPrefix[] = "Tungsten.Performance.Relay.";
const char kBlossomMetricPrefix[] = "Tungsten.Performance.Blossom.";
const char kLibraryMetricPrefix[] = "Tungsten.Performance.Library.";
const char kMemoryMetricPrefix[] = "Tungsten.Performance.Memory.";
const char kCacheMetricPrefix[] = "Tungsten.Performance.Cache.";
const char kNetworkMetricPrefix[] = "Tungsten.Performance.Network.";

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
  VLOG(1) << "Tungsten Performance: " << metric_name << " = " 
          << duration.InMilliseconds() << "ms";
  
  // Report to performance testing framework
  auto reporter = CreatePerfReporter("Tungsten", metric_name);
  reporter.AddResult("", duration.InMilliseconds());
}

// Helper function to log memory metric
void LogMemoryMetric(const std::string& metric_name, size_t memory_mb) {
  VLOG(1) << "Tungsten Memory: " << metric_name << " = " << memory_mb << "MB";
  
  perf_test::PerfResultReporter reporter("Tungsten", metric_name);
  reporter.RegisterImportantMetric("", "MB");
  reporter.AddResult("", memory_mb);
}

}  // namespace

// Startup metrics
void TungstenPerformanceMetrics::RecordBrowserStartupTime(base::TimeDelta duration) {
  LogPerformanceMetric(kStartupMetricPrefix + std::string("BrowserStartup"), duration);
}

void TungstenPerformanceMetrics::RecordNostrServiceInitTime(base::TimeDelta duration) {
  LogPerformanceMetric(kStartupMetricPrefix + std::string("NostrServiceInit"), duration);
}

void TungstenPerformanceMetrics::RecordLocalRelayStartupTime(base::TimeDelta duration) {
  LogPerformanceMetric(kStartupMetricPrefix + std::string("LocalRelayStartup"), duration);
}

void TungstenPerformanceMetrics::RecordBlossomServerStartupTime(base::TimeDelta duration) {
  LogPerformanceMetric(kStartupMetricPrefix + std::string("BlossomServerStartup"), duration);
}

// NIP-07 operation metrics
void TungstenPerformanceMetrics::RecordGetPublicKeyTime(base::TimeDelta duration) {
  LogPerformanceMetric(kNip07MetricPrefix + std::string("GetPublicKey"), duration);
}

void TungstenPerformanceMetrics::RecordSignEventTime(base::TimeDelta duration) {
  LogPerformanceMetric(kNip07MetricPrefix + std::string("SignEvent"), duration);
}

void TungstenPerformanceMetrics::RecordEncryptionTime(base::TimeDelta duration) {
  LogPerformanceMetric(kNip07MetricPrefix + std::string("Encryption"), duration);
}

void TungstenPerformanceMetrics::RecordDecryptionTime(base::TimeDelta duration) {
  LogPerformanceMetric(kNip07MetricPrefix + std::string("Decryption"), duration);
}

void TungstenPerformanceMetrics::RecordGetRelaysTime(base::TimeDelta duration) {
  LogPerformanceMetric(kNip07MetricPrefix + std::string("GetRelays"), duration);
}

// Local relay metrics
void TungstenPerformanceMetrics::RecordEventQueryTime(base::TimeDelta duration) {
  LogPerformanceMetric(kRelayMetricPrefix + std::string("EventQuery"), duration);
}

void TungstenPerformanceMetrics::RecordEventInsertTime(base::TimeDelta duration) {
  LogPerformanceMetric(kRelayMetricPrefix + std::string("EventInsert"), duration);
}

void TungstenPerformanceMetrics::RecordSubscriptionTime(base::TimeDelta duration) {
  LogPerformanceMetric(kRelayMetricPrefix + std::string("Subscription"), duration);
}

void TungstenPerformanceMetrics::RecordDatabaseSize(size_t size_mb) {
  LogMemoryMetric(kRelayMetricPrefix + std::string("DatabaseSize"), size_mb);
}

// Library loading metrics
void TungstenPerformanceMetrics::RecordLibraryLoadTime(const std::string& library_name, 
                                                      base::TimeDelta duration) {
  LogPerformanceMetric(kLibraryMetricPrefix + library_name + ".LoadTime", duration);
}

// Memory metrics
void TungstenPerformanceMetrics::RecordTotalMemoryUsage(size_t memory_mb) {
  LogMemoryMetric(kMemoryMetricPrefix + std::string("TotalUsage"), memory_mb);
  
  // Update peak tracking
  {
    base::AutoLock lock(g_memory_lock);
    g_peak_memory_usage_mb = std::max(g_peak_memory_usage_mb, memory_mb);
  }
}

void TungstenPerformanceMetrics::RecordNostrMemoryUsage(size_t memory_mb) {
  LogMemoryMetric(kMemoryMetricPrefix + std::string("NostrUsage"), memory_mb);
}

void TungstenPerformanceMetrics::RecordRelayMemoryUsage(size_t memory_mb) {
  LogMemoryMetric(kMemoryMetricPrefix + std::string("RelayUsage"), memory_mb);
}

void TungstenPerformanceMetrics::RecordBlossomMemoryUsage(size_t memory_mb) {
  LogMemoryMetric(kMemoryMetricPrefix + std::string("BlossomUsage"), memory_mb);
}

// Blossom server metrics
void TungstenPerformanceMetrics::RecordBlossomUploadTime(base::TimeDelta duration, 
                                                        size_t file_size_kb) {
  LogPerformanceMetric(kBlossomMetricPrefix + std::string("Upload"), duration);
  
  if (file_size_kb > 0) {
    double throughput_mbps = (file_size_kb * 8.0) / (duration.InMilliseconds() * 1024.0);
    perf_test::PerfResultReporter reporter("Tungsten", "BlossomUploadThroughput");
    reporter.RegisterImportantMetric("", "Mbps");
    reporter.AddResult("", throughput_mbps);
  }
}

void TungstenPerformanceMetrics::RecordBlossomDownloadTime(base::TimeDelta duration, 
                                                          size_t file_size_kb) {
  LogPerformanceMetric(kBlossomMetricPrefix + std::string("Download"), duration);
  
  if (file_size_kb > 0) {
    double throughput_mbps = (file_size_kb * 8.0) / (duration.InMilliseconds() * 1024.0);
    perf_test::PerfResultReporter reporter("Tungsten", "BlossomDownloadThroughput");
    reporter.RegisterImportantMetric("", "Mbps");
    reporter.AddResult("", throughput_mbps);
  }
}

void TungstenPerformanceMetrics::RecordBlossomAuthTime(base::TimeDelta duration) {
  LogPerformanceMetric(kBlossomMetricPrefix + std::string("Auth"), duration);
}

// Cache metrics
void TungstenPerformanceMetrics::RecordCacheHitTime(base::TimeDelta duration) {
  LogPerformanceMetric(kCacheMetricPrefix + std::string("Hit"), duration);
}

void TungstenPerformanceMetrics::RecordCacheMissTime(base::TimeDelta duration) {
  LogPerformanceMetric(kCacheMetricPrefix + std::string("Miss"), duration);
}

void TungstenPerformanceMetrics::RecordCacheSize(size_t size_mb) {
  LogMemoryMetric(kCacheMetricPrefix + std::string("Size"), size_mb);
}

// Network metrics
void TungstenPerformanceMetrics::RecordRelayConnectionTime(base::TimeDelta duration) {
  LogPerformanceMetric(kNetworkMetricPrefix + std::string("RelayConnection"), duration);
}

void TungstenPerformanceMetrics::RecordEventPublishTime(base::TimeDelta duration) {
  LogPerformanceMetric(kNetworkMetricPrefix + std::string("EventPublish"), duration);
}

void TungstenPerformanceMetrics::RecordEventFetchTime(base::TimeDelta duration) {
  LogPerformanceMetric(kNetworkMetricPrefix + std::string("EventFetch"), duration);
}

// ScopedTungstenTimer implementation
ScopedTungstenTimer::ScopedTungstenTimer(Operation operation) 
    : operation_(operation), timer_() {}

ScopedTungstenTimer::ScopedTungstenTimer(Operation operation, const std::string& context)
    : operation_(operation), context_(context), timer_() {}

ScopedTungstenTimer::~ScopedTungstenTimer() {
  base::TimeDelta elapsed = timer_.Elapsed();
  
  switch (operation_) {
    case Operation::kBrowserStartup:
      TungstenPerformanceMetrics::RecordBrowserStartupTime(elapsed);
      break;
    case Operation::kNostrServiceInit:
      TungstenPerformanceMetrics::RecordNostrServiceInitTime(elapsed);
      break;
    case Operation::kLocalRelayStartup:
      TungstenPerformanceMetrics::RecordLocalRelayStartupTime(elapsed);
      break;
    case Operation::kBlossomServerStartup:
      TungstenPerformanceMetrics::RecordBlossomServerStartupTime(elapsed);
      break;
    case Operation::kGetPublicKey:
      TungstenPerformanceMetrics::RecordGetPublicKeyTime(elapsed);
      break;
    case Operation::kSignEvent:
      TungstenPerformanceMetrics::RecordSignEventTime(elapsed);
      break;
    case Operation::kEncryption:
      TungstenPerformanceMetrics::RecordEncryptionTime(elapsed);
      break;
    case Operation::kDecryption:
      TungstenPerformanceMetrics::RecordDecryptionTime(elapsed);
      break;
    case Operation::kGetRelays:
      TungstenPerformanceMetrics::RecordGetRelaysTime(elapsed);
      break;
    case Operation::kEventQuery:
      TungstenPerformanceMetrics::RecordEventQueryTime(elapsed);
      break;
    case Operation::kEventInsert:
      TungstenPerformanceMetrics::RecordEventInsertTime(elapsed);
      break;
    case Operation::kSubscription:
      TungstenPerformanceMetrics::RecordSubscriptionTime(elapsed);
      break;
    case Operation::kLibraryLoad:
      TungstenPerformanceMetrics::RecordLibraryLoadTime(context_, elapsed);
      break;
    case Operation::kBlossomUpload:
      TungstenPerformanceMetrics::RecordBlossomUploadTime(elapsed, 0);
      break;
    case Operation::kBlossomDownload:
      TungstenPerformanceMetrics::RecordBlossomDownloadTime(elapsed, 0);
      break;
    case Operation::kBlossomAuth:
      TungstenPerformanceMetrics::RecordBlossomAuthTime(elapsed);
      break;
    case Operation::kCacheHit:
      TungstenPerformanceMetrics::RecordCacheHitTime(elapsed);
      break;
    case Operation::kCacheMiss:
      TungstenPerformanceMetrics::RecordCacheMissTime(elapsed);
      break;
    case Operation::kRelayConnection:
      TungstenPerformanceMetrics::RecordRelayConnectionTime(elapsed);
      break;
    case Operation::kEventPublish:
      TungstenPerformanceMetrics::RecordEventPublishTime(elapsed);
      break;
    case Operation::kEventFetch:
      TungstenPerformanceMetrics::RecordEventFetchTime(elapsed);
      break;
  }
}

base::TimeDelta ScopedTungstenTimer::GetElapsedTime() const {
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

}  // namespace tungsten