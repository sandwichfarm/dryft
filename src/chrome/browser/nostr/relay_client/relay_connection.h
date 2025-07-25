// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_RELAY_CLIENT_RELAY_CONNECTION_H_
#define CHROME_BROWSER_NOSTR_RELAY_CLIENT_RELAY_CONNECTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/nostr/nostr_event.h"
#include "components/nostr/nostr_filter.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "url/gurl.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace nostr {

// Forward declarations
class NostrEvent;
class NostrFilter;

// Connection state for external Nostr relay
enum class RelayConnectionState {
  DISCONNECTED,
  CONNECTING,
  CONNECTED,
  ERROR,
  RECONNECTING
};

// Result of a relay query operation
struct QueryResult {
  bool success = false;
  std::string error_message;
  std::vector<std::unique_ptr<NostrEvent>> events;
  bool end_of_stored_events = false;  // EOSE received
};

// WebSocket-based connection to an external Nostr relay
class RelayConnection : public network::mojom::WebSocketClient {
 public:
  // Callback types for relay operations
  using ConnectionCallback = base::OnceCallback<void(bool success, const std::string& error)>;
  using QueryCallback = base::OnceCallback<void(std::unique_ptr<QueryResult>)>;
  using EventCallback = base::RepeatingCallback<void(std::unique_ptr<NostrEvent>)>;
  
  explicit RelayConnection(const GURL& relay_url);
  ~RelayConnection();

  // Connection management
  void Connect(ConnectionCallback callback);
  void Disconnect();
  void Reconnect();
  
  // Query operations
  void Subscribe(const std::string& subscription_id,
                 const std::vector<NostrFilter>& filters,
                 QueryCallback callback);
  void Unsubscribe(const std::string& subscription_id);
  void CloseSubscription(const std::string& subscription_id);
  
  // State and info
  RelayConnectionState state() const { return state_; }
  const GURL& url() const { return relay_url_; }
  bool IsConnected() const { return state_ == RelayConnectionState::CONNECTED; }
  
  // Event handling
  void SetEventCallback(EventCallback callback) { event_callback_ = std::move(callback); }
  
  // Timeout configuration
  void SetConnectionTimeout(base::TimeDelta timeout) { connection_timeout_ = timeout; }
  void SetQueryTimeout(base::TimeDelta timeout) { query_timeout_ = timeout; }

 private:
  // WebSocket message handling
  void OnWebSocketConnected();
  void OnWebSocketDisconnected();
  void OnWebSocketMessageReceived(const std::string& message);
  void OnWebSocketError();
  
  // Message processing
  void ProcessRelayMessage(const std::string& message);
  void HandleEventMessage(const std::string& subscription_id, 
                         std::unique_ptr<NostrEvent> event);
  void HandleEOSEMessage(const std::string& subscription_id);
  void HandleNoticeMessage(const std::string& message);
  
  // Connection state management
  void SetState(RelayConnectionState new_state);
  void OnConnectionTimeout();
  void OnQueryTimeout(const std::string& subscription_id);
  void ScheduleReconnect();
  
  // Message sending
  void SendMessage(const std::string& message);
  std::string CreateSubscribeMessage(const std::string& subscription_id,
                                   const std::vector<NostrFilter>& filters);
  std::string CreateUnsubscribeMessage(const std::string& subscription_id);
  
  GURL relay_url_;
  RelayConnectionState state_ = RelayConnectionState::DISCONNECTED;
  
  // WebSocket connection
  mojo::Remote<network::mojom::WebSocket> websocket_;
  mojo::Receiver<network::mojom::WebSocketClient> websocket_client_receiver_{this};
  
  // Callbacks and subscriptions
  ConnectionCallback connection_callback_;
  std::map<std::string, QueryCallback> pending_queries_;
  EventCallback event_callback_;
  
  // Timeouts and reconnection
  base::TimeDelta connection_timeout_ = base::Seconds(10);
  base::TimeDelta query_timeout_ = base::Seconds(30);
  base::OneShotTimer connection_timer_;
  std::map<std::string, std::unique_ptr<base::OneShotTimer>> query_timers_;
  base::OneShotTimer reconnect_timer_;
  int reconnect_attempts_ = 0;
  static constexpr int kMaxReconnectAttempts = 5;
  static constexpr int kReconnectDelaySeconds = 5;
  
  base::WeakPtrFactory<RelayConnection> weak_factory_{this};
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_RELAY_CLIENT_RELAY_CONNECTION_H_