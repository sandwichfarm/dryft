// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/local_relay/protocol_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/nostr/local_relay/connection_manager.h"
#include "chrome/browser/nostr/local_relay/event_storage.h"
#include "chrome/browser/nostr/local_relay/query_engine.h"
#include "components/nostr/nostr_event.h"
#include "components/nostr/nostr_filter.h"
#include "crypto/sha2.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace nostr {
namespace local_relay {

namespace {

// Nostr message types
const char kEventType[] = "EVENT";
const char kReqType[] = "REQ";
const char kCloseType[] = "CLOSE";
const char kAuthType[] = "AUTH";

// Response types
const char kOkType[] = "OK";
const char kNoticeType[] = "NOTICE";
const char kEoseType[] = "EOSE";

// Error messages
const char kInvalidMessage[] = "invalid message format";
const char kInvalidEvent[] = "invalid event";
const char kInvalidFilter[] = "invalid filter";
const char kRateLimited[] = "rate limited";
const char kSubscriptionClosed[] = "subscription closed";
const char kDuplicateEvent[] = "duplicate event";

// Convert hex string to bytes
std::vector<uint8_t> HexToBytes(const std::string& hex) {
  std::vector<uint8_t> bytes;
  if (hex.length() % 2 != 0) {
    return bytes;
  }
  
  bytes.reserve(hex.length() / 2);
  for (size_t i = 0; i < hex.length(); i += 2) {
    uint8_t byte;
    if (!base::HexStringToUInt8(hex.substr(i, 2), &byte)) {
      return std::vector<uint8_t>();
    }
    bytes.push_back(byte);
  }
  return bytes;
}

// Serialize event for ID calculation
std::string SerializeEventForId(const NostrEvent& event) {
  // Format: [0,pubkey,created_at,kind,tags,content]
  base::Value::List event_data;
  event_data.Append(0);
  event_data.Append(event.pubkey);
  event_data.Append(static_cast<double>(event.created_at));
  event_data.Append(event.kind);
  event_data.Append(event.tags.Clone());
  event_data.Append(event.content);
  
  std::string json;
  if (!base::JSONWriter::Write(event_data, &json)) {
    return std::string();
  }
  
  return json;
}

}  // namespace

// ProtocolResponse implementation

ProtocolResponse ProtocolResponse::MakeOK(const std::string& event_id,
                                         bool accepted,
                                         const std::string& message) {
  ProtocolResponse response;
  response.type = OK;
  response.parameters.push_back(base::Value(event_id));
  response.parameters.push_back(base::Value(accepted));
  response.parameters.push_back(base::Value(message));
  return response;
}

ProtocolResponse ProtocolResponse::MakeEvent(const std::string& subscription_id,
                                            const NostrEvent& event) {
  ProtocolResponse response;
  response.type = EVENT;
  response.parameters.push_back(base::Value(subscription_id));
  response.parameters.push_back(event.ToValue());
  return response;
}

ProtocolResponse ProtocolResponse::MakeEOSE(const std::string& subscription_id) {
  ProtocolResponse response;
  response.type = EOSE;
  response.parameters.push_back(base::Value(subscription_id));
  return response;
}

ProtocolResponse ProtocolResponse::MakeNotice(const std::string& message) {
  ProtocolResponse response;
  response.type = NOTICE;
  response.parameters.push_back(base::Value(message));
  return response;
}

ProtocolResponse ProtocolResponse::MakeAuth(const std::string& challenge) {
  ProtocolResponse response;
  response.type = AUTH;
  response.parameters.push_back(base::Value(challenge));
  return response;
}

std::string ProtocolResponse::ToJson() const {
  base::Value::List message;
  
  // Add message type
  switch (type) {
    case OK:
      message.Append(kOkType);
      break;
    case EVENT:
      message.Append(kEventType);
      break;
    case EOSE:
      message.Append(kEoseType);
      break;
    case NOTICE:
      message.Append(kNoticeType);
      break;
    case AUTH:
      message.Append(kAuthType);
      break;
  }
  
  // Add parameters
  for (const auto& param : parameters) {
    message.Append(param.Clone());
  }
  
  std::string json;
  if (!base::JSONWriter::Write(message, &json)) {
    return "[]";
  }
  
  return json;
}

// ProtocolHandler implementation

ProtocolHandler::ProtocolHandler(EventStorage* event_storage,
                               ConnectionManager* connection_manager,
                               SendResponseCallback send_callback,
                               BroadcastCallback broadcast_callback)
    : event_storage_(event_storage),
      connection_manager_(connection_manager),
      send_callback_(send_callback),
      broadcast_callback_(broadcast_callback) {
  DCHECK(event_storage_);
  DCHECK(connection_manager_);
}

ProtocolHandler::~ProtocolHandler() = default;

void ProtocolHandler::ProcessMessage(int connection_id,
                                   const std::string& message) {
  // Update connection statistics
  auto* connection = connection_manager_->GetConnection(connection_id);
  if (connection) {
    connection->messages_received++;
  }
  
  // Parse JSON array
  auto json_value = base::JSONReader::Read(message);
  if (!json_value || !json_value->is_list()) {
    SendNotice(connection_id, kInvalidMessage);
    return;
  }
  
  const auto& message_array = json_value->GetList();
  if (message_array.empty()) {
    SendNotice(connection_id, kInvalidMessage);
    return;
  }
  
  // Get message type
  const auto* type_string = message_array[0].GetIfString();
  if (!type_string) {
    SendNotice(connection_id, kInvalidMessage);
    return;
  }
  
  // Dispatch to appropriate handler
  if (*type_string == kEventType) {
    HandleEvent(connection_id, message_array);
  } else if (*type_string == kReqType) {
    HandleReq(connection_id, message_array);
  } else if (*type_string == kCloseType) {
    HandleClose(connection_id, message_array);
  } else if (*type_string == kAuthType) {
    HandleAuth(connection_id, message_array);
  } else {
    SendNotice(connection_id, kInvalidMessage);
  }
}

void ProtocolHandler::HandleEvent(int connection_id,
                                const base::Value::List& message) {
  // EVENT format: ["EVENT", event_object]
  if (message.size() != 2) {
    SendNotice(connection_id, kInvalidMessage);
    return;
  }
  
  const auto* event_dict = message[1].GetIfDict();
  if (!event_dict) {
    SendNotice(connection_id, kInvalidMessage);
    return;
  }
  
  // Parse event
  auto event = NostrEvent::FromValue(*event_dict);
  if (!event) {
    SendNotice(connection_id, kInvalidEvent);
    return;
  }
  
  // Check rate limit
  if (!connection_manager_->CheckEventRateLimit(connection_id, 100)) {
    SendOK(connection_id, event->id, false, kRateLimited);
    return;
  }
  
  connection_manager_->RecordEvent(connection_id);
  
  // Process event
  ProcessEventMessage(connection_id, std::move(event));
}

void ProtocolHandler::HandleReq(int connection_id,
                              const base::Value::List& message) {
  // REQ format: ["REQ", subscription_id, filter1, filter2, ...]
  if (message.size() < 3) {
    SendNotice(connection_id, kInvalidMessage);
    return;
  }
  
  const auto* subscription_id = message[1].GetIfString();
  if (!subscription_id || subscription_id->empty() || 
      subscription_id->length() > max_subscription_id_length_) {
    SendNotice(connection_id, kInvalidMessage);
    return;
  }
  
  // Check rate limit
  if (!connection_manager_->CheckReqRateLimit(connection_id, 60)) {
    SendNotice(connection_id, kRateLimited);
    return;
  }
  
  connection_manager_->RecordReq(connection_id);
  
  // Parse filters
  std::vector<NostrFilter> filters;
  for (size_t i = 2; i < message.size() && i < 2 + max_filters_per_req_; ++i) {
    const auto* filter_dict = message[i].GetIfDict();
    if (!filter_dict) {
      SendNotice(connection_id, kInvalidFilter);
      return;
    }
    
    auto filter = ParseFilter(*filter_dict);
    if (!filter) {
      SendNotice(connection_id, kInvalidFilter);
      return;
    }
    
    filters.push_back(std::move(*filter));
  }
  
  ProcessReqMessage(connection_id, *subscription_id, filters);
}

void ProtocolHandler::HandleClose(int connection_id,
                                const base::Value::List& message) {
  // CLOSE format: ["CLOSE", subscription_id]
  if (message.size() != 2) {
    SendNotice(connection_id, kInvalidMessage);
    return;
  }
  
  const auto* subscription_id = message[1].GetIfString();
  if (!subscription_id) {
    SendNotice(connection_id, kInvalidMessage);
    return;
  }
  
  // Remove subscription
  if (connection_manager_->RemoveSubscription(connection_id, *subscription_id)) {
    VLOG(2) << "Closed subscription " << *subscription_id 
            << " for connection " << connection_id;
  } else {
    SendNotice(connection_id, kSubscriptionClosed);
  }
}

void ProtocolHandler::HandleAuth(int connection_id,
                               const base::Value::List& message) {
  // AUTH format: ["AUTH", event_object]
  // NIP-42 authentication - not implemented yet
  SendNotice(connection_id, "AUTH not implemented");
}

void ProtocolHandler::ProcessEventMessage(int connection_id,
                                        std::unique_ptr<NostrEvent> event) {
  // Validate event
  std::string error;
  if (!ValidateEvent(*event, error)) {
    SendOK(connection_id, event->id, false, error);
    return;
  }
  
  // Store event using EventStorage
  std::string event_id = event->id;
  NostrEvent event_copy = *event;
  
  event_storage_->StoreEvent(
      std::move(event),
      base::BindOnce(
          [](base::WeakPtr<ProtocolHandler> handler,
             int connection_id,
             std::string event_id,
             NostrEvent event_copy,
             bool success,
             const std::string& error) {
            if (!handler) return;
            
            if (success) {
              handler->SendOK(connection_id, event_id, true, "");
              // Broadcast to matching subscriptions
              handler->BroadcastEvent(event_copy);
            } else {
              handler->SendOK(connection_id, event_id, false, 
                            error.empty() ? kDuplicateEvent : error);
            }
          },
          weak_factory_.GetWeakPtr(),
          connection_id,
          event_id,
          event_copy));
}

void ProtocolHandler::ProcessReqMessage(
    int connection_id,
    const std::string& subscription_id,
    const std::vector<NostrFilter>& filters) {
  // Add subscription
  if (!connection_manager_->AddSubscription(connection_id, subscription_id, 
                                          filters)) {
    SendNotice(connection_id, "too many subscriptions");
    return;
  }
  
  // Query historical events
  QueryAndSendEvents(connection_id, subscription_id, filters);
}

bool ProtocolHandler::ValidateEvent(const NostrEvent& event, 
                                   std::string& error) {
  // Check required fields
  if (event.id.length() != 64) {
    error = "invalid id length";
    return false;
  }
  
  if (event.pubkey.length() != 64) {
    error = "invalid pubkey length";
    return false;
  }
  
  if (event.sig.length() != 128) {
    error = "invalid signature length";
    return false;
  }
  
  // Verify event ID
  if (!ValidateEventId(event)) {
    error = "invalid event id";
    return false;
  }
  
  // Verify signature (simplified - real implementation would use secp256k1)
  if (!VerifyEventSignature(event)) {
    error = "invalid signature";
    return false;
  }
  
  // Check event size
  std::string event_json = base::WriteJson(event.ToValue()).value_or("");
  if (event_json.size() > max_event_size_) {
    error = "event too large";
    return false;
  }
  
  return true;
}

bool ProtocolHandler::VerifyEventSignature(const NostrEvent& event) {
  // Simplified signature verification
  // Real implementation would:
  // 1. Use secp256k1 library
  // 2. Verify schnorr signature over event ID
  // 3. Check public key matches
  
  // For now, just check format
  auto sig_bytes = HexToBytes(event.sig);
  auto pubkey_bytes = HexToBytes(event.pubkey);
  
  return sig_bytes.size() == 64 && pubkey_bytes.size() == 32;
}

bool ProtocolHandler::ValidateEventId(const NostrEvent& event) {
  // Calculate expected event ID
  std::string serialized = SerializeEventForId(event);
  if (serialized.empty()) {
    return false;
  }
  
  // Calculate SHA256
  std::string hash = crypto::SHA256HashString(serialized);
  std::string calculated_id = base::HexEncode(hash.data(), hash.size());
  base::ToLowerASCII(&calculated_id);
  
  return calculated_id == event.id;
}

std::unique_ptr<NostrFilter> ProtocolHandler::ParseFilter(
    const base::Value::Dict& filter_json) {
  auto filter = std::make_unique<NostrFilter>();
  
  // Parse IDs
  const auto* ids = filter_json.FindList("ids");
  if (ids) {
    for (const auto& id : *ids) {
      const auto* id_str = id.GetIfString();
      if (id_str && id_str->length() <= 64) {
        filter->ids.push_back(*id_str);
      }
    }
  }
  
  // Parse authors
  const auto* authors = filter_json.FindList("authors");
  if (authors) {
    for (const auto& author : *authors) {
      const auto* author_str = author.GetIfString();
      if (author_str && author_str->length() <= 64) {
        filter->authors.push_back(*author_str);
      }
    }
  }
  
  // Parse kinds
  const auto* kinds = filter_json.FindList("kinds");
  if (kinds) {
    for (const auto& kind : *kinds) {
      if (kind.is_int()) {
        filter->kinds.push_back(kind.GetInt());
      }
    }
  }
  
  // Parse since/until
  const auto since = filter_json.FindInt("since");
  if (since) {
    filter->since = *since;
  }
  
  const auto until = filter_json.FindInt("until");
  if (until) {
    filter->until = *until;
  }
  
  // Parse limit
  const auto limit = filter_json.FindInt("limit");
  if (limit && *limit > 0) {
    filter->limit = *limit;
  }
  
  // Parse tags (#e, #p, etc)
  for (const auto& [key, value] : filter_json) {
    if (key.length() == 2 && key[0] == '#') {
      const auto* tag_values = value.GetIfList();
      if (tag_values) {
        std::vector<std::string> values;
        for (const auto& v : *tag_values) {
          const auto* v_str = v.GetIfString();
          if (v_str) {
            values.push_back(*v_str);
          }
        }
        if (!values.empty()) {
          filter->tags[key.substr(1)] = values;
        }
      }
    }
  }
  
  return filter;
}

void ProtocolHandler::SendOK(int connection_id,
                           const std::string& event_id,
                           bool accepted,
                           const std::string& message) {
  auto response = ProtocolResponse::MakeOK(event_id, accepted, message);
  send_callback_.Run(connection_id, response.ToJson());
}

void ProtocolHandler::SendEvent(int connection_id,
                              const std::string& subscription_id,
                              const NostrEvent& event) {
  auto response = ProtocolResponse::MakeEvent(subscription_id, event);
  send_callback_.Run(connection_id, response.ToJson());
}

void ProtocolHandler::SendEOSE(int connection_id,
                             const std::string& subscription_id) {
  auto response = ProtocolResponse::MakeEOSE(subscription_id);
  send_callback_.Run(connection_id, response.ToJson());
}

void ProtocolHandler::SendNotice(int connection_id,
                               const std::string& message) {
  auto response = ProtocolResponse::MakeNotice(message);
  send_callback_.Run(connection_id, response.ToJson());
}

void ProtocolHandler::BroadcastEvent(const NostrEvent& event) {
  // Find all connections with matching subscriptions
  auto matching_connections = connection_manager_->GetMatchingConnections(event);
  
  for (int conn_id : matching_connections) {
    auto matching_subs = connection_manager_->GetMatchingSubscriptions(
        conn_id, event);
    
    for (const auto& sub_id : matching_subs) {
      SendEvent(conn_id, sub_id, event);
    }
  }
}

void ProtocolHandler::QueryAndSendEvents(
    int connection_id,
    const std::string& subscription_id,
    const std::vector<NostrFilter>& filters) {
  // Query events using EventStorage
  QueryOptions options;
  options.limit = 1000;
  
  event_storage_->QueryEventsStreaming(
      filters,
      options,
      base::BindRepeating(
          [](base::WeakPtr<ProtocolHandler> handler,
             int connection_id,
             std::string subscription_id,
             std::unique_ptr<NostrEvent> event) {
            if (!handler || !event) return;
            handler->SendEvent(connection_id, subscription_id, *event);
          },
          weak_factory_.GetWeakPtr(),
          connection_id,
          subscription_id),
      base::BindOnce(
          [](base::WeakPtr<ProtocolHandler> handler,
             int connection_id,
             std::string subscription_id) {
            if (!handler) return;
            // Send EOSE to indicate end of stored events
            handler->SendEOSE(connection_id, subscription_id);
          },
          weak_factory_.GetWeakPtr(),
          connection_id,
          subscription_id));
}

}  // namespace local_relay
}  // namespace nostr