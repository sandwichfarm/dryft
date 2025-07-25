// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_LOCAL_RELAY_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_NOSTR_LOCAL_RELAY_PROTOCOL_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"

namespace nostr {

// Forward declarations
struct NostrEvent;
struct NostrFilter;

namespace local_relay {

// Forward declarations
class NostrDatabase;
class ConnectionManager;

// Response types for protocol messages
struct ProtocolResponse {
  enum Type {
    OK,      // ["OK", event_id, accepted, message]
    EVENT,   // ["EVENT", subscription_id, event]
    EOSE,    // ["EOSE", subscription_id]
    NOTICE,  // ["NOTICE", message]
    AUTH,    // ["AUTH", challenge]
  };
  
  Type type;
  std::vector<base::Value> parameters;
  
  // Helper constructors
  static ProtocolResponse MakeOK(const std::string& event_id,
                                bool accepted,
                                const std::string& message);
  static ProtocolResponse MakeEvent(const std::string& subscription_id,
                                   const NostrEvent& event);
  static ProtocolResponse MakeEOSE(const std::string& subscription_id);
  static ProtocolResponse MakeNotice(const std::string& message);
  static ProtocolResponse MakeAuth(const std::string& challenge);
  
  // Serialize to JSON array string
  std::string ToJson() const;
};

// Handles Nostr protocol messages according to NIP-01
class ProtocolHandler {
 public:
  // Callbacks for sending responses
  using SendResponseCallback = 
      base::RepeatingCallback<void(int connection_id, const std::string& message)>;
  using BroadcastCallback = 
      base::RepeatingCallback<void(const std::vector<int>& connection_ids,
                                  const std::string& subscription_id,
                                  const std::string& message)>;

  ProtocolHandler(NostrDatabase* database,
                  ConnectionManager* connection_manager,
                  SendResponseCallback send_callback,
                  BroadcastCallback broadcast_callback);
  ~ProtocolHandler();

  // Process incoming WebSocket message
  void ProcessMessage(int connection_id, const std::string& message);
  
  // Configuration
  void SetMaxEventSize(size_t max_size) { max_event_size_ = max_size; }
  void SetMaxFilters(size_t max_filters) { max_filters_per_req_ = max_filters; }
  void SetMaxSubscriptionIdLength(size_t max_length) { 
    max_subscription_id_length_ = max_length; 
  }

 private:
  // Message type handlers
  void HandleEvent(int connection_id, const base::Value::List& message);
  void HandleReq(int connection_id, const base::Value::List& message);
  void HandleClose(int connection_id, const base::Value::List& message);
  void HandleAuth(int connection_id, const base::Value::List& message);
  
  // EVENT message processing
  void ProcessEventMessage(int connection_id,
                          std::unique_ptr<NostrEvent> event);
  
  // REQ message processing  
  void ProcessReqMessage(int connection_id,
                        const std::string& subscription_id,
                        const std::vector<NostrFilter>& filters);
  
  // Event validation
  bool ValidateEvent(const NostrEvent& event, std::string& error);
  bool VerifyEventSignature(const NostrEvent& event);
  bool ValidateEventId(const NostrEvent& event);
  
  // Filter parsing
  std::unique_ptr<NostrFilter> ParseFilter(const base::Value::Dict& filter_json);
  
  // Send responses
  void SendOK(int connection_id,
              const std::string& event_id,
              bool accepted,
              const std::string& message);
  void SendEvent(int connection_id,
                const std::string& subscription_id,
                const NostrEvent& event);
  void SendEOSE(int connection_id, const std::string& subscription_id);
  void SendNotice(int connection_id, const std::string& message);
  
  // Broadcast event to matching subscriptions
  void BroadcastEvent(const NostrEvent& event);
  
  // Query database and send matching events
  void QueryAndSendEvents(int connection_id,
                         const std::string& subscription_id,
                         const std::vector<NostrFilter>& filters);
  
  // Dependencies
  NostrDatabase* database_;  // Not owned
  ConnectionManager* connection_manager_;  // Not owned
  
  // Callbacks
  SendResponseCallback send_callback_;
  BroadcastCallback broadcast_callback_;
  
  // Configuration
  size_t max_event_size_ = 256 * 1024;  // 256KB
  size_t max_filters_per_req_ = 10;
  size_t max_subscription_id_length_ = 64;
  
  // Weak pointer factory
  base::WeakPtrFactory<ProtocolHandler> weak_factory_{this};
};

}  // namespace local_relay
}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_LOCAL_RELAY_PROTOCOL_HANDLER_H_