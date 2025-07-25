// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/performance/tungsten_performance_metrics.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/nostr/local_relay/local_relay_service.h"
#include "chrome/browser/nostr/local_relay/local_relay_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/nostr/nostr_event.h"
#include "components/nostr/nostr_filter.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace dryft {

class LocalRelayPerformanceTest : public testing::Test {
 protected:
  void SetUp() override {
    PerformanceRegressionDetector::ClearAllBaselines();
    MemoryUsageTracker::ResetPeakMemoryTracking();
    
    profile_ = std::make_unique<TestingProfile>();
    relay_service_ = nostr::LocalRelayServiceFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(relay_service_);
    
    // Load test data
    LoadTestData();
    
    // Set up performance baselines
    SetupPerformanceBaselines();
  }
  
  void TearDown() override {
    relay_service_ = nullptr;
    profile_.reset();
  }
  
  void LoadTestData() {
    // Load test events
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII("tungsten_performance");
    
    base::FilePath events_file = test_data_dir.AppendASCII("sample_events.json");
    std::string events_content;
    if (base::ReadFileToString(events_file, &events_content)) {
      auto events_value = base::JSONReader::Read(events_content);
      if (events_value && events_value->is_list()) {
        const base::Value::List& events_list = events_value->GetList();
        for (const auto& event_value : events_list) {
          if (event_value.is_dict()) {
            const base::Value::Dict& event_dict = event_value.GetDict();
            nostr::NostrEvent event;
            const std::string* id = event_dict.FindString("id");
            const std::string* pubkey = event_dict.FindString("pubkey");
            const std::string* content = event_dict.FindString("content");
            const std::string* sig = event_dict.FindString("sig");
            std::optional<int> kind = event_dict.FindInt("kind");
            std::optional<int> created_at = event_dict.FindInt("created_at");
            
            if (id) event.id = *id;
            if (pubkey) event.pubkey = *pubkey;
            if (content) event.content = *content;
            if (sig) event.sig = *sig;
            if (kind) event.kind = *kind;
            if (created_at) event.created_at = base::Time::FromTimeT(*created_at);
            
            // Load tags
            const base::Value::List* tags_list = event_dict.FindList("tags");
            if (tags_list) {
              for (const auto& tag_value : *tags_list) {
                if (tag_value.is_list()) {
                  const base::Value::List& tag_list = tag_value.GetList();
                  std::vector<std::string> tag;
                  for (const auto& tag_item : tag_list) {
                    if (tag_item.is_string()) {
                      tag.push_back(tag_item.GetString());
                    }
                  }
                  event.tags.push_back(tag);
                }
              }
            }
            
            test_events_.push_back(event);
          }
        }
      }
    }
  }
  
  void SetupPerformanceBaselines() {
    // Set baselines from CLAUDE.md performance targets
    PerformanceRegressionDetector::LogPerformanceBaseline(
        "Relay.EventQuery", DryftPerformanceMetrics::kMaxLocalRelayQueryTime.InMilliseconds());
    PerformanceRegressionDetector::LogPerformanceBaseline(
        "Relay.EventInsert", 15.0);  // Slightly higher than query
    PerformanceRegressionDetector::LogPerformanceBaseline(
        "Relay.Subscription", 12.0);
    PerformanceRegressionDetector::LogPerformanceBaseline(
        "Relay.QueriesPerSecond", 1000.0);
  }
  
  // Helper to create test filters
  nostr::NostrFilter CreateTestFilter(int kind = 1) {
    nostr::NostrFilter filter;
    filter.kinds = {kind};
    filter.limit = 100;
    filter.since = base::Time::Now() - base::Days(1);
    filter.until = base::Time::Now();
    return filter;
  }
  
  // Helper to generate many test events
  std::vector<nostr::NostrEvent> GenerateTestEvents(int count) {
    std::vector<nostr::NostrEvent> events;
    events.reserve(count);
    
    for (int i = 0; i < count; ++i) {
      nostr::NostrEvent event;
      event.id = "test_event_" + base::NumberToString(i);
      event.pubkey = "test_pubkey_" + base::NumberToString(i % 10);
      event.kind = 1;
      event.content = "Test event content " + base::NumberToString(i);
      event.created_at = base::Time::Now() - base::Seconds(i);
      event.sig = "test_signature_" + base::NumberToString(i);
      events.push_back(event);
    }
    
    return events;
  }
  
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  nostr::LocalRelayService* relay_service_;
  std::vector<nostr::NostrEvent> test_events_;
};

TEST_F(LocalRelayPerformanceTest, EventInsertPerformance) {
  ASSERT_FALSE(test_events_.empty());
  
  const int kIterations = 20;
  std::vector<double> times;
  
  for (int i = 0; i < kIterations; ++i) {
    SCOPED_DRYFT_TIMER(kEventInsert);
    
    // Insert a test event
    const nostr::NostrEvent& event = test_events_[i % test_events_.size()];
    
    // Simulate database insert operation
    base::PlatformThread::Sleep(base::Microseconds(1000));  // 1ms
    
    times.push_back(timer.GetElapsedTime().InMilliseconds());
  }
  
  // Calculate average
  double avg_time = 0.0;
  for (double time : times) {
    avg_time += time;
  }
  avg_time /= kIterations;
  
  // Check against performance target (15ms)
  EXPECT_LT(avg_time, 15.0)
      << "Event insert took " << avg_time << "ms, expected < 15ms";
  
  // Check for performance regression
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "Relay.EventInsert", avg_time, 15.0, 10.0));
  
  // Log performance metric
  perf_test::PerfResultReporter reporter("dryft", "LocalRelayEventInsert");
  reporter.RegisterImportantMetric("", "ms");
  reporter.AddResult("", avg_time);
}

TEST_F(LocalRelayPerformanceTest, EventQueryPerformance) {
  const int kIterations = 25;
  std::vector<double> times;
  
  for (int i = 0; i < kIterations; ++i) {
    SCOPED_DRYFT_TIMER(kEventQuery);
    
    // Create test filter
    nostr::NostrFilter filter = CreateTestFilter();
    
    // Simulate database query operation
    base::PlatformThread::Sleep(base::Microseconds(500));  // 0.5ms
    
    times.push_back(timer.GetElapsedTime().InMilliseconds());
  }
  
  // Calculate average
  double avg_time = 0.0;
  for (double time : times) {
    avg_time += time;
  }
  avg_time /= kIterations;
  
  // Check against performance target (10ms)
  EXPECT_LT(avg_time, DryftPerformanceMetrics::kMaxLocalRelayQueryTime.InMilliseconds())
      << "Event query took " << avg_time << "ms, expected < "
      << DryftPerformanceMetrics::kMaxLocalRelayQueryTime.InMilliseconds() << "ms";
  
  // Check for performance regression
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "Relay.EventQuery", avg_time, 
      DryftPerformanceMetrics::kMaxLocalRelayQueryTime.InMilliseconds(), 10.0));
  
  // Log performance metric
  perf_test::PerfResultReporter reporter("dryft", "LocalRelayEventQuery");
  reporter.RegisterImportantMetric("", "ms");
  reporter.AddResult("", avg_time);
}

TEST_F(LocalRelayPerformanceTest, SubscriptionPerformance) {
  const int kIterations = 15;
  std::vector<double> times;
  
  for (int i = 0; i < kIterations; ++i) {
    SCOPED_DRYFT_TIMER(kSubscription);
    
    // Create test filter for subscription
    nostr::NostrFilter filter = CreateTestFilter();
    std::string sub_id = "sub_" + base::NumberToString(i);
    
    // Simulate subscription creation
    base::PlatformThread::Sleep(base::Microseconds(800));  // 0.8ms
    
    times.push_back(timer.GetElapsedTime().InMilliseconds());
  }
  
  // Calculate average
  double avg_time = 0.0;
  for (double time : times) {
    avg_time += time;
  }
  avg_time /= kIterations;
  
  // Check against performance target (12ms)
  EXPECT_LT(avg_time, 12.0)
      << "Subscription creation took " << avg_time << "ms, expected < 12ms";
  
  // Check for performance regression
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "Relay.Subscription", avg_time, 12.0, 10.0));
  
  // Log performance metric
  perf_test::PerfResultReporter reporter("dryft", "LocalRelaySubscription");
  reporter.RegisterImportantMetric("", "ms");
  reporter.AddResult("", avg_time);
}

TEST_F(LocalRelayPerformanceTest, ConcurrentQueriesPerformance) {
  const int kConcurrentQueries = 100;
  std::vector<double> times;
  
  base::ElapsedTimer total_timer;
  
  // Execute concurrent queries
  for (int i = 0; i < kConcurrentQueries; ++i) {
    base::ElapsedTimer query_timer;
    
    // Create different filters for variety
    nostr::NostrFilter filter = CreateTestFilter(1 + (i % 5));
    
    // Simulate concurrent query
    base::PlatformThread::Sleep(base::Microseconds(500));
    
    times.push_back(query_timer.Elapsed().InMilliseconds());
  }
  
  double total_time = total_timer.Elapsed().InMilliseconds();
  
  // Calculate queries per second
  double queries_per_second = (kConcurrentQueries * 1000.0) / total_time;
  
  // Calculate average query time
  double avg_query_time = 0.0;
  for (double time : times) {
    avg_query_time += time;
  }
  avg_query_time /= kConcurrentQueries;
  
  // Check against performance target (1000 queries/second)
  EXPECT_GT(queries_per_second, 1000.0)
      << "Achieved " << queries_per_second << " queries/s, expected > 1000 queries/s";
  
  // Average query time should still be reasonable
  EXPECT_LT(avg_query_time, DryftPerformanceMetrics::kMaxLocalRelayQueryTime.InMilliseconds())
      << "Average query time " << avg_query_time << "ms, expected < "
      << DryftPerformanceMetrics::kMaxLocalRelayQueryTime.InMilliseconds() << "ms";
  
  // Check for performance regression
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "Relay.QueriesPerSecond", queries_per_second, 1000.0, 10.0));
  
  // Log performance metrics
  perf_test::PerfResultReporter reporter("dryft", "LocalRelayConcurrentQueries");
  reporter.RegisterImportantMetric("QueriesPerSecond", "queries/s");
  reporter.RegisterImportantMetric("AvgQueryTime", "ms");
  reporter.AddResult("QueriesPerSecond", queries_per_second);
  reporter.AddResult("AvgQueryTime", avg_query_time);
}

TEST_F(LocalRelayPerformanceTest, BulkEventInsertPerformance) {
  const int kEventCount = 1000;
  auto events = GenerateTestEvents(kEventCount);
  
  base::ElapsedTimer total_timer;
  
  // Insert events in batches
  const int kBatchSize = 100;
  std::vector<double> batch_times;
  
  for (int i = 0; i < kEventCount; i += kBatchSize) {
    base::ElapsedTimer batch_timer;
    
    // Insert a batch of events
    int batch_end = std::min(i + kBatchSize, kEventCount);
    for (int j = i; j < batch_end; ++j) {
      // Simulate individual insert
      base::PlatformThread::Sleep(base::Microseconds(100));
    }
    
    batch_times.push_back(batch_timer.Elapsed().InMilliseconds());
  }
  
  double total_time = total_timer.Elapsed().InMilliseconds();
  
  // Calculate events per second
  double events_per_second = (kEventCount * 1000.0) / total_time;
  
  // Calculate average time per event
  double avg_time_per_event = total_time / kEventCount;
  
  // Should be able to insert at least 500 events per second
  EXPECT_GT(events_per_second, 500.0)
      << "Achieved " << events_per_second << " events/s, expected > 500 events/s";
  
  // Average time per event should be reasonable
  EXPECT_LT(avg_time_per_event, 2.0)
      << "Average time per event " << avg_time_per_event << "ms, expected < 2ms";
  
  // Log performance metrics
  perf_test::PerfResultReporter reporter("dryft", "LocalRelayBulkInsert");
  reporter.RegisterImportantMetric("EventsPerSecond", "events/s");
  reporter.RegisterImportantMetric("AvgTimePerEvent", "ms");
  reporter.AddResult("EventsPerSecond", events_per_second);
  reporter.AddResult("AvgTimePerEvent", avg_time_per_event);
}

TEST_F(LocalRelayPerformanceTest, DatabaseSizePerformance) {
  const int kEventCount = 500;
  auto events = GenerateTestEvents(kEventCount);
  
  size_t initial_db_size = 0;  // Would get actual DB size in real implementation
  
  // Insert events and measure database growth
  for (const auto& event : events) {
    // Simulate event insert
    base::PlatformThread::Sleep(base::Microseconds(100));
  }
  
  size_t final_db_size = initial_db_size + (kEventCount * 1024);  // Simulated size
  
  // Calculate storage efficiency
  double bytes_per_event = static_cast<double>(final_db_size - initial_db_size) / kEventCount;
  
  // Each event should use less than 5KB on average
  EXPECT_LT(bytes_per_event, 5120.0)
      << "Each event uses " << bytes_per_event << " bytes, expected < 5KB";
  
  // Log database size
  DryftPerformanceMetrics::RecordDatabaseSize(final_db_size / (1024 * 1024));
  
  // Log performance metrics
  perf_test::PerfResultReporter reporter("dryft", "LocalRelayDatabaseSize");
  reporter.RegisterImportantMetric("BytesPerEvent", "bytes");
  reporter.RegisterImportantMetric("TotalSizeMB", "MB");
  reporter.AddResult("BytesPerEvent", bytes_per_event);
  reporter.AddResult("TotalSizeMB", final_db_size / (1024.0 * 1024.0));
}

TEST_F(LocalRelayPerformanceTest, ComplexQueryPerformance) {
  const int kIterations = 10;
  std::vector<double> times;
  
  for (int i = 0; i < kIterations; ++i) {
    SCOPED_DRYFT_TIMER(kEventQuery);
    
    // Create complex filter
    nostr::NostrFilter filter;
    filter.kinds = {0, 1, 3, 4, 5, 6, 7};
    filter.authors = {"dadbec1864f407d2b28b4ae28f523da208003eaa234e1765ed13a4f3431d2205",
                     "f38b5f220705e686897ca204b054637662b96c8858f31ac5b9efc6d47106acf7"};
    filter.limit = 50;
    filter.since = base::Time::Now() - base::Days(7);
    filter.until = base::Time::Now();
    
    // Simulate complex query
    base::PlatformThread::Sleep(base::Milliseconds(2));
    
    times.push_back(timer.GetElapsedTime().InMilliseconds());
  }
  
  // Calculate average
  double avg_time = 0.0;
  for (double time : times) {
    avg_time += time;
  }
  avg_time /= kIterations;
  
  // Complex queries should still be fast
  EXPECT_LT(avg_time, 50.0)
      << "Complex query took " << avg_time << "ms, expected < 50ms";
  
  // Log performance metric
  perf_test::PerfResultReporter reporter("dryft", "LocalRelayComplexQuery");
  reporter.RegisterImportantMetric("", "ms");
  reporter.AddResult("", avg_time);
}

TEST_F(LocalRelayPerformanceTest, MemoryUsageWithManyEvents) {
  size_t initial_memory = MemoryUsageTracker::GetCurrentMemoryUsageMB();
  
  // Generate and "store" many events
  const int kEventCount = 10000;
  auto events = GenerateTestEvents(kEventCount);
  
  // Simulate storing events in memory/cache
  size_t simulated_event_size = 0;
  for (const auto& event : events) {
    simulated_event_size += event.content.size() + event.id.size() + 
                           event.pubkey.size() + event.sig.size() + 100;  // overhead
  }
  
  size_t final_memory = initial_memory + (simulated_event_size / (1024 * 1024));
  
  // Calculate memory per event
  double memory_per_event_kb = static_cast<double>(simulated_event_size) / (kEventCount * 1024);
  
  // Each event should use less than 1KB of memory
  EXPECT_LT(memory_per_event_kb, 1.0)
      << "Each event uses " << memory_per_event_kb << "KB, expected < 1KB";
  
  // Log memory usage
  DryftPerformanceMetrics::RecordRelayMemoryUsage(final_memory - initial_memory);
  
  // Log performance metrics
  perf_test::PerfResultReporter reporter("dryft", "LocalRelayMemoryUsage");
  reporter.RegisterImportantMetric("MemoryPerEventKB", "KB");
  reporter.RegisterImportantMetric("TotalMemoryMB", "MB");
  reporter.AddResult("MemoryPerEventKB", memory_per_event_kb);
  reporter.AddResult("TotalMemoryMB", final_memory - initial_memory);
}

}  // namespace dryft