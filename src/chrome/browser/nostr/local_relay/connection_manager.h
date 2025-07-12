// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_LOCAL_RELAY_CONNECTION_MANAGER_H_
#define CHROME_BROWSER_NOSTR_LOCAL_RELAY_CONNECTION_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace nostr {

// Forward declarations
struct NostrEvent;
struct NostrFilter;

namespace local_relay {

// Rate limit tracking for a connection
struct RateLimitInfo {
  // Event rate limiting
  int events_this_minute = 0;
  base::Time events_window_start;
  
  // REQ rate limiting
  int reqs_this_minute = 0;
  base::Time reqs_window_start;
  
  // Check if event is allowed
  bool CanSendEvent(int max_per_minute);
  
  // Check if REQ is allowed
  bool CanSendReq(int max_per_minute);
  
  // Update counters
  void RecordEvent();
  void RecordReq();
  
 private:
  void UpdateWindow(int& counter, base::Time& window_start);
};

// Information about a subscription
struct Subscription {
  std::string id;
  std::vector<NostrFilter> filters;
  base::Time created_at;
};

// Information about a connected client
struct ClientConnection {
  int connection_id;
  std::string remote_address;
  base::Time connected_at;
  
  // Authentication state (for future NIP-42 support)
  bool authenticated = false;
  std::string auth_pubkey;
  
  // Active subscriptions for this connection
  base::flat_map<std::string, std::unique_ptr<Subscription>> subscriptions;
  
  // Rate limiting
  RateLimitInfo rate_limit;
  
  // Statistics
  int64_t messages_sent = 0;
  int64_t messages_received = 0;
  int64_t events_published = 0;
};

// Manages WebSocket connections and subscriptions for the local relay
class ConnectionManager {
 public:
  ConnectionManager(int max_connections, int max_subscriptions_per_connection);
  ~ConnectionManager();

  // Connection lifecycle
  bool AddConnection(int connection_id, const std::string& remote_address);
  void RemoveConnection(int connection_id);
  ClientConnection* GetConnection(int connection_id);
  
  // Subscription management
  bool AddSubscription(int connection_id,
                      const std::string& subscription_id,
                      const std::vector<NostrFilter>& filters);
  bool RemoveSubscription(int connection_id, const std::string& subscription_id);
  void RemoveAllSubscriptions(int connection_id);
  
  // Find connections with subscriptions matching an event
  std::vector<int> GetMatchingConnections(const NostrEvent& event) const;
  
  // Get subscriptions for a connection that match an event
  std::vector<std::string> GetMatchingSubscriptions(
      int connection_id,
      const NostrEvent& event) const;
  
  // Rate limiting
  bool CheckEventRateLimit(int connection_id, int max_per_minute);
  bool CheckReqRateLimit(int connection_id, int max_per_minute);
  void RecordEvent(int connection_id);
  void RecordReq(int connection_id);
  
  // Statistics
  int GetConnectionCount() const { return connections_.size(); }
  int GetTotalSubscriptions() const;
  base::Value::Dict GetStatistics() const;
  base::Value::Dict GetConnectionStats(int connection_id) const;
  
  // Periodic cleanup (remove expired rate limit windows, etc)
  void PerformMaintenance();

 private:
  // Check if a filter matches an event
  bool FilterMatchesEvent(const NostrFilter& filter,
                         const NostrEvent& event) const;
  
  // Maximum limits
  const int max_connections_;
  const int max_subscriptions_per_connection_;
  
  // Active connections
  base::flat_map<int, std::unique_ptr<ClientConnection>> connections_;
  
  // Periodic maintenance timer
  base::RepeatingTimer maintenance_timer_;
  
  // Thread checker
  SEQUENCE_CHECKER(sequence_checker_);
  
  // Weak pointer factory
  base::WeakPtrFactory<ConnectionManager> weak_factory_{this};
};

}  // namespace local_relay
}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_LOCAL_RELAY_CONNECTION_MANAGER_H_