// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOSTR_NOSTR_EVENT_H_
#define COMPONENTS_NOSTR_NOSTR_EVENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"

namespace nostr {

// Represents a Nostr event as defined in NIP-01
struct NostrEvent {
  // Required fields
  std::string id;          // 32-byte SHA256 hex of serialized event
  std::string pubkey;      // 32-byte public key hex
  int64_t created_at;      // Unix timestamp in seconds
  int kind;                // Event kind number
  base::Value::List tags;  // Array of tag arrays
  std::string content;     // Arbitrary string content
  std::string sig;         // 64-byte schnorr signature hex
  
  // Local relay metadata (not part of protocol)
  base::Time received_at;  // When the relay received this event
  
  NostrEvent();
  ~NostrEvent();
  
  // Serialization
  base::Value ToValue() const;
  static std::unique_ptr<NostrEvent> FromValue(const base::Value::Dict& value);
  
  // Helpers
  bool IsValid() const;
  bool IsEphemeral() const { return kind >= 20000 && kind < 30000; }
  bool IsReplaceable() const { 
    return kind == 0 || kind == 3 || (kind >= 10000 && kind < 20000);
  }
  bool IsParameterizedReplaceable() const { 
    return kind >= 30000 && kind < 40000; 
  }
  
  // Get the 'd' tag value for parameterized replaceable events
  std::string GetDTagValue() const;
  
  // Check if event has a specific tag
  bool HasTag(const std::string& tag_name) const;
  std::vector<std::string> GetTagValues(const std::string& tag_name) const;
};

}  // namespace nostr

#endif  // COMPONENTS_NOSTR_NOSTR_EVENT_H_