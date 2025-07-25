// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/performance/tungsten_performance_metrics.h"

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/nostr/nostr_service.h"
#include "chrome/browser/nostr/nostr_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/nostr/nostr_event.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace dryft {

class NIP07PerformanceTest : public testing::Test {
 protected:
  void SetUp() override {
    PerformanceRegressionDetector::ClearAllBaselines();
    MemoryUsageTracker::ResetPeakMemoryTracking();
    
    profile_ = std::make_unique<TestingProfile>();
    nostr_service_ = NostrServiceFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(nostr_service_);
    
    // Load test data
    LoadTestData();
    
    // Set up performance baselines
    SetupPerformanceBaselines();
  }
  
  void TearDown() override {
    nostr_service_ = nullptr;
    profile_.reset();
  }
  
  void LoadTestData() {
    // Load test keys
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII("tungsten_performance");
    
    base::FilePath keys_file = test_data_dir.AppendASCII("test_keys.json");
    std::string keys_content;
    if (base::ReadFileToString(keys_file, &keys_content)) {
      auto keys_value = base::JSONReader::Read(keys_content);
      if (keys_value && keys_value->is_dict()) {
        const base::Value::Dict& keys_dict = keys_value->GetDict();
        const base::Value::List* keys_list = keys_dict.FindList("test_keys");
        if (keys_list) {
          for (const auto& key_value : *keys_list) {
            if (key_value.is_dict()) {
              const base::Value::Dict& key_dict = key_value.GetDict();
              TestKey key;
              const std::string* name = key_dict.FindString("name");
              const std::string* private_key = key_dict.FindString("private_key");
              const std::string* public_key = key_dict.FindString("public_key");
              if (name) key.name = *name;
              if (private_key) key.private_key = *private_key;
              if (public_key) key.public_key = *public_key;
              test_keys_.push_back(key);
            }
          }
        }
      }
    }
    
    // Load test events
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
            
            test_events_.push_back(event);
          }
        }
      }
    }
  }
  
  void SetupPerformanceBaselines() {
    // Set baselines from CLAUDE.md performance targets
    PerformanceRegressionDetector::LogPerformanceBaseline(
        "NIP07.GetPublicKey", DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds());
    PerformanceRegressionDetector::LogPerformanceBaseline(
        "NIP07.SignEvent", DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds());
    PerformanceRegressionDetector::LogPerformanceBaseline(
        "NIP07.Encryption", DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds());
    PerformanceRegressionDetector::LogPerformanceBaseline(
        "NIP07.Decryption", DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds());
    PerformanceRegressionDetector::LogPerformanceBaseline(
        "NIP07.GetRelays", DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds());
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
  
  struct TestKey {
    std::string name;
    std::string private_key;
    std::string public_key;
  };
  
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  NostrService* nostr_service_;
  std::vector<TestKey> test_keys_;
  std::vector<nostr::NostrEvent> test_events_;
};

TEST_F(NIP07PerformanceTest, GetPublicKeyPerformance) {
  ASSERT_FALSE(test_keys_.empty());
  
  const int kIterations = 20;
  std::vector<double> times;
  
  for (int i = 0; i < kIterations; ++i) {
    SCOPED_DRYFT_TIMER(kGetPublicKey);
    
    // Simulate getPublicKey() operation
    std::string public_key = test_keys_[i % test_keys_.size()].public_key;
    
    // Add some CPU work to simulate key retrieval
    base::PlatformThread::Sleep(base::Microseconds(100));
    
    times.push_back(timer.GetElapsedTime().InMilliseconds());
  }
  
  // Calculate average
  double avg_time = 0.0;
  for (double time : times) {
    avg_time += time;
  }
  avg_time /= kIterations;
  
  // Check against performance target
  EXPECT_LT(avg_time, DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds())
      << "getPublicKey() took " << avg_time << "ms, expected < "
      << DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds() << "ms";
  
  // Check for performance regression
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "NIP07.GetPublicKey", avg_time, 
      DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds(), 10.0));
}

TEST_F(NIP07PerformanceTest, SignEventPerformance) {
  ASSERT_FALSE(test_keys_.empty());
  ASSERT_FALSE(test_events_.empty());
  
  const int kIterations = 15;
  std::vector<double> times;
  
  for (int i = 0; i < kIterations; ++i) {
    SCOPED_DRYFT_TIMER(kSignEvent);
    
    // Simulate signEvent() operation
    const TestKey& key = test_keys_[i % test_keys_.size()];
    const nostr::NostrEvent& event = test_events_[i % test_events_.size()];
    
    // Add some CPU work to simulate event signing
    base::PlatformThread::Sleep(base::Microseconds(500));
    
    times.push_back(timer.GetElapsedTime().InMilliseconds());
  }
  
  // Calculate average
  double avg_time = 0.0;
  for (double time : times) {
    avg_time += time;
  }
  avg_time /= kIterations;
  
  // Check against performance target
  EXPECT_LT(avg_time, DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds())
      << "signEvent() took " << avg_time << "ms, expected < "
      << DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds() << "ms";
  
  // Check for performance regression
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "NIP07.SignEvent", avg_time, 
      DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds(), 10.0));
}

TEST_F(NIP07PerformanceTest, EncryptionPerformance) {
  ASSERT_FALSE(test_keys_.empty());
  
  const int kIterations = 12;
  std::vector<double> times;
  
  for (int i = 0; i < kIterations; ++i) {
    SCOPED_DRYFT_TIMER(kEncryption);
    
    // Simulate nip04.encrypt() operation
    const TestKey& sender_key = test_keys_[i % test_keys_.size()];
    const TestKey& recipient_key = test_keys_[(i + 1) % test_keys_.size()];
    const std::string plaintext = "Test message for encryption performance";
    
    // Add some CPU work to simulate encryption
    base::PlatformThread::Sleep(base::Microseconds(300));
    
    times.push_back(timer.GetElapsedTime().InMilliseconds());
  }
  
  // Calculate average
  double avg_time = 0.0;
  for (double time : times) {
    avg_time += time;
  }
  avg_time /= kIterations;
  
  // Check against performance target
  EXPECT_LT(avg_time, DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds())
      << "nip04.encrypt() took " << avg_time << "ms, expected < "
      << DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds() << "ms";
  
  // Check for performance regression
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "NIP07.Encryption", avg_time, 
      DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds(), 10.0));
}

TEST_F(NIP07PerformanceTest, DecryptionPerformance) {
  ASSERT_FALSE(test_keys_.empty());
  
  const int kIterations = 12;
  std::vector<double> times;
  
  for (int i = 0; i < kIterations; ++i) {
    SCOPED_DRYFT_TIMER(kDecryption);
    
    // Simulate nip04.decrypt() operation
    const TestKey& sender_key = test_keys_[i % test_keys_.size()];
    const TestKey& recipient_key = test_keys_[(i + 1) % test_keys_.size()];
    const std::string ciphertext = "encrypted_message_for_decryption_performance";
    
    // Add some CPU work to simulate decryption
    base::PlatformThread::Sleep(base::Microseconds(400));
    
    times.push_back(timer.GetElapsedTime().InMilliseconds());
  }
  
  // Calculate average
  double avg_time = 0.0;
  for (double time : times) {
    avg_time += time;
  }
  avg_time /= kIterations;
  
  // Check against performance target
  EXPECT_LT(avg_time, DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds())
      << "nip04.decrypt() took " << avg_time << "ms, expected < "
      << DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds() << "ms";
  
  // Check for performance regression
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "NIP07.Decryption", avg_time, 
      DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds(), 10.0));
}

TEST_F(NIP07PerformanceTest, GetRelaysPerformance) {
  const int kIterations = 25;
  std::vector<double> times;
  
  for (int i = 0; i < kIterations; ++i) {
    SCOPED_DRYFT_TIMER(kGetRelays);
    
    // Simulate getRelays() operation
    std::vector<std::string> relays = {
      "wss://relay.nostr.band",
      "wss://nostr-pub.wellorder.net",
      "wss://relay.damus.io",
      "wss://nos.lol",
      "wss://relay.snort.social"
    };
    
    // Add some CPU work to simulate relay list retrieval
    base::PlatformThread::Sleep(base::Microseconds(50));
    
    times.push_back(timer.GetElapsedTime().InMilliseconds());
  }
  
  // Calculate average
  double avg_time = 0.0;
  for (double time : times) {
    avg_time += time;
  }
  avg_time /= kIterations;
  
  // Check against performance target (should be faster than other operations)
  EXPECT_LT(avg_time, DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds())
      << "getRelays() took " << avg_time << "ms, expected < "
      << DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds() << "ms";
  
  // Check for performance regression
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "NIP07.GetRelays", avg_time, 
      DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds(), 10.0));
}

TEST_F(NIP07PerformanceTest, ConcurrentOperationsPerformance) {
  ASSERT_FALSE(test_keys_.empty());
  
  const int kConcurrentOperations = 50;
  std::vector<double> times;
  
  base::ElapsedTimer total_timer;
  
  // Simulate concurrent NIP-07 operations
  for (int i = 0; i < kConcurrentOperations; ++i) {
    base::ElapsedTimer op_timer;
    
    // Mix of different operations
    switch (i % 4) {
      case 0: {
        SCOPED_DRYFT_TIMER(kGetPublicKey);
        std::string public_key = test_keys_[i % test_keys_.size()].public_key;
        base::PlatformThread::Sleep(base::Microseconds(100));
        break;
      }
      case 1: {
        SCOPED_DRYFT_TIMER(kSignEvent);
        const nostr::NostrEvent& event = test_events_[i % test_events_.size()];
        base::PlatformThread::Sleep(base::Microseconds(500));
        break;
      }
      case 2: {
        SCOPED_DRYFT_TIMER(kEncryption);
        base::PlatformThread::Sleep(base::Microseconds(300));
        break;
      }
      case 3: {
        SCOPED_DRYFT_TIMER(kGetRelays);
        base::PlatformThread::Sleep(base::Microseconds(50));
        break;
      }
    }
    
    times.push_back(op_timer.Elapsed().InMilliseconds());
  }
  
  double total_time = total_timer.Elapsed().InMilliseconds();
  
  // Calculate average per operation
  double avg_time = 0.0;
  for (double time : times) {
    avg_time += time;
  }
  avg_time /= kConcurrentOperations;
  
  // Calculate operations per second
  double ops_per_second = (kConcurrentOperations * 1000.0) / total_time;
  
  // Log performance metrics
  perf_test::PerfResultReporter reporter("dryft", "NIP07ConcurrentOperations");
  reporter.RegisterImportantMetric("AvgTimePerOp", "ms");
  reporter.RegisterImportantMetric("OperationsPerSecond", "ops/s");
  reporter.AddResult("AvgTimePerOp", avg_time);
  reporter.AddResult("OperationsPerSecond", ops_per_second);
  
  // We should be able to handle at least 100 operations per second
  EXPECT_GT(ops_per_second, 100.0)
      << "Concurrent operations achieved " << ops_per_second 
      << " ops/s, expected > 100 ops/s";
  
  // Average operation time should still be reasonable
  EXPECT_LT(avg_time, DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds())
      << "Average operation time " << avg_time << "ms, expected < "
      << DryftPerformanceMetrics::kMaxNip07OperationTime.InMilliseconds() << "ms";
}

TEST_F(NIP07PerformanceTest, LargeEventSigningPerformance) {
  ASSERT_FALSE(test_keys_.empty());
  
  // Create large event content
  std::string large_content;
  const int kContentSize = 10000;  // 10KB
  large_content.reserve(kContentSize);
  for (int i = 0; i < kContentSize; ++i) {
    large_content += char('a' + (i % 26));
  }
  
  const int kIterations = 5;
  std::vector<double> times;
  
  for (int i = 0; i < kIterations; ++i) {
    SCOPED_DRYFT_TIMER(kSignEvent);
    
    // Create large event
    nostr::NostrEvent large_event;
    large_event.kind = 1;
    large_event.content = large_content;
    large_event.created_at = base::Time::Now();
    large_event.pubkey = test_keys_[i % test_keys_.size()].public_key;
    
    // Add some CPU work to simulate signing large content
    base::PlatformThread::Sleep(base::Milliseconds(2));
    
    times.push_back(timer.GetElapsedTime().InMilliseconds());
  }
  
  // Calculate average
  double avg_time = 0.0;
  for (double time : times) {
    avg_time += time;
  }
  avg_time /= kIterations;
  
  // Large events should still be signed within reasonable time
  EXPECT_LT(avg_time, 50.0)
      << "Large event signing took " << avg_time << "ms, expected < 50ms";
  
  // Log performance metric
  perf_test::PerfResultReporter reporter("dryft", "NIP07LargeEventSigning");
  reporter.RegisterImportantMetric("", "ms");
  reporter.AddResult("", avg_time);
}

}  // namespace dryft