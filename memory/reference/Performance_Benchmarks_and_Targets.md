# Performance Benchmarks and Targets
## dryft browser - Detailed Performance Specifications

### 1. Performance Target Overview

```cpp
namespace performance {

struct PerformanceTargets {
  // Browser startup impact
  struct StartupTargets {
    base::TimeDelta cold_start_overhead = base::Milliseconds(50);
    base::TimeDelta warm_start_overhead = base::Milliseconds(20);
    base::TimeDelta first_paint_delay = base::Milliseconds(0);  // No visible delay
    size_t startup_memory_overhead = 10 * 1024 * 1024;  // 10MB
  };
  
  // Runtime performance
  struct RuntimeTargets {
    double cpu_usage_idle = 0.1;  // 0.1% when idle
    double cpu_usage_active = 5.0;  // 5% during active relay communication
    size_t memory_overhead_base = 50 * 1024 * 1024;  // 50MB base
    size_t memory_per_relay = 5 * 1024 * 1024;  // 5MB per active relay
    size_t memory_per_thousand_events = 2 * 1024 * 1024;  // 2MB per 1k events
  };
  
  // Operation latencies
  struct LatencyTargets {
    base::TimeDelta nip07_get_pubkey = base::Milliseconds(5);
    base::TimeDelta nip07_sign_event = base::Milliseconds(20);
    base::TimeDelta relay_connect = base::Seconds(1);
    base::TimeDelta event_broadcast = base::Milliseconds(100);
    base::TimeDelta event_query_local = base::Milliseconds(10);
    base::TimeDelta event_query_relay = base::Milliseconds(500);
    base::TimeDelta blossom_cache_hit = base::Milliseconds(5);
    base::TimeDelta blossom_cache_miss = base::Seconds(2);
  };
  
  // Throughput targets
  struct ThroughputTargets {
    int events_per_second_receive = 1000;
    int events_per_second_process = 500;
    int events_per_second_store = 200;
    int subscriptions_per_relay = 100;
    int concurrent_relay_connections = 20;
    size_t max_message_size = 10 * 1024 * 1024;  // 10MB
  };
};

}  // namespace performance
```

### 2. Startup Performance Optimization

```cpp
class StartupPerformanceOptimizer {
 public:
  struct StartupMetrics {
    base::TimeDelta time_to_main;
    base::TimeDelta time_to_first_paint;
    base::TimeDelta time_to_interactive;
    base::TimeDelta nostr_init_time;
    size_t memory_at_startup;
    int threads_created;
  };
  
  void OptimizeStartup() {
    // Lazy initialization of Nostr components
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&StartupPerformanceOptimizer::InitializeNostrServices,
                      weak_factory_.GetWeakPtr()));
  }
  
 private:
  void InitializeNostrServices() {
    TRACE_EVENT0("nostr", "InitializeNostrServices");
    
    // Phase 1: Critical components only (< 10ms)
    {
      TRACE_EVENT0("nostr", "Phase1_Critical");
      
      // Initialize key service (required for NIP-07)
      key_service_ = std::make_unique<KeyService>();
      key_service_->InitializeMinimal();
      
      // Register protocol handler
      protocol_handler_ = std::make_unique<NostrProtocolHandler>();
    }
    
    // Phase 2: Defer non-critical initialization (background)
    base::ThreadPool::PostDelayedTask(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&StartupPerformanceOptimizer::InitializePhase2,
                      weak_factory_.GetWeakPtr()),
        base::Seconds(1));
  }
  
  void InitializePhase2() {
    TRACE_EVENT0("nostr", "Phase2_Deferred");
    
    // Local relay (can be started later)
    if (prefs_->GetBoolean(prefs::kNostrLocalRelayEnabled)) {
      local_relay_ = std::make_unique<LocalRelayService>();
      local_relay_->StartLazy();
    }
    
    // Blossom server (on-demand)
    if (prefs_->GetBoolean(prefs::kBlossomServerEnabled)) {
      blossom_server_ = std::make_unique<BlossomService>();
      blossom_server_->StartLazy();
    }
  }
};

// Startup benchmark test
void MeasureStartupPerformance() {
  base::ElapsedTimer timer;
  
  // Cold start
  {
    ProcessManager process_manager;
    auto browser = process_manager.LaunchBrowser({"--enable-nostr"});
    
    auto cold_start_time = timer.Elapsed();
    EXPECT_LT(cold_start_time, base::Milliseconds(50));
    
    browser.Terminate();
  }
  
  // Warm start
  timer.Reset();
  {
    ProcessManager process_manager;
    auto browser = process_manager.LaunchBrowser({"--enable-nostr"});
    
    auto warm_start_time = timer.Elapsed();
    EXPECT_LT(warm_start_time, base::Milliseconds(20));
  }
}
```

### 3. Memory Usage Profiling

```cpp
class MemoryProfiler {
 public:
  struct MemorySnapshot {
    size_t total_allocated;
    size_t nostr_service_memory;
    size_t key_storage_memory;
    size_t relay_connections_memory;
    size_t event_database_memory;
    size_t blossom_cache_memory;
    size_t javascript_heap_memory;
  };
  
  MemorySnapshot CaptureSnapshot() {
    MemorySnapshot snapshot;
    
    // Get process memory info
    base::ProcessMetrics* metrics = base::ProcessMetrics::CreateCurrentProcessMetrics();
    snapshot.total_allocated = metrics->GetWorkingSetSize();
    
    // Nostr service breakdown
    snapshot.nostr_service_memory = CalculateNostrServiceMemory();
    snapshot.key_storage_memory = key_service_->GetMemoryUsage();
    snapshot.relay_connections_memory = relay_manager_->GetMemoryUsage();
    snapshot.event_database_memory = event_db_->GetMemoryUsage();
    snapshot.blossom_cache_memory = blossom_cache_->GetMemoryUsage();
    
    // V8 heap for window.nostr
    v8::HeapStatistics heap_stats;
    v8::Isolate::GetCurrent()->GetHeapStatistics(&heap_stats);
    snapshot.javascript_heap_memory = heap_stats.used_heap_size();
    
    return snapshot;
  }
  
  void RunMemoryBenchmark() {
    // Baseline
    auto baseline = CaptureSnapshot();
    
    // Test: 10 relay connections
    for (int i = 0; i < 10; i++) {
      relay_manager_->ConnectToRelay(
          base::StringPrintf("wss://relay%d.test", i));
    }
    WaitForConnections();
    
    auto with_relays = CaptureSnapshot();
    size_t relay_overhead = with_relays.relay_connections_memory - 
                           baseline.relay_connections_memory;
    
    EXPECT_LT(relay_overhead / 10, 5 * 1024 * 1024);  // < 5MB per relay
    
    // Test: Store 10,000 events
    std::vector<NostrEvent> events = GenerateTestEvents(10000);
    for (const auto& event : events) {
      event_db_->StoreEvent(event);
    }
    
    auto with_events = CaptureSnapshot();
    size_t event_overhead = with_events.event_database_memory - 
                           with_relays.event_database_memory;
    
    EXPECT_LT(event_overhead / 10, 2 * 1024 * 1024);  // < 2MB per 1k events
  }
};
```

### 4. CPU Usage Optimization

```cpp
class CPUUsageOptimizer {
 public:
  struct CPUMetrics {
    double idle_usage_percent;
    double active_usage_percent;
    double peak_usage_percent;
    std::map<std::string, double> component_usage;
  };
  
  void OptimizeCPUUsage() {
    // Use efficient JSON parsing
    ConfigureRapidJSON();
    
    // Batch database operations
    event_db_->EnableBatching(true);
    event_db_->SetBatchSize(100);
    event_db_->SetBatchInterval(base::Milliseconds(100));
    
    // Throttle non-critical operations
    sync_manager_->SetThrottleRate(10);  // 10 ops/second
    
    // Use worker threads for crypto operations
    key_service_->SetThreadPool(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::USER_VISIBLE,
             base::MayBlock()}));
  }
  
  void MeasureCPUUsage() {
    CPUMonitor monitor;
    
    // Idle measurement (5 seconds)
    monitor.Start();
    base::PlatformThread::Sleep(base::Seconds(5));
    auto idle_usage = monitor.GetAverageUsage();
    EXPECT_LT(idle_usage, 0.1);  // < 0.1% when idle
    
    // Active measurement - simulate heavy load
    monitor.Reset();
    
    // Connect to multiple relays
    for (int i = 0; i < 10; i++) {
      relay_manager_->ConnectToRelay(GetTestRelay(i));
    }
    
    // Generate and process events
    for (int i = 0; i < 1000; i++) {
      auto event = GenerateTestEvent();
      relay_manager_->BroadcastEvent(event);
      
      if (i % 10 == 0) {
        base::RunLoop().RunUntilIdle();
      }
    }
    
    auto active_usage = monitor.GetAverageUsage();
    EXPECT_LT(active_usage, 5.0);  // < 5% during active use
  }
};
```

### 5. Network Performance

```cpp
class NetworkPerformanceBenchmark {
 public:
  struct NetworkMetrics {
    base::TimeDelta relay_connection_time;
    base::TimeDelta ssl_handshake_time;
    base::TimeDelta first_message_time;
    int messages_per_second;
    size_t bytes_per_second;
    double packet_loss_rate;
  };
  
  void BenchmarkRelayConnection() {
    std::vector<base::TimeDelta> connection_times;
    
    for (int i = 0; i < 100; i++) {
      base::ElapsedTimer timer;
      
      auto connection = std::make_unique<RelayConnection>(GetTestRelay());
      base::RunLoop run_loop;
      
      connection->Connect(
          base::BindOnce([](base::RunLoop* loop, bool success) {
            EXPECT_TRUE(success);
            loop->Quit();
          }, &run_loop));
      
      run_loop.Run();
      
      connection_times.push_back(timer.Elapsed());
    }
    
    // Calculate percentiles
    std::sort(connection_times.begin(), connection_times.end());
    
    auto p50 = connection_times[50];
    auto p95 = connection_times[95];
    auto p99 = connection_times[99];
    
    EXPECT_LT(p50, base::Milliseconds(500));
    EXPECT_LT(p95, base::Seconds(1));
    EXPECT_LT(p99, base::Seconds(2));
  }
  
  void BenchmarkEventThroughput() {
    // Setup test relay
    auto relay = ConnectToTestRelay();
    
    // Measure receive throughput
    int events_received = 0;
    base::ElapsedTimer timer;
    
    relay->Subscribe("perf-test", CreateHighVolumeFilter(),
        base::BindRepeating([&events_received](const NostrEvent& event) {
          events_received++;
        }));
    
    // Let it run for 10 seconds
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        run_loop.QuitClosure(),
        base::Seconds(10));
    run_loop.Run();
    
    double events_per_second = events_received / timer.Elapsed().InSecondsF();
    EXPECT_GT(events_per_second, 1000);  // > 1000 events/second
  }
};
```

### 6. Database Performance

```cpp
class DatabasePerformanceBenchmark {
 public:
  void BenchmarkEventStorage() {
    EventDatabase db(":memory:");  // In-memory for testing
    
    // Insert performance
    std::vector<NostrEvent> events = GenerateTestEvents(10000);
    
    base::ElapsedTimer timer;
    for (const auto& event : events) {
      db.StoreEvent(event);
    }
    auto insert_time = timer.Elapsed();
    
    double inserts_per_second = 10000.0 / insert_time.InSecondsF();
    EXPECT_GT(inserts_per_second, 200);  // > 200 inserts/second
    
    // Query performance - by author
    timer.Reset();
    auto results = db.QueryByAuthor(events[0].pubkey);
    auto query_time = timer.Elapsed();
    
    EXPECT_LT(query_time, base::Milliseconds(10));  // < 10ms for indexed query
    
    // Complex query - multiple filters
    NostrFilter complex_filter;
    complex_filter.authors = {events[0].pubkey, events[1].pubkey};
    complex_filter.kinds = {1, 3, 7};
    complex_filter.since = base::Time::Now().ToTimeT() - 3600;
    
    timer.Reset();
    results = db.QueryEvents({complex_filter});
    auto complex_query_time = timer.Elapsed();
    
    EXPECT_LT(complex_query_time, base::Milliseconds(50));
  }
  
  void BenchmarkIndexPerformance() {
    // Test index effectiveness
    EventDatabase db(":memory:");
    
    // Insert 100k events
    for (int i = 0; i < 100000; i++) {
      db.StoreEvent(GenerateTestEvent());
    }
    
    // Measure query performance with and without indexes
    db.DropIndexes();
    
    base::ElapsedTimer timer;
    auto results = db.QueryByKind(1);
    auto without_index = timer.Elapsed();
    
    db.CreateIndexes();
    
    timer.Reset();
    results = db.QueryByKind(1);
    auto with_index = timer.Elapsed();
    
    // Indexes should provide >10x speedup
    EXPECT_LT(with_index.InMillisecondsF() * 10, 
              without_index.InMillisecondsF());
  }
};
```

### 7. JavaScript Performance

```cpp
class JavaScriptPerformanceBenchmark {
 public:
  void BenchmarkNIP07Operations() {
    content::WebContents* web_contents = CreateTestWebContents();
    
    // Benchmark getPublicKey
    std::string script = R"(
      async function benchmarkGetPublicKey() {
        const iterations = 1000;
        const start = performance.now();
        
        for (let i = 0; i < iterations; i++) {
          await window.nostr.getPublicKey();
        }
        
        const end = performance.now();
        return (end - start) / iterations;
      }
      benchmarkGetPublicKey();
    )";
    
    double avg_time_ms = content::EvalJs(web_contents, script).ExtractDouble();
    EXPECT_LT(avg_time_ms, 5.0);  // < 5ms per call
    
    // Benchmark signEvent
    script = R"(
      async function benchmarkSignEvent() {
        const event = {
          kind: 1,
          created_at: Math.floor(Date.now() / 1000),
          tags: [],
          content: "Benchmark test"
        };
        
        const iterations = 100;
        const start = performance.now();
        
        for (let i = 0; i < iterations; i++) {
          await window.nostr.signEvent(event);
        }
        
        const end = performance.now();
        return (end - start) / iterations;
      }
      benchmarkSignEvent();
    )";
    
    avg_time_ms = content::EvalJs(web_contents, script).ExtractDouble();
    EXPECT_LT(avg_time_ms, 20.0);  // < 20ms per signature
  }
};
```

### 8. Blossom Cache Performance

```cpp
class BlossomCachePerformance {
 public:
  void BenchmarkCacheOperations() {
    BlossomCache cache(base::FilePath("/tmp/blossom_bench"));
    cache.SetMaxSize(100 * 1024 * 1024);  // 100MB
    
    // Generate test blobs
    std::vector<BlobData> blobs;
    for (int i = 0; i < 1000; i++) {
      blobs.push_back(GenerateRandomBlob(
          base::RandInt(1024, 1024 * 1024)));  // 1KB to 1MB
    }
    
    // Benchmark insertions
    base::ElapsedTimer timer;
    for (const auto& blob : blobs) {
      cache.Put(blob.hash, blob.data);
    }
    auto insert_time = timer.Elapsed();
    
    LOG(INFO) << "Cache insertion rate: " 
              << blobs.size() / insert_time.InSecondsF() << " blobs/sec";
    
    // Benchmark cache hits
    timer.Reset();
    int hits = 0;
    for (int i = 0; i < 10000; i++) {
      auto hash = blobs[base::RandInt(0, blobs.size() - 1)].hash;
      if (cache.Get(hash)) {
        hits++;
      }
    }
    auto lookup_time = timer.Elapsed();
    
    double lookups_per_second = 10000.0 / lookup_time.InSecondsF();
    EXPECT_GT(lookups_per_second, 100000);  // > 100k lookups/second
    
    // Verify cache hit rate
    double hit_rate = static_cast<double>(hits) / 10000.0;
    EXPECT_GT(hit_rate, 0.95);  // > 95% hit rate
  }
};
```

### 9. End-to-End Performance Scenarios

```cpp
class EndToEndPerformance {
 public:
  void BenchmarkTypicalUserSession() {
    // Simulate 1-hour user session
    PerformanceRecorder recorder;
    recorder.Start();
    
    // Connect to relays
    ConnectToDefaultRelays();
    
    // Subscribe to typical filters
    SubscribeToFollows(100);  // Following 100 users
    
    // Simulate user activity
    for (int minute = 0; minute < 60; minute++) {
      // Post a note every 5 minutes
      if (minute % 5 == 0) {
        PostNote("Test note " + base::NumberToString(minute));
      }
      
      // Scroll through feed (trigger event loading)
      LoadMoreEvents(20);
      
      // Random interactions
      if (base::RandInt(0, 10) < 3) {
        ReactToRandomEvent();
      }
      
      // Simulate idle time
      base::PlatformThread::Sleep(base::Seconds(base::RandInt(30, 90)));
    }
    
    auto metrics = recorder.Stop();
    
    // Verify performance targets
    EXPECT_LT(metrics.average_cpu_usage, 2.0);  // < 2% average CPU
    EXPECT_LT(metrics.peak_memory_usage, 200 * 1024 * 1024);  // < 200MB
    EXPECT_LT(metrics.total_network_bytes, 50 * 1024 * 1024);  // < 50MB
  }
};
```

### 10. Performance Monitoring Dashboard

```cpp
class PerformanceMonitoringDashboard {
 public:
  void InitializeMonitoring() {
    // Real-time metrics collection
    metric_collector_ = std::make_unique<MetricCollector>();
    
    // Register metrics
    metric_collector_->RegisterMetric(
        "nostr.relay.latency",
        base::BindRepeating(&GetRelayLatency));
    
    metric_collector_->RegisterMetric(
        "nostr.cpu.usage",
        base::BindRepeating(&GetCPUUsage));
    
    metric_collector_->RegisterMetric(
        "nostr.memory.usage",
        base::BindRepeating(&GetMemoryUsage));
    
    // Start collection
    metric_collector_->Start(base::Seconds(1));  // 1Hz sampling
  }
  
  // Export metrics for analysis
  void ExportMetrics(const base::FilePath& output_path) {
    auto metrics = metric_collector_->GetAllMetrics();
    
    // Create performance report
    rapidjson::Document report;
    report.SetObject();
    
    // Summary statistics
    report.AddMember("summary", CreateSummary(metrics), 
                    report.GetAllocator());
    
    // Detailed time series
    report.AddMember("timeseries", CreateTimeSeries(metrics),
                    report.GetAllocator());
    
    // Performance violations
    report.AddMember("violations", FindViolations(metrics),
                    report.GetAllocator());
    
    // Write to file
    WriteJSONToFile(report, output_path);
  }
};
```

### 11. Performance Regression Testing

```cpp
class PerformanceRegressionTest : public testing::Test {
 protected:
  void RunRegressionTest(const std::string& test_name) {
    // Load baseline metrics
    auto baseline = LoadBaseline(test_name);
    
    // Run performance test
    auto current = RunPerformanceTest(test_name);
    
    // Compare against baseline
    EXPECT_LT(current.startup_time, 
              baseline.startup_time * 1.1);  // Allow 10% regression
    
    EXPECT_LT(current.memory_usage,
              baseline.memory_usage * 1.15);  // Allow 15% regression
    
    EXPECT_LT(current.cpu_usage,
              baseline.cpu_usage * 1.2);  // Allow 20% regression
    
    // Update baseline if improvement
    if (current.IsBetterThan(baseline)) {
      UpdateBaseline(test_name, current);
    }
  }
};

// Automated performance tests
TEST_F(PerformanceRegressionTest, StartupPerformance) {
  RunRegressionTest("startup");
}

TEST_F(PerformanceRegressionTest, RelayConnectionPerformance) {
  RunRegressionTest("relay_connection");
}

TEST_F(PerformanceRegressionTest, EventProcessingPerformance) {
  RunRegressionTest("event_processing");
}
```

### 12. Performance Optimization Guidelines

```cpp
// Performance best practices enforced by static analysis
namespace performance_guidelines {

// Avoid blocking the main thread
[[clang::annotate("performance-critical")]]
void ProcessNostrEvent(const NostrEvent& event) {
  // This will trigger a warning if called on main thread
  DCHECK(!base::CurrentUIThread::IsSet());
  
  // Heavy processing here
}

// Use efficient data structures
template<typename T>
using EventCache = base::flat_map<std::string, T>;  // O(log n) but cache-friendly

// Batch operations
class BatchProcessor {
 public:
  void AddEvent(NostrEvent event) {
    batch_.push_back(std::move(event));
    
    if (batch_.size() >= kBatchSize || batch_timer_->IsRunning()) {
      ProcessBatch();
    } else if (!batch_timer_->IsRunning()) {
      batch_timer_->Start(FROM_HERE, kBatchInterval, this,
                         &BatchProcessor::ProcessBatch);
    }
  }
  
 private:
  static constexpr size_t kBatchSize = 100;
  static constexpr base::TimeDelta kBatchInterval = base::Milliseconds(100);
};

}  // namespace performance_guidelines
```