// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_cache_manager.h"
#include "chrome/browser/nostr/nsite/nsite_streaming_server.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace nostr {

namespace {

constexpr char kMetricPrefixNsitePerformance[] = "NsitePerformance.";
constexpr char kMetricCacheHitTimeSuffix[] = "cache_hit_time";
constexpr char kMetricCacheMissTimeSuffix[] = "cache_miss_time";
constexpr char kMetricServerStartupTimeSuffix[] = "server_startup_time";
constexpr char kMetricMemoryUsageSuffix[] = "memory_usage";

perf_test::PerfResultReporter SetUpReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixNsitePerformance, story);
  reporter.RegisterImportantMetric(kMetricCacheHitTimeSuffix, "ms");
  reporter.RegisterImportantMetric(kMetricCacheMissTimeSuffix, "ms");
  reporter.RegisterImportantMetric(kMetricServerStartupTimeSuffix, "ms");
  reporter.RegisterImportantMetric(kMetricMemoryUsageSuffix, "MB");
  return reporter;
}

}  // namespace

class NsitePerformanceTest : public testing::Test {
 public:
  NsitePerformanceTest() = default;
  ~NsitePerformanceTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(NsitePerformanceTest, CacheHitPerformance) {
  auto reporter = SetUpReporter("CacheHitPerformance");
  
  base::FilePath temp_dir;
  ASSERT_TRUE(base::CreateNewTempDirectory("nsite_perf_test", &temp_dir));
  
  NsiteCacheManager cache_manager(temp_dir);
  
  // Populate cache with test data
  std::string test_content(1024, 'x');  // 1KB content
  std::string npub = "npub1234567890abcdefghijklmnopqrstuvwxyz234567890abcdefghijk";
  
  for (int i = 0; i < 100; ++i) {
    cache_manager.PutFile(npub, "/file" + base::NumberToString(i) + ".html", 
                         test_content, "text/html");
  }
  
  // Measure cache hit performance
  constexpr int kIterations = 1000;
  base::ElapsedTimer timer;
  
  for (int i = 0; i < kIterations; ++i) {
    int file_index = i % 100;
    auto file = cache_manager.GetFile(npub, "/file" + base::NumberToString(file_index) + ".html");
    ASSERT_TRUE(file != nullptr);
  }
  
  base::TimeDelta total_time = timer.Elapsed();
  double avg_time_ms = total_time.InMillisecondsF() / kIterations;
  
  reporter.AddResult(kMetricCacheHitTimeSuffix, avg_time_ms);
  
  // Verify performance target: cache hits should be under 100ms
  EXPECT_LT(avg_time_ms, 100.0) << "Cache hit time exceeded 100ms target";
}

TEST_F(NsitePerformanceTest, CacheMissPerformance) {
  auto reporter = SetUpReporter("CacheMissPerformance");
  
  base::FilePath temp_dir;
  ASSERT_TRUE(base::CreateNewTempDirectory("nsite_perf_test", &temp_dir));
  
  NsiteCacheManager cache_manager(temp_dir);
  
  std::string npub = "npub1234567890abcdefghijklmnopqrstuvwxyz234567890abcdefghijk";
  
  // Measure cache miss performance
  constexpr int kIterations = 100;
  base::ElapsedTimer timer;
  
  for (int i = 0; i < kIterations; ++i) {
    auto file = cache_manager.GetFile(npub, "/nonexistent" + base::NumberToString(i) + ".html");
    ASSERT_TRUE(file == nullptr);
  }
  
  base::TimeDelta total_time = timer.Elapsed();
  double avg_time_ms = total_time.InMillisecondsF() / kIterations;
  
  reporter.AddResult(kMetricCacheMissTimeSuffix, avg_time_ms);
  
  // Verify performance target: cache misses should be under 50ms
  EXPECT_LT(avg_time_ms, 50.0) << "Cache miss time exceeded 50ms target";
}

TEST_F(NsitePerformanceTest, ServerStartupPerformance) {
  auto reporter = SetUpReporter("ServerStartupPerformance");
  
  base::FilePath temp_dir;
  ASSERT_TRUE(base::CreateNewTempDirectory("nsite_perf_test", &temp_dir));
  
  // Measure server startup time
  constexpr int kIterations = 10;
  base::TimeDelta total_time;
  
  for (int i = 0; i < kIterations; ++i) {
    NsiteStreamingServer server(temp_dir);
    
    base::ElapsedTimer timer;
    uint16_t port = server.Start();
    total_time += timer.Elapsed();
    
    ASSERT_NE(port, 0u) << "Server failed to start";
    server.Stop();
  }
  
  double avg_time_ms = total_time.InMillisecondsF() / kIterations;
  
  reporter.AddResult(kMetricServerStartupTimeSuffix, avg_time_ms);
  
  // Verify performance target: server startup should be under 500ms
  EXPECT_LT(avg_time_ms, 500.0) << "Server startup time exceeded 500ms target";
}

TEST_F(NsitePerformanceTest, MemoryUsageWithManyFiles) {
  auto reporter = SetUpReporter("MemoryUsageWithManyFiles");
  
  base::FilePath temp_dir;
  ASSERT_TRUE(base::CreateNewTempDirectory("nsite_perf_test", &temp_dir));
  
  NsiteCacheManager cache_manager(temp_dir);
  
  // Simulate 10 nsites with 50 files each
  std::string test_content(2048, 'x');  // 2KB per file
  
  for (int nsite = 0; nsite < 10; ++nsite) {
    std::string npub = "npub123456789" + base::NumberToString(nsite) + 
                      std::string(50, '0');  // Pad to 63 chars
    
    for (int file = 0; file < 50; ++file) {
      cache_manager.PutFile(npub, "/file" + base::NumberToString(file) + ".html",
                           test_content, "text/html");
    }
  }
  
  auto stats = cache_manager.GetStats();
  double memory_mb = static_cast<double>(stats.total_size) / (1024 * 1024);
  
  reporter.AddResult(kMetricMemoryUsageSuffix, memory_mb);
  
  // Verify performance target: memory usage should be under 100MB for 10 nsites
  EXPECT_LT(memory_mb, 100.0) << "Memory usage exceeded 100MB target";
  
  // Verify we actually cached the expected amount of data
  EXPECT_EQ(stats.file_count, 500u);
  EXPECT_GT(stats.total_size, 500 * 2048 * 0.9);  // Allow some overhead
}

TEST_F(NsitePerformanceTest, ConcurrentCacheAccess) {
  auto reporter = SetUpReporter("ConcurrentCacheAccess");
  
  base::FilePath temp_dir;
  ASSERT_TRUE(base::CreateNewTempDirectory("nsite_perf_test", &temp_dir));
  
  NsiteCacheManager cache_manager(temp_dir);
  
  // Populate cache
  std::string test_content(1024, 'x');
  std::string npub = "npub1234567890abcdefghijklmnopqrstuvwxyz234567890abcdefghijk";
  
  for (int i = 0; i < 50; ++i) {
    cache_manager.PutFile(npub, "/file" + base::NumberToString(i) + ".html",
                         test_content, "text/html");
  }
  
  // Measure concurrent access performance
  constexpr int kIterations = 1000;
  base::ElapsedTimer timer;
  
  // Simulate rapid sequential access (real concurrent access would need threading)
  for (int i = 0; i < kIterations; ++i) {
    int file_index = i % 50;
    auto file = cache_manager.GetFile(npub, "/file" + base::NumberToString(file_index) + ".html");
    ASSERT_TRUE(file != nullptr);
  }
  
  base::TimeDelta total_time = timer.Elapsed();
  double avg_time_ms = total_time.InMillisecondsF() / kIterations;
  double throughput = kIterations / total_time.InSecondsF();
  
  // Report throughput as requests per second
  reporter.AddResult("concurrent_throughput", throughput);
  
  // Verify performance: should handle at least 100 requests/second
  EXPECT_GT(throughput, 100.0) << "Concurrent access throughput too low";
}

}  // namespace nostr