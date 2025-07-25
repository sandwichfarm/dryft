// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/local_relay/connection_manager.h"

#include <algorithm>
#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/nostr/nostr_event.h"
#include "components/nostr/nostr_filter.h"

namespace nostr {
namespace local_relay {

namespace {

// Time window for rate limiting (1 minute)
constexpr base::TimeDelta kRateLimitWindow = base::Minutes(1);

// Maintenance interval (clean up old rate limit windows)
constexpr base::TimeDelta kMaintenanceInterval = base::Minutes(5);

}  // namespace

// RateLimitInfo implementation

bool RateLimitInfo::CanSendEvent(int max_per_minute) {
  UpdateWindow(events_this_minute, events_window_start);
  return events_this_minute < max_per_minute;
}

bool RateLimitInfo::CanSendReq(int max_per_minute) {
  UpdateWindow(reqs_this_minute, reqs_window_start);
  return reqs_this_minute < max_per_minute;
}

void RateLimitInfo::RecordEvent() {
  UpdateWindow(events_this_minute, events_window_start);
  events_this_minute++;
}

void RateLimitInfo::RecordReq() {
  UpdateWindow(reqs_this_minute, reqs_window_start);
  reqs_this_minute++;
}

void RateLimitInfo::UpdateWindow(int& counter, base::Time& window_start) {
  base::Time now = base::Time::Now();
  if (now - window_start > kRateLimitWindow) {
    counter = 0;
    window_start = now;
  }
}

// ConnectionManager implementation

ConnectionManager::ConnectionManager(int max_connections,
                                   int max_subscriptions_per_connection)
    : max_connections_(max_connections),
      max_subscriptions_per_connection_(max_subscriptions_per_connection) {
  // Start maintenance timer
  maintenance_timer_.Start(FROM_HERE, kMaintenanceInterval,
                          this, &ConnectionManager::PerformMaintenance);
}

ConnectionManager::~ConnectionManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  maintenance_timer_.Stop();
}

bool ConnectionManager::AddConnection(int connection_id,
                                    const std::string& remote_address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (connections_.size() >= static_cast<size_t>(max_connections_)) {
    return false;
  }
  
  auto connection = std::make_unique<ClientConnection>();
  connection->connection_id = connection_id;
  connection->remote_address = remote_address;
  connection->connected_at = base::Time::Now();
  
  connections_[connection_id] = std::move(connection);
  
  VLOG(1) << "Added connection " << connection_id << " from " << remote_address;
  return true;
}

void ConnectionManager::RemoveConnection(int connection_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  auto it = connections_.find(connection_id);
  if (it != connections_.end()) {
    VLOG(1) << "Removing connection " << connection_id;
    connections_.erase(it);
  }
}

ClientConnection* ConnectionManager::GetConnection(int connection_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  auto it = connections_.find(connection_id);
  if (it != connections_.end()) {
    return it->second.get();
  }
  return nullptr;
}

bool ConnectionManager::AddSubscription(
    int connection_id,
    const std::string& subscription_id,
    const std::vector<NostrFilter>& filters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  auto* connection = GetConnection(connection_id);
  if (!connection) {
    return false;
  }
  
  if (connection->subscriptions.size() >= 
      static_cast<size_t>(max_subscriptions_per_connection_)) {
    return false;
  }
  
  auto subscription = std::make_unique<Subscription>();
  subscription->id = subscription_id;
  subscription->filters = filters;
  subscription->created_at = base::Time::Now();
  
  connection->subscriptions[subscription_id] = std::move(subscription);
  
  VLOG(2) << "Added subscription " << subscription_id 
          << " for connection " << connection_id;
  return true;
}

bool ConnectionManager::RemoveSubscription(int connection_id,
                                         const std::string& subscription_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  auto* connection = GetConnection(connection_id);
  if (!connection) {
    return false;
  }
  
  auto it = connection->subscriptions.find(subscription_id);
  if (it != connection->subscriptions.end()) {
    connection->subscriptions.erase(it);
    VLOG(2) << "Removed subscription " << subscription_id 
            << " for connection " << connection_id;
    return true;
  }
  
  return false;
}

void ConnectionManager::RemoveAllSubscriptions(int connection_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  auto* connection = GetConnection(connection_id);
  if (connection) {
    connection->subscriptions.clear();
    VLOG(2) << "Removed all subscriptions for connection " << connection_id;
  }
}

std::vector<int> ConnectionManager::GetMatchingConnections(
    const NostrEvent& event) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  std::vector<int> matching_connections;
  
  for (const auto& [conn_id, connection] : connections_) {
    for (const auto& [sub_id, subscription] : connection->subscriptions) {
      for (const auto& filter : subscription->filters) {
        if (FilterMatchesEvent(filter, event)) {
          matching_connections.push_back(conn_id);
          break;  // Found a match, no need to check other filters
        }
      }
      if (std::find(matching_connections.begin(), matching_connections.end(), 
                    conn_id) != matching_connections.end()) {
        break;  // Already added this connection
      }
    }
  }
  
  return matching_connections;
}

std::vector<std::string> ConnectionManager::GetMatchingSubscriptions(
    int connection_id,
    const NostrEvent& event) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  std::vector<std::string> matching_subscriptions;
  
  auto it = connections_.find(connection_id);
  if (it == connections_.end()) {
    return matching_subscriptions;
  }
  
  for (const auto& [sub_id, subscription] : it->second->subscriptions) {
    for (const auto& filter : subscription->filters) {
      if (FilterMatchesEvent(filter, event)) {
        matching_subscriptions.push_back(sub_id);
        break;  // Found a match, no need to check other filters
      }
    }
  }
  
  return matching_subscriptions;
}

bool ConnectionManager::CheckEventRateLimit(int connection_id,
                                          int max_per_minute) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  auto* connection = GetConnection(connection_id);
  if (!connection) {
    return false;
  }
  
  return connection->rate_limit.CanSendEvent(max_per_minute);
}

bool ConnectionManager::CheckReqRateLimit(int connection_id,
                                        int max_per_minute) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  auto* connection = GetConnection(connection_id);
  if (!connection) {
    return false;
  }
  
  return connection->rate_limit.CanSendReq(max_per_minute);
}

void ConnectionManager::RecordEvent(int connection_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  auto* connection = GetConnection(connection_id);
  if (connection) {
    connection->rate_limit.RecordEvent();
    connection->events_published++;
  }
}

void ConnectionManager::RecordReq(int connection_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  auto* connection = GetConnection(connection_id);
  if (connection) {
    connection->rate_limit.RecordReq();
  }
}

int ConnectionManager::GetTotalSubscriptions() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  int total = 0;
  for (const auto& [conn_id, connection] : connections_) {
    total += connection->subscriptions.size();
  }
  return total;
}

base::Value::Dict ConnectionManager::GetStatistics() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  base::Value::Dict stats;
  stats.Set("connection_count", static_cast<int>(connections_.size()));
  stats.Set("total_subscriptions", GetTotalSubscriptions());
  
  base::Value::List connection_list;
  for (const auto& [conn_id, connection] : connections_) {
    connection_list.Append(GetConnectionStats(conn_id));
  }
  stats.Set("connections", std::move(connection_list));
  
  return stats;
}

base::Value::Dict ConnectionManager::GetConnectionStats(int connection_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  base::Value::Dict stats;
  
  auto it = connections_.find(connection_id);
  if (it == connections_.end()) {
    return stats;
  }
  
  const auto& connection = it->second;
  stats.Set("connection_id", connection_id);
  stats.Set("remote_address", connection->remote_address);
  stats.Set("connected_at", connection->connected_at.ToJsTime());
  stats.Set("authenticated", connection->authenticated);
  stats.Set("subscription_count", 
            static_cast<int>(connection->subscriptions.size()));
  stats.Set("messages_sent", static_cast<double>(connection->messages_sent));
  stats.Set("messages_received", 
            static_cast<double>(connection->messages_received));
  stats.Set("events_published", 
            static_cast<double>(connection->events_published));
  
  return stats;
}

void ConnectionManager::PerformMaintenance() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  // Currently, rate limit windows are updated on demand
  // This method is here for future maintenance tasks like:
  // - Cleaning up stale connections
  // - Enforcing connection timeouts
  // - Collecting metrics
  
  VLOG(3) << "Performing connection manager maintenance";
}

bool ConnectionManager::FilterMatchesEvent(const NostrFilter& filter,
                                         const NostrEvent& event) const {
  // Check event IDs
  if (!filter.ids.empty()) {
    if (std::find(filter.ids.begin(), filter.ids.end(), event.id) == 
        filter.ids.end()) {
      return false;
    }
  }
  
  // Check authors
  if (!filter.authors.empty()) {
    if (std::find(filter.authors.begin(), filter.authors.end(), 
                  event.pubkey) == filter.authors.end()) {
      return false;
    }
  }
  
  // Check kinds
  if (!filter.kinds.empty()) {
    if (std::find(filter.kinds.begin(), filter.kinds.end(), 
                  event.kind) == filter.kinds.end()) {
      return false;
    }
  }
  
  // Check time constraints
  if (filter.since.has_value() && event.created_at < filter.since.value()) {
    return false;
  }
  
  if (filter.until.has_value() && event.created_at > filter.until.value()) {
    return false;
  }
  
  // Check tags
  for (const auto& [tag_name, tag_values] : filter.tags) {
    bool found_matching_tag = false;
    
    // Look for this tag in the event
    for (const auto& tag : event.tags) {
      const auto* tag_list = tag.GetIfList();
      if (!tag_list || tag_list->empty()) {
        continue;
      }
      
      const auto* name = (*tag_list)[0].GetIfString();
      if (!name || *name != tag_name) {
        continue;
      }
      
      // Check if any tag value matches
      for (size_t i = 1; i < tag_list->size(); ++i) {
        const auto* value = (*tag_list)[i].GetIfString();
        if (value && std::find(tag_values.begin(), tag_values.end(), 
                              *value) != tag_values.end()) {
          found_matching_tag = true;
          break;
        }
      }
      
      if (found_matching_tag) {
        break;
      }
    }
    
    if (!found_matching_tag) {
      return false;
    }
  }
  
  return true;
}

}  // namespace local_relay
}  // namespace nostr