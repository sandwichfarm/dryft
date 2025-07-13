// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/relay_client/relay_connection.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/nostr/nostr_event.h"
#include "components/nostr/nostr_filter.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/websocket.mojom.h"

namespace nostr {

RelayConnection::RelayConnection(const GURL& relay_url)
    : relay_url_(relay_url) {
  DCHECK(relay_url_.is_valid());
  DCHECK(relay_url_.SchemeIsWSOrWSS());
}

RelayConnection::~RelayConnection() {
  Disconnect();
}

void RelayConnection::Connect(ConnectionCallback callback) {
  if (state_ == RelayConnectionState::CONNECTED) {
    std::move(callback).Run(true, "");
    return;
  }
  
  if (state_ == RelayConnectionState::CONNECTING) {
    // Already connecting, queue callback
    // TODO: Support multiple connection callbacks
    DLOG(WARNING) << "Already connecting to relay: " << relay_url_;
    std::move(callback).Run(false, "Already connecting");
    return;
  }
  
  connection_callback_ = std::move(callback);
  SetState(RelayConnectionState::CONNECTING);
  
  // Start connection timeout
  connection_timer_.Start(
      FROM_HERE, connection_timeout_,
      base::BindOnce(&RelayConnection::OnConnectionTimeout,
                     weak_factory_.GetWeakPtr()));
  
  // TODO: Create WebSocket connection using Chromium's network service
  // This is a placeholder - actual implementation would create WebSocket
  LOG(INFO) << "Connecting to relay: " << relay_url_;
  
  // Simulate successful connection for now
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RelayConnection::OnWebSocketConnected,
                     weak_factory_.GetWeakPtr()),
      base::Milliseconds(100));
}

void RelayConnection::Disconnect() {
  if (state_ == RelayConnectionState::DISCONNECTED) {
    return;
  }
  
  // Cancel all timers
  connection_timer_.Stop();
  reconnect_timer_.Stop();
  query_timers_.clear();
  
  // Close WebSocket
  if (websocket_.is_bound()) {
    websocket_.reset();
  }
  
  // Clear callbacks
  if (connection_callback_) {
    std::move(connection_callback_).Run(false, "Disconnected");
  }
  
  // Fail all pending queries
  for (auto& [subscription_id, callback] : pending_queries_) {
    auto result = std::make_unique<QueryResult>();
    result->success = false;
    result->error_message = "Connection closed";
    std::move(callback).Run(std::move(result));
  }
  pending_queries_.clear();
  
  SetState(RelayConnectionState::DISCONNECTED);
  LOG(INFO) << "Disconnected from relay: " << relay_url_;
}

void RelayConnection::Reconnect() {
  if (state_ == RelayConnectionState::CONNECTING ||
      state_ == RelayConnectionState::RECONNECTING) {
    return;
  }
  
  SetState(RelayConnectionState::RECONNECTING);
  
  // Exponential backoff for reconnection
  int delay_seconds = kReconnectDelaySeconds * (1 << std::min(reconnect_attempts_, 5));
  reconnect_timer_.Start(
      FROM_HERE, base::Seconds(delay_seconds),
      base::BindOnce(&RelayConnection::Connect,
                     weak_factory_.GetWeakPtr(),
                     base::BindOnce([](bool success, const std::string& error) {
                       // Reconnection callback - log result
                       LOG_IF(ERROR, !success) << "Reconnection failed: " << error;
                     })));
  
  reconnect_attempts_++;
  LOG(INFO) << "Scheduled reconnection to " << relay_url_ 
            << " in " << delay_seconds << " seconds (attempt " 
            << reconnect_attempts_ << ")";
}

void RelayConnection::Subscribe(const std::string& subscription_id,
                               const std::vector<NostrFilter>& filters,
                               QueryCallback callback) {
  if (!IsConnected()) {
    auto result = std::make_unique<QueryResult>();
    result->success = false;
    result->error_message = "Not connected to relay";
    std::move(callback).Run(std::move(result));
    return;
  }
  
  // Store callback for this subscription
  pending_queries_[subscription_id] = std::move(callback);
  
  // Start query timeout
  auto timer = std::make_unique<base::OneShotTimer>();
  timer->Start(
      FROM_HERE, query_timeout_,
      base::BindOnce(&RelayConnection::OnQueryTimeout,
                     weak_factory_.GetWeakPtr(), subscription_id));
  query_timers_[subscription_id] = std::move(timer);
  
  // Send subscribe message
  std::string message = CreateSubscribeMessage(subscription_id, filters);
  SendMessage(message);
  
  LOG(INFO) << "Subscribed to relay " << relay_url_ 
            << " with subscription ID: " << subscription_id;
}

void RelayConnection::Unsubscribe(const std::string& subscription_id) {
  CloseSubscription(subscription_id);
}

void RelayConnection::CloseSubscription(const std::string& subscription_id) {
  // Remove from pending queries
  pending_queries_.erase(subscription_id);
  
  // Cancel query timer
  query_timers_.erase(subscription_id);
  
  // Send close message if connected
  if (IsConnected()) {
    std::string message = CreateUnsubscribeMessage(subscription_id);
    SendMessage(message);
  }
  
  LOG(INFO) << "Closed subscription: " << subscription_id;
}

void RelayConnection::OnWebSocketConnected() {
  connection_timer_.Stop();
  SetState(RelayConnectionState::CONNECTED);
  reconnect_attempts_ = 0;  // Reset reconnection counter
  
  if (connection_callback_) {
    std::move(connection_callback_).Run(true, "");
  }
  
  LOG(INFO) << "Connected to relay: " << relay_url_;
}

void RelayConnection::OnWebSocketDisconnected() {
  LOG(WARNING) << "WebSocket disconnected from relay: " << relay_url_;
  
  if (state_ == RelayConnectionState::CONNECTED) {
    // Unexpected disconnection - attempt reconnect
    if (reconnect_attempts_ < kMaxReconnectAttempts) {
      Reconnect();
    } else {
      SetState(RelayConnectionState::ERROR);
      LOG(ERROR) << "Max reconnection attempts reached for relay: " << relay_url_;
    }
  } else {
    SetState(RelayConnectionState::DISCONNECTED);
  }
}

void RelayConnection::OnWebSocketMessageReceived(const std::string& message) {
  ProcessRelayMessage(message);
}

void RelayConnection::OnWebSocketError() {
  LOG(ERROR) << "WebSocket error for relay: " << relay_url_;
  SetState(RelayConnectionState::ERROR);
  
  if (connection_callback_) {
    std::move(connection_callback_).Run(false, "WebSocket error");
  }
}

void RelayConnection::ProcessRelayMessage(const std::string& message) {
  // Parse JSON message from relay
  auto parsed = base::JSONReader::Read(message);
  if (!parsed || !parsed->is_list()) {
    LOG(WARNING) << "Invalid relay message format: " << message;
    return;
  }
  
  const auto& message_array = parsed->GetList();
  if (message_array.empty() || !message_array[0].is_string()) {
    LOG(WARNING) << "Invalid relay message structure: " << message;
    return;
  }
  
  const std::string& message_type = message_array[0].GetString();
  
  if (message_type == "EVENT" && message_array.size() >= 3) {
    // Handle incoming event: ["EVENT", subscription_id, event]
    const std::string& subscription_id = message_array[1].GetString();
    
    // Parse event from JSON
    auto event = NostrEvent::FromJSON(message_array[2]);
    if (event) {
      HandleEventMessage(subscription_id, std::move(event));
    } else {
      LOG(WARNING) << "Failed to parse event from relay message";
    }
    
  } else if (message_type == "EOSE" && message_array.size() >= 2) {
    // Handle end of stored events: ["EOSE", subscription_id]
    const std::string& subscription_id = message_array[1].GetString();
    HandleEOSEMessage(subscription_id);
    
  } else if (message_type == "NOTICE" && message_array.size() >= 2) {
    // Handle relay notice: ["NOTICE", message]
    const std::string& notice_message = message_array[1].GetString();
    HandleNoticeMessage(notice_message);
    
  } else {
    LOG(WARNING) << "Unknown relay message type: " << message_type;
  }
}

void RelayConnection::HandleEventMessage(const std::string& subscription_id,
                                        std::unique_ptr<NostrEvent> event) {
  // Notify event callback if set
  if (event_callback_) {
    event_callback_.Run(std::move(event));
  }
  
  // Note: Events are handled via callback, not stored in QueryResult
  // The QueryResult will be completed when EOSE is received
}

void RelayConnection::HandleEOSEMessage(const std::string& subscription_id) {
  // Find and complete the pending query
  auto it = pending_queries_.find(subscription_id);
  if (it != pending_queries_.end()) {
    auto result = std::make_unique<QueryResult>();
    result->success = true;
    result->end_of_stored_events = true;
    
    // Complete the query
    std::move(it->second).Run(std::move(result));
    pending_queries_.erase(it);
    
    // Cancel query timer
    query_timers_.erase(subscription_id);
  }
  
  LOG(INFO) << "Received EOSE for subscription: " << subscription_id;
}

void RelayConnection::HandleNoticeMessage(const std::string& message) {
  LOG(INFO) << "Relay notice from " << relay_url_ << ": " << message;
}

void RelayConnection::SetState(RelayConnectionState new_state) {
  if (state_ != new_state) {
    LOG(INFO) << "Relay " << relay_url_ << " state changed: " 
              << static_cast<int>(state_) << " -> " << static_cast<int>(new_state);
    state_ = new_state;
  }
}

void RelayConnection::OnConnectionTimeout() {
  LOG(ERROR) << "Connection timeout for relay: " << relay_url_;
  
  if (connection_callback_) {
    std::move(connection_callback_).Run(false, "Connection timeout");
  }
  
  SetState(RelayConnectionState::ERROR);
}

void RelayConnection::OnQueryTimeout(const std::string& subscription_id) {
  LOG(WARNING) << "Query timeout for subscription: " << subscription_id;
  
  auto it = pending_queries_.find(subscription_id);
  if (it != pending_queries_.end()) {
    auto result = std::make_unique<QueryResult>();
    result->success = false;
    result->error_message = "Query timeout";
    
    std::move(it->second).Run(std::move(result));
    pending_queries_.erase(it);
  }
  
  query_timers_.erase(subscription_id);
}

void RelayConnection::SendMessage(const std::string& message) {
  if (!IsConnected()) {
    LOG(WARNING) << "Attempted to send message when not connected: " << message;
    return;
  }
  
  // TODO: Send via WebSocket
  LOG(INFO) << "Sending to relay " << relay_url_ << ": " << message;
}

std::string RelayConnection::CreateSubscribeMessage(
    const std::string& subscription_id,
    const std::vector<NostrFilter>& filters) {
  base::Value::List message_array;
  message_array.Append("REQ");
  message_array.Append(subscription_id);
  
  // Add filters
  for (const auto& filter : filters) {
    message_array.Append(filter.ToJSON());
  }
  
  std::string json_string;
  base::JSONWriter::Write(message_array, &json_string);
  return json_string;
}

std::string RelayConnection::CreateUnsubscribeMessage(
    const std::string& subscription_id) {
  base::Value::List message_array;
  message_array.Append("CLOSE");
  message_array.Append(subscription_id);
  
  std::string json_string;
  base::JSONWriter::Write(message_array, &json_string);
  return json_string;
}

}  // namespace nostr