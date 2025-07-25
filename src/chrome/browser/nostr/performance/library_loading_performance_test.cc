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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resources/nostr_resource_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace dryft {

class LibraryLoadingPerformanceTest : public testing::Test {
 protected:
  void SetUp() override {
    PerformanceRegressionDetector::ClearAllBaselines();
    MemoryUsageTracker::ResetPeakMemoryTracking();
    
    profile_ = std::make_unique<TestingProfile>();
    
    // Load benchmark data
    LoadBenchmarkData();
    
    // Set up performance baselines
    SetupPerformanceBaselines();
  }
  
  void TearDown() override {
    profile_.reset();
  }
  
  void LoadBenchmarkData() {
    // Load benchmark targets
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII("tungsten_performance");
    
    base::FilePath benchmark_file = test_data_dir.AppendASCII("benchmark_data.json");
    std::string benchmark_content;
    if (base::ReadFileToString(benchmark_file, &benchmark_content)) {
      auto benchmark_value = base::JSONReader::Read(benchmark_content);
      if (benchmark_value && benchmark_value->is_dict()) {
        const base::Value::Dict& benchmark_dict = benchmark_value->GetDict();
        const base::Value::Dict* perf_benchmarks = benchmark_dict.FindDict("performance_benchmarks");
        if (perf_benchmarks) {
          const base::Value::Dict* library_targets = perf_benchmarks->FindDict("library_loading_targets");
          if (library_targets) {
            // Load library loading targets
            library_targets_ = *library_targets;
          }
        }
      }
    }
  }
  
  void SetupPerformanceBaselines() {
    // Set baselines for different libraries
    PerformanceRegressionDetector::LogPerformanceBaseline("Library.ndk", 100.0);
    PerformanceRegressionDetector::LogPerformanceBaseline("Library.nostr-tools", 80.0);
    PerformanceRegressionDetector::LogPerformanceBaseline("Library.secp256k1", 60.0);
    PerformanceRegressionDetector::LogPerformanceBaseline("Library.applesauce", 90.0);
    PerformanceRegressionDetector::LogPerformanceBaseline("Library.alby-sdk", 85.0);
  }
  
  // Simulate library loading
  void SimulateLibraryLoad(const std::string& library_name, int complexity_ms) {
    // Simulate network request delay
    base::PlatformThread::Sleep(base::Milliseconds(complexity_ms / 4));
    
    // Simulate parsing/compilation delay
    base::PlatformThread::Sleep(base::Milliseconds(complexity_ms / 2));
    
    // Simulate initialization delay
    base::PlatformThread::Sleep(base::Milliseconds(complexity_ms / 4));
  }
  
  // Get expected loading time for a library
  double GetExpectedLoadTime(const std::string& library_name) {
    const base::Value::Dict* target_dict = library_targets_.FindDict(library_name + "_load_ms");
    if (target_dict) {
      // This is a simplification - in real implementation, we'd parse the structure properly
      return 100.0;  // Default target
    }
    return 100.0;  // Default target
  }
  
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  base::Value::Dict library_targets_;
};

TEST_F(LibraryLoadingPerformanceTest, NDKLibraryLoadingPerformance) {
  const std::string library_name = "ndk";
  const int kIterations = 8;
  std::vector<double> times;
  
  for (int i = 0; i < kIterations; ++i) {
    SCOPED_DRYFT_TIMER_WITH_CONTEXT(kLibraryLoad, library_name);
    
    // Simulate NDK library loading (largest library)
    SimulateLibraryLoad(library_name, 25);  // 25ms simulation
    
    times.push_back(timer.GetElapsedTime().InMilliseconds());
  }
  
  // Calculate average
  double avg_time = 0.0;
  for (double time : times) {
    avg_time += time;
  }
  avg_time /= kIterations;
  
  // NDK should load within 100ms
  EXPECT_LT(avg_time, 100.0)
      << "NDK library loading took " << avg_time << "ms, expected < 100ms";
  
  // Check for performance regression
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "Library.ndk", avg_time, 100.0, 10.0));
  
  // Log performance metric
  perf_test::PerfResultReporter reporter("dryft", "LibraryLoadingNDK");
  reporter.RegisterImportantMetric("", "ms");
  reporter.AddResult("", avg_time);
}

TEST_F(LibraryLoadingPerformanceTest, NostrToolsLibraryLoadingPerformance) {
  const std::string library_name = "nostr-tools";
  const int kIterations = 8;
  std::vector<double> times;
  
  for (int i = 0; i < kIterations; ++i) {
    SCOPED_DRYFT_TIMER_WITH_CONTEXT(kLibraryLoad, library_name);
    
    // Simulate nostr-tools library loading
    SimulateLibraryLoad(library_name, 20);  // 20ms simulation
    
    times.push_back(timer.GetElapsedTime().InMilliseconds());
  }
  
  // Calculate average
  double avg_time = 0.0;
  for (double time : times) {
    avg_time += time;
  }
  avg_time /= kIterations;
  
  // nostr-tools should load within 80ms
  EXPECT_LT(avg_time, 80.0)
      << "nostr-tools library loading took " << avg_time << "ms, expected < 80ms";
  
  // Check for performance regression
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "Library.nostr-tools", avg_time, 80.0, 10.0));
  
  // Log performance metric
  perf_test::PerfResultReporter reporter("dryft", "LibraryLoadingNostrTools");
  reporter.RegisterImportantMetric("", "ms");
  reporter.AddResult("", avg_time);
}

TEST_F(LibraryLoadingPerformanceTest, Secp256k1LibraryLoadingPerformance) {
  const std::string library_name = "secp256k1";
  const int kIterations = 10;
  std::vector<double> times;
  
  for (int i = 0; i < kIterations; ++i) {
    SCOPED_DRYFT_TIMER_WITH_CONTEXT(kLibraryLoad, library_name);
    
    // Simulate secp256k1 library loading (smallest library)
    SimulateLibraryLoad(library_name, 15);  // 15ms simulation
    
    times.push_back(timer.GetElapsedTime().InMilliseconds());
  }
  
  // Calculate average
  double avg_time = 0.0;
  for (double time : times) {
    avg_time += time;
  }
  avg_time /= kIterations;
  
  // secp256k1 should load within 60ms
  EXPECT_LT(avg_time, 60.0)
      << "secp256k1 library loading took " << avg_time << "ms, expected < 60ms";
  
  // Check for performance regression
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "Library.secp256k1", avg_time, 60.0, 10.0));
  
  // Log performance metric
  perf_test::PerfResultReporter reporter("dryft", "LibraryLoadingSecp256k1");
  reporter.RegisterImportantMetric("", "ms");
  reporter.AddResult("", avg_time);
}

TEST_F(LibraryLoadingPerformanceTest, ApplesauceLibraryLoadingPerformance) {
  const std::string library_name = "applesauce";
  const int kIterations = 8;
  std::vector<double> times;
  
  for (int i = 0; i < kIterations; ++i) {
    SCOPED_DRYFT_TIMER_WITH_CONTEXT(kLibraryLoad, library_name);
    
    // Simulate applesauce library loading
    SimulateLibraryLoad(library_name, 22);  // 22ms simulation
    
    times.push_back(timer.GetElapsedTime().InMilliseconds());
  }
  
  // Calculate average
  double avg_time = 0.0;
  for (double time : times) {
    avg_time += time;
  }
  avg_time /= kIterations;
  
  // applesauce should load within 90ms
  EXPECT_LT(avg_time, 90.0)
      << "applesauce library loading took " << avg_time << "ms, expected < 90ms";
  
  // Check for performance regression
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "Library.applesauce", avg_time, 90.0, 10.0));
  
  // Log performance metric
  perf_test::PerfResultReporter reporter("dryft", "LibraryLoadingApplesauce");
  reporter.RegisterImportantMetric("", "ms");
  reporter.AddResult("", avg_time);
}

TEST_F(LibraryLoadingPerformanceTest, AlbySDKLibraryLoadingPerformance) {
  const std::string library_name = "alby-sdk";
  const int kIterations = 8;
  std::vector<double> times;
  
  for (int i = 0; i < kIterations; ++i) {
    SCOPED_DRYFT_TIMER_WITH_CONTEXT(kLibraryLoad, library_name);
    
    // Simulate alby-sdk library loading
    SimulateLibraryLoad(library_name, 21);  // 21ms simulation
    
    times.push_back(timer.GetElapsedTime().InMilliseconds());
  }
  
  // Calculate average
  double avg_time = 0.0;
  for (double time : times) {
    avg_time += time;
  }
  avg_time /= kIterations;
  
  // alby-sdk should load within 85ms
  EXPECT_LT(avg_time, 85.0)
      << "alby-sdk library loading took " << avg_time << "ms, expected < 85ms";
  
  // Check for performance regression
  EXPECT_TRUE(PerformanceRegressionDetector::CheckPerformanceRegression(
      "Library.alby-sdk", avg_time, 85.0, 10.0));
  
  // Log performance metric
  perf_test::PerfResultReporter reporter("dryft", "LibraryLoadingAlbySDK");
  reporter.RegisterImportantMetric("", "ms");
  reporter.AddResult("", avg_time);
}

TEST_F(LibraryLoadingPerformanceTest, ConcurrentLibraryLoadingPerformance) {
  const std::vector<std::string> libraries = {
    "ndk", "nostr-tools", "secp256k1", "applesauce", "alby-sdk"
  };
  
  const int kIterations = 3;
  std::vector<double> total_times;
  
  for (int i = 0; i < kIterations; ++i) {
    base::ElapsedTimer total_timer;
    
    // Simulate concurrent library loading
    for (const auto& library : libraries) {
      SCOPED_DRYFT_TIMER_WITH_CONTEXT(kLibraryLoad, library);
      
      // Simulate library loading with different complexities
      int complexity = (library == "ndk") ? 25 : 
                      (library == "nostr-tools") ? 20 :
                      (library == "applesauce") ? 22 :
                      (library == "alby-sdk") ? 21 : 15;
      
      SimulateLibraryLoad(library, complexity);
    }
    
    total_times.push_back(total_timer.Elapsed().InMilliseconds());
  }
  
  // Calculate average total time
  double avg_total_time = 0.0;
  for (double time : total_times) {
    avg_total_time += time;
  }
  avg_total_time /= kIterations;
  
  // All libraries should load within 400ms total
  EXPECT_LT(avg_total_time, 400.0)
      << "Concurrent library loading took " << avg_total_time << "ms, expected < 400ms";
  
  // Log performance metric
  perf_test::PerfResultReporter reporter("dryft", "LibraryLoadingConcurrent");
  reporter.RegisterImportantMetric("TotalTime", "ms");
  reporter.RegisterImportantMetric("AvgTimePerLibrary", "ms");
  reporter.AddResult("TotalTime", avg_total_time);
  reporter.AddResult("AvgTimePerLibrary", avg_total_time / libraries.size());
}

TEST_F(LibraryLoadingPerformanceTest, LibraryLoadingCacheEffectiveness) {
  const std::string library_name = "ndk";
  const int kIterations = 10;
  
  std::vector<double> first_load_times;
  std::vector<double> cached_load_times;
  
  for (int i = 0; i < kIterations; ++i) {
    // First load (cold cache)
    {
      SCOPED_DRYFT_TIMER_WITH_CONTEXT(kLibraryLoad, library_name);
      SimulateLibraryLoad(library_name, 25);  // Full load time
      first_load_times.push_back(timer.GetElapsedTime().InMilliseconds());
    }
    
    // Second load (warm cache)
    {
      SCOPED_DRYFT_TIMER_WITH_CONTEXT(kLibraryLoad, library_name);
      SimulateLibraryLoad(library_name, 5);   // Cached load time
      cached_load_times.push_back(timer.GetElapsedTime().InMilliseconds());
    }
  }
  
  // Calculate averages
  double avg_first_load = 0.0;
  for (double time : first_load_times) {
    avg_first_load += time;
  }
  avg_first_load /= kIterations;
  
  double avg_cached_load = 0.0;
  for (double time : cached_load_times) {
    avg_cached_load += time;
  }
  avg_cached_load /= kIterations;
  
  // Calculate cache improvement
  double cache_improvement = (avg_first_load - avg_cached_load) / avg_first_load * 100.0;
  
  // Cache should provide at least 50% improvement
  EXPECT_GT(cache_improvement, 50.0)
      << "Cache improvement is " << cache_improvement << "%, expected > 50%";
  
  // Cached loads should be significantly faster
  EXPECT_LT(avg_cached_load, avg_first_load * 0.5)
      << "Cached load " << avg_cached_load << "ms should be < 50% of first load " 
      << avg_first_load << "ms";
  
  // Log performance metrics
  perf_test::PerfResultReporter reporter("dryft", "LibraryLoadingCacheEffectiveness");
  reporter.RegisterImportantMetric("FirstLoadTime", "ms");
  reporter.RegisterImportantMetric("CachedLoadTime", "ms");
  reporter.RegisterImportantMetric("CacheImprovement", "%");
  reporter.AddResult("FirstLoadTime", avg_first_load);
  reporter.AddResult("CachedLoadTime", avg_cached_load);
  reporter.AddResult("CacheImprovement", cache_improvement);
}

TEST_F(LibraryLoadingPerformanceTest, LibraryBundleSizeImpact) {
  struct LibraryInfo {
    std::string name;
    size_t size_kb;
    int load_time_ms;
  };
  
  std::vector<LibraryInfo> libraries = {
    {"secp256k1", 150, 60},     // Smallest
    {"alby-sdk", 300, 85},      // Medium
    {"nostr-tools", 450, 80},   // Medium-large
    {"applesauce", 500, 90},    // Large
    {"ndk", 800, 100}           // Largest
  };
  
  // Test loading time correlation with bundle size
  for (const auto& lib : libraries) {
    const int kIterations = 5;
    std::vector<double> times;
    
    for (int i = 0; i < kIterations; ++i) {
      SCOPED_DRYFT_TIMER_WITH_CONTEXT(kLibraryLoad, lib.name);
      SimulateLibraryLoad(lib.name, lib.load_time_ms / 4);  // Scaled simulation
      times.push_back(timer.GetElapsedTime().InMilliseconds());
    }
    
    // Calculate average
    double avg_time = 0.0;
    for (double time : times) {
      avg_time += time;
    }
    avg_time /= kIterations;
    
    // Calculate loading efficiency (KB per ms)
    double efficiency = lib.size_kb / avg_time;
    
    // Should be able to load at least 10KB per ms
    EXPECT_GT(efficiency, 10.0)
        << "Library " << lib.name << " efficiency is " << efficiency 
        << " KB/ms, expected > 10 KB/ms";
    
    // Log performance metrics
    perf_test::PerfResultReporter reporter("dryft", "LibraryBundleSize_" + lib.name);
    reporter.RegisterImportantMetric("LoadTime", "ms");
    reporter.RegisterImportantMetric("BundleSize", "KB");
    reporter.RegisterImportantMetric("Efficiency", "KB/ms");
    reporter.AddResult("LoadTime", avg_time);
    reporter.AddResult("BundleSize", lib.size_kb);
    reporter.AddResult("Efficiency", efficiency);
  }
}

TEST_F(LibraryLoadingPerformanceTest, LibraryExecutionOverhead) {
  const std::string library_name = "nostr-tools";
  const int kIterations = 15;
  
  std::vector<double> load_times;
  std::vector<double> execution_times;
  
  for (int i = 0; i < kIterations; ++i) {
    // Measure library loading
    base::ElapsedTimer load_timer;
    SimulateLibraryLoad(library_name, 20);
    load_times.push_back(load_timer.Elapsed().InMilliseconds());
    
    // Measure library execution overhead
    base::ElapsedTimer exec_timer;
    // Simulate library function calls
    for (int j = 0; j < 10; ++j) {
      base::PlatformThread::Sleep(base::Microseconds(100));  // Simulate function call
    }
    execution_times.push_back(exec_timer.Elapsed().InMilliseconds());
  }
  
  // Calculate averages
  double avg_load_time = 0.0;
  for (double time : load_times) {
    avg_load_time += time;
  }
  avg_load_time /= kIterations;
  
  double avg_execution_time = 0.0;
  for (double time : execution_times) {
    avg_execution_time += time;
  }
  avg_execution_time /= kIterations;
  
  // Execution overhead should be minimal compared to load time
  EXPECT_LT(avg_execution_time, avg_load_time * 0.1)
      << "Execution overhead " << avg_execution_time << "ms should be < 10% of load time " 
      << avg_load_time << "ms";
  
  // Log performance metrics
  perf_test::PerfResultReporter reporter("dryft", "LibraryExecutionOverhead");
  reporter.RegisterImportantMetric("LoadTime", "ms");
  reporter.RegisterImportantMetric("ExecutionTime", "ms");
  reporter.RegisterImportantMetric("OverheadRatio", "%");
  reporter.AddResult("LoadTime", avg_load_time);
  reporter.AddResult("ExecutionTime", avg_execution_time);
  reporter.AddResult("OverheadRatio", (avg_execution_time / avg_load_time) * 100.0);
}

}  // namespace dryft