# Low Level Design Document: Local Relay
## Tungsten Browser - Built-in Nostr Relay Implementation

### 1. Component Overview

The Local Relay component provides a fully-functional Nostr relay running within the browser process. It serves as a local cache, reduces latency, provides offline functionality, and enhances privacy by allowing selective synchronization with external relays.

### 2. Detailed Architecture

```
┌─────────────────────────────────────────────────────────┐
│                 WebSocket Server Layer                   │
│  ┌─────────────────────────────────────────────────┐   │
│  │          Local WebSocket Server                   │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ Connection   │  │ Message Router    │       │   │
│  │  │ Manager      │  │                   │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                  NIP-01 Protocol Layer                   │
│  ┌─────────────────────────────────────────────────┐   │
│  │             Protocol Handler                      │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ REQ Handler  │  │ EVENT Handler     │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ CLOSE Handler│  │ AUTH Handler      │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                   Storage Layer                          │
│  ┌─────────────────────────────────────────────────┐   │
│  │              Event Database                       │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ IndexedDB    │  │ Query Engine      │       │   │
│  │  │ Storage      │  │                   │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                Synchronization Layer                     │
│  ┌─────────────────────────────────────────────────┐   │
│  │            External Relay Sync                    │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ Sync Manager │  │ Conflict         │       │   │
│  │  │              │  │ Resolution       │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│              Policy & Filter Layer                       │
│  ┌─────────────────────────────────────────────────┐   │
│  │           Policy Engine                           │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ Rate Limiter │  │ Content Filter    │       │   │
│  │  │              │  │                   │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### 3. WebSocket Server Implementation

```cpp
// services/local_relay_service.cc
class LocalRelayService : public service_manager::Service {
 public:
  LocalRelayService();
  
  void Start(uint16_t port = 8081);
  void Stop();
  
 private:
  class WebSocketServer : public net::WebSocketEventInterface {
   public:
    void OnConnect(std::unique_ptr<net::WebSocketChannel> channel) override;
    void OnDataFrame(bool fin, 
                    net::WebSocketMessageType type,
                    scoped_refptr<net::IOBuffer> buffer,
                    size_t size) override;
    void OnClosingHandshake() override;
    void OnDropChannel(bool was_clean,
                      uint16_t code,
                      const std::string& reason) override;
  };
  
  std::unique_ptr<net::HttpServer> http_server_;
  std::unique_ptr<WebSocketServer> ws_server_;
  std::unique_ptr<ProtocolHandler> protocol_handler_;
  std::unique_ptr<EventDatabase> event_db_;
};

// Connection management
class ConnectionManager {
 public:
  struct Connection {
    std::string id;
    std::unique_ptr<net::WebSocketChannel> channel;
    std::map<std::string, SubscriptionFilter> subscriptions;
    base::Time connected_at;
    bool authenticated;
    std::string pubkey;  // If authenticated
  };
  
  void AddConnection(std::unique_ptr<net::WebSocketChannel> channel);
  void RemoveConnection(const std::string& connection_id);
  Connection* GetConnection(const std::string& connection_id);
  void BroadcastEvent(const NostrEvent& event);
  
 private:
  std::map<std::string, std::unique_ptr<Connection>> connections_;
  base::Lock connections_lock_;
};
```

### 4. NIP-01 Protocol Implementation

```cpp
// Protocol message handlers
class ProtocolHandler {
 public:
  ProtocolHandler(EventDatabase* db, ConnectionManager* conn_mgr);
  
  void HandleMessage(const std::string& connection_id,
                    const std::string& message);
  
 private:
  // Message type handlers
  void HandleEvent(const std::string& connection_id,
                  const rapidjson::Value& message);
  void HandleReq(const std::string& connection_id,
                const rapidjson::Value& message);
  void HandleClose(const std::string& connection_id,
                  const rapidjson::Value& message);
  void HandleAuth(const std::string& connection_id,
                 const rapidjson::Value& message);
  
  // Helper methods
  bool ValidateEvent(const NostrEvent& event);
  bool CheckRateLimit(const std::string& connection_id);
  void SendNotice(const std::string& connection_id,
                 const std::string& message);
  void SendEvent(const std::string& connection_id,
                const std::string& subscription_id,
                const NostrEvent& event);
  void SendEOSE(const std::string& connection_id,
               const std::string& subscription_id);
  void SendOK(const std::string& connection_id,
             const std::string& event_id,
             bool success,
             const std::string& message);
  
  EventDatabase* event_db_;
  ConnectionManager* connection_manager_;
  RateLimiter rate_limiter_;
};

// EVENT message handler
void ProtocolHandler::HandleEvent(const std::string& connection_id,
                                const rapidjson::Value& message) {
  if (!message.IsArray() || message.Size() < 2) {
    SendNotice(connection_id, "Invalid EVENT message");
    return;
  }
  
  // Parse event
  NostrEvent event;
  if (!ParseEvent(message[1], &event)) {
    SendNotice(connection_id, "Invalid event format");
    return;
  }
  
  // Validate event
  if (!ValidateEvent(event)) {
    SendOK(connection_id, event.id, false, "Invalid event signature");
    return;
  }
  
  // Check rate limits
  if (!CheckRateLimit(connection_id)) {
    SendOK(connection_id, event.id, false, "Rate limit exceeded");
    return;
  }
  
  // Store event
  bool stored = event_db_->StoreEvent(event);
  if (stored) {
    // Broadcast to matching subscriptions
    connection_manager_->BroadcastEvent(event);
    
    // Sync to external relays if configured
    sync_manager_->QueueForSync(event);
  }
  
  SendOK(connection_id, event.id, stored, 
         stored ? "" : "Duplicate or rejected");
}

// REQ message handler
void ProtocolHandler::HandleReq(const std::string& connection_id,
                               const rapidjson::Value& message) {
  if (!message.IsArray() || message.Size() < 3) {
    SendNotice(connection_id, "Invalid REQ message");
    return;
  }
  
  // Extract subscription ID
  if (!message[1].IsString()) {
    SendNotice(connection_id, "Invalid subscription ID");
    return;
  }
  std::string sub_id = message[1].GetString();
  
  // Parse filters
  std::vector<NostrFilter> filters;
  for (size_t i = 2; i < message.Size(); ++i) {
    NostrFilter filter;
    if (!ParseFilter(message[i], &filter)) {
      SendNotice(connection_id, "Invalid filter");
      return;
    }
    filters.push_back(filter);
  }
  
  // Store subscription
  auto* connection = connection_manager_->GetConnection(connection_id);
  connection->subscriptions[sub_id] = filters;
  
  // Query existing events
  auto events = event_db_->QueryEvents(filters);
  
  // Send matching events
  for (const auto& event : events) {
    SendEvent(connection_id, sub_id, event);
  }
  
  // Send EOSE
  SendEOSE(connection_id, sub_id);
}
```

### 5. Event Database Implementation

```cpp
// Event storage using IndexedDB
class EventDatabase {
 public:
  EventDatabase();
  
  bool StoreEvent(const NostrEvent& event);
  std::vector<NostrEvent> QueryEvents(
      const std::vector<NostrFilter>& filters);
  void DeleteEvent(const std::string& event_id);
  void DeleteExpiredEvents();
  
 private:
  // Database schema
  struct EventRecord {
    std::string id;
    std::string pubkey;
    int64_t created_at;
    int kind;
    std::string tags_json;  // Serialized tags for indexing
    std::string content;
    std::string sig;
    
    // Derived fields for indexing
    std::vector<std::string> referenced_events;
    std::vector<std::string> referenced_pubkeys;
  };
  
  // Query builder
  class QueryBuilder {
   public:
    QueryBuilder& WithIds(const std::vector<std::string>& ids);
    QueryBuilder& WithAuthors(const std::vector<std::string>& authors);
    QueryBuilder& WithKinds(const std::vector<int>& kinds);
    QueryBuilder& WithTags(const std::map<std::string, 
                          std::vector<std::string>>& tags);
    QueryBuilder& Since(int64_t timestamp);
    QueryBuilder& Until(int64_t timestamp);
    QueryBuilder& Limit(int limit);
    
    std::string BuildSQL() const;
    
   private:
    std::vector<std::string> conditions_;
    std::map<std::string, std::string> parameters_;
    int limit_ = 0;
  };
  
  std::unique_ptr<sql::Database> db_;
  base::FilePath db_path_;
};

// Efficient query implementation
std::vector<NostrEvent> EventDatabase::QueryEvents(
    const std::vector<NostrFilter>& filters) {
  std::set<std::string> result_ids;
  std::vector<NostrEvent> results;
  
  for (const auto& filter : filters) {
    QueryBuilder builder;
    
    // Build query conditions
    if (!filter.ids.empty()) {
      builder.WithIds(filter.ids);
    }
    if (!filter.authors.empty()) {
      builder.WithAuthors(filter.authors);
    }
    if (!filter.kinds.empty()) {
      builder.WithKinds(filter.kinds);
    }
    if (!filter.tags.empty()) {
      builder.WithTags(filter.tags);
    }
    if (filter.since > 0) {
      builder.Since(filter.since);
    }
    if (filter.until > 0) {
      builder.Until(filter.until);
    }
    if (filter.limit > 0) {
      builder.Limit(filter.limit);
    }
    
    // Execute query
    sql::Statement statement(db_->GetUniqueStatement(
        builder.BuildSQL().c_str()));
    
    while (statement.Step()) {
      std::string event_id = statement.ColumnString(0);
      
      // Avoid duplicates across filters
      if (result_ids.find(event_id) != result_ids.end()) {
        continue;
      }
      result_ids.insert(event_id);
      
      // Reconstruct event
      NostrEvent event;
      event.id = event_id;
      event.pubkey = statement.ColumnString(1);
      event.created_at = statement.ColumnInt64(2);
      event.kind = statement.ColumnInt(3);
      
      // Parse tags
      std::string tags_json = statement.ColumnString(4);
      rapidjson::Document doc;
      doc.Parse(tags_json.c_str());
      for (const auto& tag : doc.GetArray()) {
        std::vector<std::string> tag_values;
        for (const auto& value : tag.GetArray()) {
          tag_values.push_back(value.GetString());
        }
        event.tags.push_back(tag_values);
      }
      
      event.content = statement.ColumnString(5);
      event.sig = statement.ColumnString(6);
      
      results.push_back(event);
    }
  }
  
  // Sort by created_at descending
  std::sort(results.begin(), results.end(),
           [](const NostrEvent& a, const NostrEvent& b) {
             return a.created_at > b.created_at;
           });
  
  return results;
}
```

### 6. External Relay Synchronization

```cpp
class RelaySyncManager {
 public:
  struct SyncConfig {
    std::vector<std::string> relay_urls;
    bool sync_outgoing = true;  // Send local events to relays
    bool sync_incoming = true;  // Fetch events from relays
    std::vector<NostrFilter> incoming_filters;
    int sync_interval_seconds = 300;  // 5 minutes
  };
  
  void Configure(const SyncConfig& config);
  void QueueForSync(const NostrEvent& event);
  void StartSync();
  void StopSync();
  
 private:
  void SyncWithRelay(const std::string& relay_url);
  void SendPendingEvents(RelayConnection* connection);
  void FetchNewEvents(RelayConnection* connection);
  void HandleConflict(const NostrEvent& local_event,
                     const NostrEvent& remote_event);
  
  SyncConfig config_;
  std::queue<NostrEvent> outgoing_queue_;
  base::RepeatingTimer sync_timer_;
  std::map<std::string, std::unique_ptr<RelayConnection>> connections_;
};

// Sync implementation
void RelaySyncManager::SyncWithRelay(const std::string& relay_url) {
  auto& connection = connections_[relay_url];
  if (!connection) {
    connection = std::make_unique<RelayConnection>(relay_url);
  }
  
  if (!connection->IsConnected()) {
    connection->Connect(
        base::BindOnce(&RelaySyncManager::OnRelayConnected,
                      weak_factory_.GetWeakPtr(),
                      relay_url));
    return;
  }
  
  // Send pending events
  if (config_.sync_outgoing) {
    SendPendingEvents(connection.get());
  }
  
  // Fetch new events
  if (config_.sync_incoming) {
    FetchNewEvents(connection.get());
  }
}

void RelaySyncManager::SendPendingEvents(RelayConnection* connection) {
  while (!outgoing_queue_.empty()) {
    const NostrEvent& event = outgoing_queue_.front();
    
    connection->SendEvent(
        event,
        base::BindOnce(&RelaySyncManager::OnEventSent,
                      weak_factory_.GetWeakPtr(),
                      event.id));
    
    outgoing_queue_.pop();
  }
}

void RelaySyncManager::FetchNewEvents(RelayConnection* connection) {
  // Get last sync timestamp
  int64_t last_sync = GetLastSyncTime(connection->url());
  
  // Build filters with since timestamp
  std::vector<NostrFilter> filters = config_.incoming_filters;
  for (auto& filter : filters) {
    filter.since = last_sync;
  }
  
  // Subscribe to new events
  connection->Subscribe(
      "sync",
      filters,
      base::BindRepeating(&RelaySyncManager::OnEventReceived,
                         weak_factory_.GetWeakPtr()));
}
```

### 7. Rate Limiting and Policy Engine

```cpp
class RateLimiter {
 public:
  struct RateLimitConfig {
    int events_per_minute = 10;
    int subscriptions_per_connection = 10;
    int max_filters_per_subscription = 10;
    int max_event_size = 64 * 1024;  // 64KB
    int max_subscription_results = 1000;
  };
  
  bool CheckEventRate(const std::string& pubkey);
  bool CheckSubscriptionLimit(const std::string& connection_id,
                             int current_subs);
  bool CheckEventSize(size_t size);
  
 private:
  struct RateLimitEntry {
    std::deque<base::Time> timestamps;
    int subscription_count = 0;
  };
  
  std::map<std::string, RateLimitEntry> rate_limits_;
  RateLimitConfig config_;
  base::Lock lock_;
};

// Content filtering
class ContentFilter {
 public:
  struct FilterRule {
    enum Type { BLOCK_PUBKEY, BLOCK_CONTENT, REQUIRE_POW };
    Type type;
    std::string pattern;
    int pow_difficulty = 0;
  };
  
  void AddRule(const FilterRule& rule);
  bool ShouldAcceptEvent(const NostrEvent& event);
  
 private:
  bool CheckProofOfWork(const NostrEvent& event, int difficulty);
  bool ContainsBlockedContent(const std::string& content);
  
  std::vector<FilterRule> rules_;
  std::set<std::string> blocked_pubkeys_;
  std::vector<std::regex> blocked_patterns_;
};
```

### 8. WebSocket Message Protocol

```cpp
// Message serialization/deserialization
class MessageSerializer {
 public:
  // Outgoing messages
  static std::string SerializeEvent(const std::string& subscription_id,
                                  const NostrEvent& event) {
    rapidjson::Document doc;
    doc.SetArray();
    doc.PushBack("EVENT", doc.GetAllocator());
    doc.PushBack(rapidjson::Value(subscription_id.c_str(), 
                                 doc.GetAllocator()), 
                doc.GetAllocator());
    
    rapidjson::Value event_obj(rapidjson::kObjectType);
    event_obj.AddMember("id", rapidjson::Value(event.id.c_str(), 
                                              doc.GetAllocator()),
                       doc.GetAllocator());
    event_obj.AddMember("pubkey", rapidjson::Value(event.pubkey.c_str(),
                                                  doc.GetAllocator()),
                       doc.GetAllocator());
    event_obj.AddMember("created_at", event.created_at, 
                       doc.GetAllocator());
    event_obj.AddMember("kind", event.kind, doc.GetAllocator());
    
    // Add tags
    rapidjson::Value tags(rapidjson::kArrayType);
    for (const auto& tag : event.tags) {
      rapidjson::Value tag_array(rapidjson::kArrayType);
      for (const auto& value : tag) {
        tag_array.PushBack(rapidjson::Value(value.c_str(), 
                                          doc.GetAllocator()),
                          doc.GetAllocator());
      }
      tags.PushBack(tag_array, doc.GetAllocator());
    }
    event_obj.AddMember("tags", tags, doc.GetAllocator());
    
    event_obj.AddMember("content", rapidjson::Value(event.content.c_str(),
                                                   doc.GetAllocator()),
                       doc.GetAllocator());
    event_obj.AddMember("sig", rapidjson::Value(event.sig.c_str(),
                                              doc.GetAllocator()),
                       doc.GetAllocator());
    
    doc.PushBack(event_obj, doc.GetAllocator());
    
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    
    return std::string(buffer.GetString());
  }
  
  static std::string SerializeEOSE(const std::string& subscription_id) {
    return base::StringPrintf("[\"EOSE\",\"%s\"]", 
                            subscription_id.c_str());
  }
  
  static std::string SerializeOK(const std::string& event_id,
                               bool success,
                               const std::string& message) {
    return base::StringPrintf("[\"OK\",\"%s\",%s,\"%s\"]",
                            event_id.c_str(),
                            success ? "true" : "false",
                            message.c_str());
  }
  
  static std::string SerializeNotice(const std::string& message) {
    return base::StringPrintf("[\"NOTICE\",\"%s\"]", message.c_str());
  }
};
```

### 9. Storage Management

```cpp
class StorageManager {
 public:
  struct StorageConfig {
    size_t max_db_size = 1024 * 1024 * 1024;  // 1GB
    int retention_days = 30;
    bool auto_cleanup = true;
    int cleanup_interval_hours = 24;
  };
  
  void Configure(const StorageConfig& config);
  void EnforceStorageLimits();
  size_t GetDatabaseSize();
  void PruneOldEvents();
  
 private:
  void DeleteEventsOlderThan(base::Time cutoff);
  void DeleteLargeEvents(size_t target_size);
  void CompactDatabase();
  
  StorageConfig config_;
  EventDatabase* event_db_;
  base::RepeatingTimer cleanup_timer_;
};

// Cleanup implementation
void StorageManager::EnforceStorageLimits() {
  size_t current_size = GetDatabaseSize();
  
  if (current_size > config_.max_db_size) {
    // Delete old events first
    base::Time cutoff = base::Time::Now() - 
        base::TimeDelta::FromDays(config_.retention_days);
    DeleteEventsOlderThan(cutoff);
    
    // If still over limit, delete large events
    current_size = GetDatabaseSize();
    if (current_size > config_.max_db_size) {
      DeleteLargeEvents(config_.max_db_size * 0.9);
    }
    
    // Compact database
    CompactDatabase();
  }
}
```

### 10. Performance Optimizations

```cpp
// Connection pooling for external relays
class RelayConnectionPool {
 public:
  std::shared_ptr<RelayConnection> GetConnection(
      const std::string& relay_url) {
    base::AutoLock lock(pool_lock_);
    
    auto it = connections_.find(relay_url);
    if (it != connections_.end() && it->second->IsHealthy()) {
      return it->second;
    }
    
    // Create new connection
    auto connection = std::make_shared<RelayConnection>(relay_url);
    connections_[relay_url] = connection;
    
    // Start health monitoring
    StartHealthCheck(connection);
    
    return connection;
  }
  
 private:
  void StartHealthCheck(std::shared_ptr<RelayConnection> connection) {
    health_check_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(30),
        base::BindRepeating(&RelayConnectionPool::CheckHealth,
                          weak_factory_.GetWeakPtr(),
                          connection));
  }
  
  std::map<std::string, std::shared_ptr<RelayConnection>> connections_;
  base::Lock pool_lock_;
  base::RepeatingTimer health_check_timer_;
};

// Event batching for broadcast
class EventBroadcaster {
 public:
  void QueueForBroadcast(const NostrEvent& event,
                        const std::set<std::string>& connection_ids) {
    base::AutoLock lock(queue_lock_);
    
    for (const auto& conn_id : connection_ids) {
      pending_broadcasts_[conn_id].push_back(event);
    }
    
    // Schedule batch send
    if (!batch_timer_.IsRunning()) {
      batch_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(10),
          base::BindOnce(&EventBroadcaster::FlushBatches,
                        weak_factory_.GetWeakPtr()));
    }
  }
  
 private:
  void FlushBatches() {
    base::AutoLock lock(queue_lock_);
    
    for (auto& [conn_id, events] : pending_broadcasts_) {
      SendBatch(conn_id, events);
    }
    
    pending_broadcasts_.clear();
  }
  
  std::map<std::string, std::vector<NostrEvent>> pending_broadcasts_;
  base::Lock queue_lock_;
  base::OneShotTimer batch_timer_;
};
```

### 11. Testing Strategy

```cpp
// Unit tests
TEST(LocalRelayTest, StoresAndRetrievesEvents) {
  EventDatabase db;
  
  NostrEvent event;
  event.id = "test123";
  event.pubkey = "pubkey123";
  event.created_at = base::Time::Now().ToTimeT();
  event.kind = 1;
  event.content = "Test message";
  event.sig = "validsig";
  
  EXPECT_TRUE(db.StoreEvent(event));
  
  NostrFilter filter;
  filter.ids.push_back(event.id);
  
  auto results = db.QueryEvents({filter});
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].id, event.id);
}

TEST(ProtocolHandlerTest, ValidatesEventSignatures) {
  ProtocolHandler handler(nullptr, nullptr);
  
  NostrEvent event;
  event.id = "invalid";
  event.sig = "invalidsig";
  
  EXPECT_FALSE(handler.ValidateEvent(event));
}

// Integration tests
TEST(LocalRelayIntegrationTest, HandlesMultipleConnections) {
  LocalRelayService relay;
  relay.Start(8081);
  
  // Create multiple WebSocket connections
  auto client1 = CreateWebSocketClient("ws://localhost:8081");
  auto client2 = CreateWebSocketClient("ws://localhost:8081");
  
  // Subscribe on client1
  client1->Send("[\"REQ\",\"sub1\",{\"kinds\":[1]}]");
  
  // Send event from client2
  client2->Send("[\"EVENT\",{...}]");
  
  // Verify client1 receives the event
  std::string message = client1->ReceiveMessage();
  EXPECT_TRUE(message.find("EVENT") != std::string::npos);
}
```

### 12. Configuration and Settings

```cpp
class LocalRelaySettings {
 public:
  struct Settings {
    bool enabled = true;
    uint16_t port = 8081;
    std::string bind_address = "127.0.0.1";
    
    // Storage settings
    size_t max_db_size = 1024 * 1024 * 1024;  // 1GB
    int retention_days = 30;
    
    // Rate limiting
    int events_per_minute = 10;
    int max_subscription_results = 1000;
    
    // Sync settings
    bool sync_enabled = false;
    std::vector<std::string> sync_relays;
    
    // Content filtering
    bool enable_pow_filter = false;
    int min_pow_difficulty = 0;
  };
  
  static Settings Load();
  static void Save(const Settings& settings);
  
 private:
  static base::FilePath GetSettingsPath();
};
```

### 13. Monitoring and Metrics

```cpp
class RelayMetrics {
 public:
  void RecordEvent(const std::string& type);
  void RecordLatency(const std::string& operation, 
                    base::TimeDelta latency);
  void RecordConnectionCount(int count);
  
  struct Stats {
    int total_events_stored;
    int total_subscriptions;
    int active_connections;
    double average_query_latency_ms;
    size_t database_size_bytes;
  };
  
  Stats GetStats() const;
  
 private:
  std::atomic<int> event_count_{0};
  std::atomic<int> subscription_count_{0};
  std::atomic<int> connection_count_{0};
  base::MovingAverage query_latency_;
};
```