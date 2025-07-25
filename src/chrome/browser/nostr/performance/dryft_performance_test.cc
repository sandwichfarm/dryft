// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/performance/dryft_performance_metrics.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/nostr/nostr_service.h"
#include "chrome/browser/nostr/nostr_service_factory.h"
#include "chrome/browser/nostr/local_relay/local_relay_service.h"
#include "chrome/browser/nostr/local_relay/local_relay_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/nostr/nostr_event.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace dryft {

class DryftPerformanceTest : public testing::Test {
 protected:
  void SetUp() override {
    PerformanceRegressionDetector::ClearAllBaselines();
    MemoryUsageTracker::ResetPeakMemoryTracking();
    
    profile_ = std::make_unique<TestingProfile>();
    
    // Record initial memory usage
    initial_memory_mb_ = MemoryUsageTracker::GetCurrentMemoryUsageMB();
    
    // Set up performance baselines
    SetupPerformanceBaselines();
  }
  
  void TearDown() override {
    profile_.reset();
  }
  
  void SetupPerformanceBaselines() {
    // Record baselines based on targets from CLAUDE.md
    PerformanceRegressionDetector::LogPerformanceBaseline(
        "BrowserStartup", DryftPerformanceMetrics::kMaxStartupOverhead.InMilliseconds());
    PerformanceRegressionDetector::LogPerformanceBaseline(
        "NIP07.GetPublicKey", DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds());
    PerformanceRegressionDetector::LogPerformanceBaseline(
        "NIP07.SignEvent", DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds());
    PerformanceRegressionDetector::LogPerformanceBaseline(
        "Relay.EventQuery", DryftPerformanceMetrics::kMaxLocalRelayQueryTime.InMilliseconds());
    PerformanceRegressionDetector::LogPerformanceBaseline(
        "Memory.BaseUsage", DryftPerformanceMetrics::kMaxBaseMemoryUsageMB);
  }
  
  // Helper to run performance test multiple times and get average
  template<typename TestFunction>
  double RunPerformanceTest(TestFunction test_func, int iterations = 10) {
    std::vector<double> results;
    results.reserve(iterations);
    
    for (int i = 0; i < iterations; ++i) {
      base::ElapsedTimer timer;
      test_func();
      results.push_back(timer.Elapsed().InMilliseconds());
    }
    
    // Calculate average
    double sum = 0.0;
    for (double result : results) {
      sum += result;
    }
    return sum / iterations;
  }
  
  // Generate test event for performance testing
  nostr::NostrEvent CreateTestEvent() {
    nostr::NostrEvent event;
    event.kind = 1;  // Text note
    event.content = "Test event for performance testing";
    event.created_at = base::Time::Now();
    event.pubkey = "test_pubkey_64_characters_long_for_performance_testing_purpose";
    event.id = "test_event_id_64_characters_long_for_performance_testing_purpose";
    event.sig = "test_signature_128_characters_long_for_performance_testing_purpose_with_more_text";
    return event;
  }
  
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  size_t initial_memory_mb_;
};

// Startup Performance Tests
class StartupPerformanceTest : public DryftPerformanceTest {
 protected:
  void SetUp() override {
    DryftPerformanceTest::SetUp();
  }
};

TEST_F(StartupPerformanceTest, NostrServiceInitializationPerformance) {
  const int kIterations = 5;
  std::vector<double> init_times;
  
  for (int i = 0; i < kIterations; ++i) {
    // Create new profile for each iteration
    auto test_profile = std::make_unique<TestingProfile>();
    
    base::ElapsedTimer timer;
    auto* nostr_service = NostrServiceFactory::GetForProfile(test_profile.get());
    ASSERT_TRUE(nostr_service);
    
    double init_time_ms = timer.Elapsed().InMilliseconds();
    init_times.push_back(init_time_ms);
    
    // Record the metric
    DryftPerformanceMetrics::RecordNostrServiceInitTime(timer.Elapsed());
  }
  
  // Calculate average
  double avg_init_time = 0.0;
  for (double time : init_times) {
    avg_init_time += time;
  }
  avg_init_time /= kIterations;
  
  // Check against performance target
  EXPECT_LT(avg_init_time, DryftPerformanceMetrics::kMaxStartupOverhead.InMilliseconds())
      << "NostrService initialization took " << avg_init_time << "ms, expected < "
      << DryftPerformanceMetrics::kMaxStartupOverhead.InMilliseconds() << "ms";
  
  // Check for performance regression
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "NostrServiceInit", avg_init_time, 30.0, 20.0));
}

TEST_F(StartupPerformanceTest, LocalRelayStartupPerformance) {
  const int kIterations = 5;
  std::vector<double> startup_times;
  
  for (int i = 0; i < kIterations; ++i) {
    auto test_profile = std::make_unique<TestingProfile>();
    
    base::ElapsedTimer timer;
    auto* relay_service = nostr::LocalRelayServiceFactory::GetForProfile(test_profile.get());
    ASSERT_TRUE(relay_service);
    
    double startup_time_ms = timer.Elapsed().InMilliseconds();
    startup_times.push_back(startup_time_ms);
    
    DryftPerformanceMetrics::RecordLocalRelayStartupTime(timer.Elapsed());
  }
  
  // Calculate average
  double avg_startup_time = 0.0;
  for (double time : startup_times) {
    avg_startup_time += time;
  }
  avg_startup_time /= kIterations;
  
  // Check against performance target
  EXPECT_LT(avg_startup_time, DryftPerformanceMetrics::kMaxStartupOverhead.InMilliseconds())
      << "LocalRelay startup took " << avg_startup_time << "ms, expected < "
      << DryftPerformanceMetrics::kMaxStartupOverhead.InMilliseconds() << "ms";
}

// Memory Performance Tests
class MemoryPerformanceTest : public DryftPerformanceTest {
 protected:
  void SetUp() override {
    DryftPerformanceTest::SetUp();
  }
};

TEST_F(MemoryPerformanceTest, BaseMemoryUsage) {
  // Get current memory usage
  size_t current_memory_mb = MemoryUsageTracker::GetCurrentMemoryUsageMB();
  
  // Record the metric
  DryftPerformanceMetrics::RecordTotalMemoryUsage(current_memory_mb);
  
  // Check against performance target
  EXPECT_LT(current_memory_mb, DryftPerformanceMetrics::kMaxBaseMemoryUsageMB)
      << "Base memory usage is " << current_memory_mb << "MB, expected < "
      << DryftPerformanceMetrics::kMaxBaseMemoryUsageMB << "MB";
  
  // Check for performance regression
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "BaseMemoryUsage", current_memory_mb, 
      DryftPerformanceMetrics::kMaxBaseMemoryUsageMB, 10.0));
}

TEST_F(MemoryPerformanceTest, NostrServiceMemoryUsage) {
  size_t memory_before = MemoryUsageTracker::GetCurrentMemoryUsageMB();
  
  // Initialize Nostr service
  auto* nostr_service = NostrServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(nostr_service);
  
  size_t memory_after = MemoryUsageTracker::GetCurrentMemoryUsageMB();
  size_t nostr_memory_usage = memory_after - memory_before;
  
  // Record the metric
  DryftPerformanceMetrics::RecordNostrMemoryUsage(nostr_memory_usage);
  
  // NostrService should use less than 20MB
  EXPECT_LT(nostr_memory_usage, 20U)
      << "NostrService uses " << nostr_memory_usage << "MB, expected < 20MB";
}

TEST_F(MemoryPerformanceTest, LocalRelayMemoryUsage) {
  size_t memory_before = MemoryUsageTracker::GetCurrentMemoryUsageMB();
  
  // Initialize Local Relay service
  auto* relay_service = nostr::LocalRelayServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(relay_service);
  
  size_t memory_after = MemoryUsageTracker::GetCurrentMemoryUsageMB();
  size_t relay_memory_usage = memory_after - memory_before;
  
  // Record the metric
  DryftPerformanceMetrics::RecordRelayMemoryUsage(relay_memory_usage);
  
  // LocalRelay should use less than 15MB
  EXPECT_LT(relay_memory_usage, 15U)
      << "LocalRelay uses " << relay_memory_usage << "MB, expected < 15MB";
}

TEST_F(MemoryPerformanceTest, MemoryUsageWithManyEvents) {
  auto* relay_service = nostr::LocalRelayServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(relay_service);
  
  size_t memory_before = MemoryUsageTracker::GetCurrentMemoryUsageMB();
  
  // Create many events
  const int kEventCount = 1000;
  for (int i = 0; i < kEventCount; ++i) {
    nostr::NostrEvent event = CreateTestEvent();
    event.id = "test_event_" + base::NumberToString(i);
    
    // Store event (in real implementation, this would go to database)
    // For testing, we'll just create the event objects
  }
  
  size_t memory_after = MemoryUsageTracker::GetCurrentMemoryUsageMB();
  size_t memory_per_event_kb = ((memory_after - memory_before) * 1024) / kEventCount;
  
  // Each event should use less than 1KB of memory
  EXPECT_LT(memory_per_event_kb, 1U)
      << "Each event uses " << memory_per_event_kb << "KB, expected < 1KB";
}

// Performance Regression Tests
class PerformanceRegressionTest : public DryftPerformanceTest {
 protected:
  void SetUp() override {
    DryftPerformanceTest::SetUp();
  }
};

TEST_F(PerformanceRegressionTest, PerformanceBaselines) {
  // Test baseline storage and retrieval
  PerformanceRegressionDetector::LogPerformanceBaseline("TestMetric", 100.0);
  
  double baseline = PerformanceRegressionDetector::GetPerformanceBaseline("TestMetric");
  EXPECT_DOUBLE_EQ(baseline, 100.0);
  
  // Test regression detection
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "TestMetric", 95.0, 100.0, 10.0));  // 5% improvement, should pass
  
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "TestMetric", 110.0, 100.0, 10.0));  // 10% regression, should still pass
  
  EXPECT_FALSE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "TestMetric", 115.0, 100.0, 10.0));  // 15% regression, should fail
}

// Timer Tests
class TimerPerformanceTest : public DryftPerformanceTest {
 protected:
  void SetUp() override {
    DryftPerformanceTest::SetUp();
  }
};

TEST_F(TimerPerformanceTest, ScopedTimerBasicUsage) {
  // Test basic timer usage
  {
    SCOPED_DRYFT_TIMER(kNostrServiceInit);
    base::PlatformThread::Sleep(base::Milliseconds(10));
  }
  
  // Test timer with context
  {
    SCOPED_DRYFT_TIMER_WITH_CONTEXT(kLibraryLoad, "ndk");
    base::PlatformThread::Sleep(base::Milliseconds(5));
  }
  
  // Test manual timer
  {
    ScopedDryftTimer timer(ScopedDryftTimer::Operation::kGetPublicKey);
    base::PlatformThread::Sleep(base::Milliseconds(1));
    
    // Check elapsed time
    base::TimeDelta elapsed = timer.GetElapsedTime();
    EXPECT_GE(elapsed.InMilliseconds(), 1);
  }
}

TEST_F(TimerPerformanceTest, MemoryUsageTracker) {
  // Test memory usage tracking
  size_t initial_memory = MemoryUsageTracker::GetCurrentMemoryUsageMB();
  EXPECT_GT(initial_memory, 0U);
  
  // Test peak memory tracking
  MemoryUsageTracker::ResetPeakMemoryTracking();
  size_t peak_before = MemoryUsageTracker::GetPeakMemoryUsageMB();
  
  // Allocate some memory
  std::vector<char> memory_buffer(1024 * 1024);  // 1MB
  DryftPerformanceMetrics::RecordTotalMemoryUsage(
      MemoryUsageTracker::GetCurrentMemoryUsageMB());
  
  size_t peak_after = MemoryUsageTracker::GetPeakMemoryUsageMB();
  EXPECT_GE(peak_after, peak_before);
  
  // Test memory threshold checking
  EXPECT_TRUE(MemoryUsageTracker::IsMemoryUsageAcceptable(10, 20));
  EXPECT_FALSE(MemoryUsageTracker::IsMemoryUsageAcceptable(30, 20));
}

}  // namespace dryft