# Relay Connection Management Details
## Tungsten Browser - Advanced Relay Connection Strategies

### 1. Connection Pool Architecture

```cpp
class RelayConnectionPool {
 public:
  struct ConnectionConfig {
    int max_connections_per_relay = 3;
    int max_total_connections = 20;
    int connection_timeout_ms = 5000;
    int idle_timeout_ms = 60000;
    int max_reconnect_attempts = 5;
    base::TimeDelta reconnect_base_delay = base::Seconds(1);
    double reconnect_backoff_multiplier = 2.0;
    int max_pending_messages = 1000;
  };
  
  struct RelayInfo {
    std::string url;
    int priority = 0;  // Lower = higher priority
    double reliability_score = 1.0;  // 0.0 to 1.0
    base::Time last_successful_connection;
    int consecutive_failures = 0;
    bool supports_nip_11 = false;
    RelayCapabilities capabilities;
  };
  
  struct ConnectionStats {
    int active_connections = 0;
    int pending_connections = 0;
    int failed_connections = 0;
    base::TimeDelta average_latency;
    double success_rate = 0.0;
    int messages_sent = 0;
    int messages_received = 0;
  };
  
 private:
  struct Connection {
    enum State {
      DISCONNECTED,
      CONNECTING,
      CONNECTED,
      AUTHENTICATED,
      DISCONNECTING,
      FAILED
    };
    
    std::string relay_url;
    State state = DISCONNECTED;
    std::unique_ptr<WebSocketClient> ws_client;
    base::Time connected_at;
    base::Time last_activity;
    int pending_requests = 0;
    std::queue<PendingMessage> message_queue;
    base::circular_deque<base::TimeDelta> latency_samples;
    std::unique_ptr<base::RetainingOneShotTimer> reconnect_timer;
    int reconnect_attempt = 0;
  };
  
  std::map<std::string, std::vector<std::unique_ptr<Connection>>> connections_;
  std::map<std::string, RelayInfo> relay_info_;
  ConnectionConfig config_;
  mutable base::Lock lock_;
};
```

### 2. Connection Lifecycle Management

```cpp
class RelayConnectionManager {
 public:
  // Connection establishment with retry logic
  void ConnectToRelay(const std::string& relay_url) {
    auto connection = std::make_unique<Connection>();
    connection->relay_url = relay_url;
    connection->state = Connection::CONNECTING;
    
    // Create WebSocket with custom options
    auto ws_options = network::mojom::WebSocketOptions::New();
    ws_options->max_message_size = 10 * 1024 * 1024;  // 10MB
    ws_options->throttling_period = base::Milliseconds(100);
    
    connection->ws_client = std::make_unique<WebSocketClient>(
        relay_url,
        ws_options,
        base::BindOnce(&RelayConnectionManager::OnConnected,
                      weak_factory_.GetWeakPtr(),
                      connection.get()),
        base::BindOnce(&RelayConnectionManager::OnDisconnected,
                      weak_factory_.GetWeakPtr(),
                      connection.get()),
        base::BindRepeating(&RelayConnectionManager::OnMessage,
                          weak_factory_.GetWeakPtr(),
                          connection.get()));
    
    // Start connection timeout
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RelayConnectionManager::OnConnectionTimeout,
                      weak_factory_.GetWeakPtr(),
                      connection.get()),
        base::Milliseconds(config_.connection_timeout_ms));
    
    // Store connection
    {
      base::AutoLock lock(lock_);
      connections_[relay_url].push_back(std::move(connection));
    }
  }
  
  // Intelligent reconnection with exponential backoff
  void ScheduleReconnect(Connection* connection) {
    if (connection->reconnect_attempt >= config_.max_reconnect_attempts) {
      LOG(WARNING) << "Max reconnect attempts reached for " 
                   << connection->relay_url;
      connection->state = Connection::FAILED;
      UpdateRelayReliability(connection->relay_url, false);
      return;
    }
    
    // Calculate backoff delay
    base::TimeDelta delay = config_.reconnect_base_delay *
        std::pow(config_.reconnect_backoff_multiplier,
                connection->reconnect_attempt);
    
    // Add jitter to prevent thundering herd
    delay += base::Milliseconds(base::RandInt(0, 1000));
    
    connection->reconnect_timer = std::make_unique<base::RetainingOneShotTimer>(
        FROM_HERE,
        delay,
        base::BindOnce(&RelayConnectionManager::AttemptReconnect,
                      weak_factory_.GetWeakPtr(),
                      connection));
    connection->reconnect_timer->Start();
    
    connection->reconnect_attempt++;
  }
};
```

### 3. Load Balancing and Relay Selection

```cpp
class RelayLoadBalancer {
 public:
  // Select best relay for a request based on multiple factors
  std::string SelectRelay(const NostrFilter& filter,
                         RequestPriority priority) {
    std::vector<RelayScore> scores;
    
    {
      base::AutoLock lock(lock_);
      for (const auto& [url, info] : relay_info_) {
        if (!IsRelayHealthy(url)) continue;
        
        RelayScore score;
        score.url = url;
        score.score = CalculateRelayScore(info, filter, priority);
        scores.push_back(score);
      }
    }
    
    // Sort by score (higher is better)
    std::sort(scores.begin(), scores.end(),
             [](const RelayScore& a, const RelayScore& b) {
               return a.score > b.score;
             });
    
    // Use weighted random selection from top candidates
    if (scores.empty()) return "";
    
    int top_candidates = std::min(3, static_cast<int>(scores.size()));
    std::vector<double> weights;
    for (int i = 0; i < top_candidates; i++) {
      weights.push_back(scores[i].score);
    }
    
    std::discrete_distribution<> dist(weights.begin(), weights.end());
    int selected = dist(random_generator_);
    
    return scores[selected].url;
  }
  
 private:
  double CalculateRelayScore(const RelayInfo& info,
                            const NostrFilter& filter,
                            RequestPriority priority) {
    double score = 100.0;
    
    // Factor 1: Reliability (40% weight)
    score *= (0.4 * info.reliability_score);
    
    // Factor 2: Current load (30% weight)
    double load_factor = 1.0 - (GetRelayLoad(info.url) / 100.0);
    score *= (0.3 * load_factor);
    
    // Factor 3: Latency (20% weight)
    double latency_factor = 1.0 - (GetAverageLatency(info.url).InMilliseconds() / 1000.0);
    score *= (0.2 * std::max(0.0, latency_factor));
    
    // Factor 4: Feature support (10% weight)
    if (RequiresSpecialFeatures(filter)) {
      score *= (0.1 * (info.capabilities.supports_filters ? 1.0 : 0.5));
    }
    
    // Boost for high priority requests on less loaded relays
    if (priority == RequestPriority::HIGH) {
      score *= (2.0 - load_factor);
    }
    
    return score;
  }
  
  double GetRelayLoad(const std::string& relay_url) {
    auto connections = GetActiveConnections(relay_url);
    int total_pending = 0;
    for (const auto& conn : connections) {
      total_pending += conn->pending_requests;
    }
    return static_cast<double>(total_pending) / connections.size();
  }
};
```

### 4. Message Queuing and Prioritization

```cpp
class MessageQueue {
 public:
  enum MessagePriority {
    CRITICAL = 0,  // Auth, errors
    HIGH = 1,      // User-initiated requests
    NORMAL = 2,    // Background sync
    LOW = 3        // Preloading, analytics
  };
  
  struct QueuedMessage {
    std::string id;
    std::string message;
    MessagePriority priority;
    base::Time queued_at;
    int retry_count = 0;
    base::OnceCallback<void(bool success)> callback;
  };
  
  void EnqueueMessage(const std::string& relay_url,
                     std::unique_ptr<QueuedMessage> message) {
    base::AutoLock lock(lock_);
    
    // Check queue size limits
    auto& queue = relay_queues_[relay_url];
    if (queue.size() >= config_.max_queue_size) {
      // Evict lowest priority old messages
      EvictOldMessages(queue, 1);
    }
    
    // Insert based on priority
    auto insert_pos = std::lower_bound(
        queue.begin(), queue.end(), message->priority,
        [](const auto& msg, MessagePriority priority) {
          return msg->priority < priority;
        });
    
    queue.insert(insert_pos, std::move(message));
    
    // Wake up sender thread
    ProcessPendingMessages(relay_url);
  }
  
 private:
  void ProcessPendingMessages(const std::string& relay_url) {
    auto connection = GetHealthyConnection(relay_url);
    if (!connection || connection->state != Connection::CONNECTED) {
      return;
    }
    
    base::AutoLock lock(lock_);
    auto& queue = relay_queues_[relay_url];
    
    while (!queue.empty() && 
           connection->pending_requests < config_.max_pending_per_connection) {
      auto message = std::move(queue.front());
      queue.pop_front();
      
      // Send with timeout tracking
      SendMessageWithTimeout(connection, std::move(message));
    }
  }
  
  void SendMessageWithTimeout(Connection* connection,
                             std::unique_ptr<QueuedMessage> message) {
    auto timeout_callback = base::BindOnce(
        &MessageQueue::OnMessageTimeout,
        weak_factory_.GetWeakPtr(),
        connection->relay_url,
        message->id);
    
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        std::move(timeout_callback),
        base::Seconds(30));
    
    connection->ws_client->Send(message->message);
    connection->pending_requests++;
    
    // Track in-flight messages
    in_flight_messages_[message->id] = std::move(message);
  }
};
```

### 5. Health Monitoring and Circuit Breaking

```cpp
class RelayHealthMonitor {
 public:
  struct HealthMetrics {
    double success_rate = 1.0;
    base::TimeDelta average_latency;
    int consecutive_errors = 0;
    base::Time last_error;
    CircuitState circuit_state = CircuitState::CLOSED;
    base::Time circuit_opened_at;
  };
  
  enum CircuitState {
    CLOSED,      // Normal operation
    OPEN,        // Failing, reject requests
    HALF_OPEN    // Testing recovery
  };
  
  bool IsRelayHealthy(const std::string& relay_url) {
    base::AutoLock lock(lock_);
    auto it = health_metrics_.find(relay_url);
    if (it == health_metrics_.end()) return true;
    
    const auto& metrics = it->second;
    
    // Check circuit breaker state
    if (metrics.circuit_state == CircuitState::OPEN) {
      // Check if we should transition to half-open
      if (base::Time::Now() - metrics.circuit_opened_at > 
          config_.circuit_reset_timeout) {
        it->second.circuit_state = CircuitState::HALF_OPEN;
        return true;  // Allow one test request
      }
      return false;
    }
    
    // Check health thresholds
    if (metrics.success_rate < config_.min_success_rate ||
        metrics.consecutive_errors > config_.max_consecutive_errors ||
        metrics.average_latency > config_.max_acceptable_latency) {
      // Open circuit breaker
      it->second.circuit_state = CircuitState::OPEN;
      it->second.circuit_opened_at = base::Time::Now();
      
      LOG(WARNING) << "Circuit breaker opened for " << relay_url;
      return false;
    }
    
    return true;
  }
  
  void RecordSuccess(const std::string& relay_url,
                    base::TimeDelta latency) {
    base::AutoLock lock(lock_);
    auto& metrics = health_metrics_[relay_url];
    
    // Update success rate (exponential moving average)
    metrics.success_rate = 0.95 * metrics.success_rate + 0.05;
    
    // Update latency (exponential moving average)
    if (metrics.average_latency.is_zero()) {
      metrics.average_latency = latency;
    } else {
      metrics.average_latency = 0.8 * metrics.average_latency + 0.2 * latency;
    }
    
    // Reset error counter
    metrics.consecutive_errors = 0;
    
    // Close circuit if in half-open state
    if (metrics.circuit_state == CircuitState::HALF_OPEN) {
      metrics.circuit_state = CircuitState::CLOSED;
      LOG(INFO) << "Circuit breaker closed for " << relay_url;
    }
  }
  
  void RecordFailure(const std::string& relay_url,
                    FailureType type) {
    base::AutoLock lock(lock_);
    auto& metrics = health_metrics_[relay_url];
    
    // Update success rate
    metrics.success_rate = 0.95 * metrics.success_rate;
    
    // Increment error counter
    metrics.consecutive_errors++;
    metrics.last_error = base::Time::Now();
    
    // Open circuit if half-open
    if (metrics.circuit_state == CircuitState::HALF_OPEN) {
      metrics.circuit_state = CircuitState::OPEN;
      metrics.circuit_opened_at = base::Time::Now();
    }
  }
};
```

### 6. Relay Discovery and Auto-Configuration

```cpp
class RelayDiscovery {
 public:
  // NIP-11 relay information discovery
  void DiscoverRelayInfo(const std::string& relay_url) {
    std::string info_url = relay_url;
    if (base::StartsWith(info_url, "wss://")) {
      info_url.replace(0, 6, "https://");
    } else if (base::StartsWith(info_url, "ws://")) {
      info_url.replace(0, 5, "http://");
    }
    
    auto request = std::make_unique<network::SimpleURLLoader>(
        network::SimpleURLLoader::Create(
            CreateRelayInfoRequest(info_url),
            TRAFFIC_ANNOTATION));
    
    request->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&RelayDiscovery::OnRelayInfoReceived,
                      weak_factory_.GetWeakPtr(),
                      relay_url));
  }
  
 private:
  void OnRelayInfoReceived(const std::string& relay_url,
                          std::unique_ptr<std::string> response) {
    if (!response) return;
    
    // Parse NIP-11 relay information document
    rapidjson::Document doc;
    doc.Parse(response->c_str());
    
    if (doc.HasParseError()) return;
    
    RelayCapabilities caps;
    
    if (doc.HasMember("supported_nips") && doc["supported_nips"].IsArray()) {
      for (const auto& nip : doc["supported_nips"].GetArray()) {
        if (nip.IsInt()) {
          caps.supported_nips.insert(nip.GetInt());
        }
      }
    }
    
    if (doc.HasMember("limitation") && doc["limitation"].IsObject()) {
      const auto& limits = doc["limitation"];
      
      if (limits.HasMember("max_message_length")) {
        caps.max_message_length = limits["max_message_length"].GetInt();
      }
      
      if (limits.HasMember("max_subscriptions")) {
        caps.max_subscriptions = limits["max_subscriptions"].GetInt();
      }
      
      if (limits.HasMember("max_filters")) {
        caps.max_filters = limits["max_filters"].GetInt();
      }
    }
    
    // Update relay info
    UpdateRelayCapabilities(relay_url, caps);
  }
};
```

### 7. Connection Pooling Strategies

```cpp
class ConnectionPoolStrategy {
 public:
  // Adaptive pool sizing based on usage patterns
  void AdjustPoolSize(const std::string& relay_url) {
    auto stats = GetRelayStats(relay_url);
    auto& config = relay_configs_[relay_url];
    
    // Calculate optimal pool size
    int optimal_size = CalculateOptimalPoolSize(stats);
    int current_size = GetActiveConnections(relay_url).size();
    
    if (optimal_size > current_size) {
      // Scale up
      for (int i = current_size; i < optimal_size; i++) {
        CreateNewConnection(relay_url);
      }
    } else if (optimal_size < current_size) {
      // Scale down (gracefully close idle connections)
      auto connections = GetIdleConnections(relay_url);
      int to_close = current_size - optimal_size;
      
      for (int i = 0; i < to_close && i < connections.size(); i++) {
        connections[i]->Close(CloseReason::POOL_SCALING);
      }
    }
  }
  
 private:
  int CalculateOptimalPoolSize(const RelayStats& stats) {
    // Base size on recent usage
    double avg_concurrent_requests = stats.GetAverageConcurrentRequests();
    int base_size = std::ceil(avg_concurrent_requests / 
                             config_.requests_per_connection);
    
    // Add buffer for burst capacity
    int burst_buffer = std::ceil(stats.GetPeakConcurrentRequests() * 0.2);
    
    // Apply limits
    int optimal = base_size + burst_buffer;
    return std::clamp(optimal, 
                     config_.min_connections_per_relay,
                     config_.max_connections_per_relay);
  }
  
  // Connection affinity for subscriptions
  Connection* GetConnectionForSubscription(const std::string& relay_url,
                                         const std::string& subscription_id) {
    // Try to use same connection for related subscriptions
    auto it = subscription_affinity_.find(subscription_id);
    if (it != subscription_affinity_.end()) {
      auto conn = GetConnection(it->second);
      if (conn && conn->state == Connection::CONNECTED) {
        return conn;
      }
    }
    
    // Otherwise select least loaded connection
    return SelectLeastLoadedConnection(relay_url);
  }
};
```

### 8. Synchronization and Consistency

```cpp
class RelaySyncManager {
 public:
  // Ensure event propagation across relays
  void BroadcastEvent(const NostrEvent& event,
                     const std::vector<std::string>& target_relays) {
    auto broadcast_id = GenerateBroadcastId();
    
    BroadcastTracker tracker;
    tracker.event = event;
    tracker.target_relays = target_relays;
    tracker.start_time = base::Time::Now();
    
    {
      base::AutoLock lock(lock_);
      active_broadcasts_[broadcast_id] = tracker;
    }
    
    // Send to all target relays in parallel
    for (const auto& relay : target_relays) {
      SendEventToRelay(relay, event,
          base::BindOnce(&RelaySyncManager::OnBroadcastResult,
                        weak_factory_.GetWeakPtr(),
                        broadcast_id,
                        relay));
    }
    
    // Set timeout for broadcast completion
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RelaySyncManager::OnBroadcastTimeout,
                      weak_factory_.GetWeakPtr(),
                      broadcast_id),
        config_.broadcast_timeout);
  }
  
  // Conflict resolution for replaceable events
  void ResolveEventConflict(const NostrEvent& local_event,
                          const NostrEvent& remote_event) {
    // For replaceable events, use created_at as tie breaker
    if (IsReplaceableEvent(local_event.kind)) {
      if (remote_event.created_at > local_event.created_at) {
        // Remote event is newer, update local
        UpdateLocalEvent(remote_event);
      }
    }
    
    // For regular events, both can coexist
    // But we might want to track duplicates
    TrackDuplicateEvent(local_event.id, remote_event.id);
  }
};
```

### 9. Performance Monitoring

```cpp
class RelayPerformanceMonitor {
 public:
  struct PerformanceMetrics {
    // Latency percentiles
    base::TimeDelta p50_latency;
    base::TimeDelta p95_latency;
    base::TimeDelta p99_latency;
    
    // Throughput
    double messages_per_second;
    double bytes_per_second;
    
    // Error rates
    double connection_error_rate;
    double message_error_rate;
    
    // Resource usage
    size_t memory_usage;
    double cpu_usage_percent;
  };
  
  void RecordMetrics() {
    for (const auto& [relay_url, connections] : connection_pool_) {
      PerformanceMetrics metrics = CalculateMetrics(relay_url);
      
      // Log to UMA
      UMA_HISTOGRAM_TIMES("Nostr.Relay.Latency.P50",
                         metrics.p50_latency);
      UMA_HISTOGRAM_TIMES("Nostr.Relay.Latency.P95",
                         metrics.p95_latency);
      
      // Check for performance degradation
      if (metrics.p95_latency > config_.latency_threshold) {
        LOG(WARNING) << "High latency detected for " << relay_url
                     << ": " << metrics.p95_latency.InMilliseconds() << "ms";
        
        // Trigger mitigation
        connection_pool_->ReduceLoad(relay_url);
      }
    }
  }
  
 private:
  PerformanceMetrics CalculateMetrics(const std::string& relay_url) {
    PerformanceMetrics metrics;
    
    // Collect latency samples
    std::vector<base::TimeDelta> latencies;
    for (const auto& conn : GetConnections(relay_url)) {
      latencies.insert(latencies.end(),
                      conn->latency_samples.begin(),
                      conn->latency_samples.end());
    }
    
    // Calculate percentiles
    if (!latencies.empty()) {
      std::sort(latencies.begin(), latencies.end());
      metrics.p50_latency = latencies[latencies.size() * 0.50];
      metrics.p95_latency = latencies[latencies.size() * 0.95];
      metrics.p99_latency = latencies[latencies.size() * 0.99];
    }
    
    return metrics;
  }
};
```

### 10. Advanced Features

```cpp
// Relay clustering for high availability
class RelayCluster {
 public:
  void AddRelayToCluster(const std::string& cluster_name,
                        const std::string& relay_url) {
    clusters_[cluster_name].relays.push_back(relay_url);
  }
  
  // Send to multiple relays for redundancy
  void SendToCluster(const std::string& cluster_name,
                    const std::string& message,
                    int min_confirmations = 1) {
    auto& cluster = clusters_[cluster_name];
    int confirmations_needed = std::min(min_confirmations,
                                      static_cast<int>(cluster.relays.size()));
    
    auto confirmation_tracker = std::make_shared<ConfirmationTracker>();
    confirmation_tracker->needed = confirmations_needed;
    
    for (const auto& relay : cluster.relays) {
      SendToRelay(relay, message,
          base::BindOnce(&RelayCluster::OnRelayConfirmation,
                        weak_factory_.GetWeakPtr(),
                        confirmation_tracker));
    }
  }
};

// Geographic relay selection
class GeoRelaySelector {
 public:
  std::vector<std::string> SelectNearestRelays(int count) {
    // Get user's approximate location (privacy-preserving)
    auto user_region = GetUserRegion();
    
    // Sort relays by geographic distance
    std::vector<RelayDistance> distances;
    for (const auto& [url, info] : relay_registry_) {
      if (info.geographic_region.empty()) continue;
      
      double distance = CalculateRegionDistance(user_region,
                                               info.geographic_region);
      distances.push_back({url, distance});
    }
    
    std::sort(distances.begin(), distances.end(),
             [](const auto& a, const auto& b) {
               return a.distance < b.distance;
             });
    
    // Return nearest relays
    std::vector<std::string> result;
    for (int i = 0; i < count && i < distances.size(); i++) {
      result.push_back(distances[i].relay_url);
    }
    
    return result;
  }
};
```