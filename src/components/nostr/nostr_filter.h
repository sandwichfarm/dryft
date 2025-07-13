// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOSTR_NOSTR_FILTER_H_
#define COMPONENTS_NOSTR_NOSTR_FILTER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/values.h"

namespace nostr {

// Represents a Nostr filter as defined in NIP-01
struct NostrFilter {
  // Event IDs to filter by (prefix matching supported)
  std::vector<std::string> ids;
  
  // Authors (public keys) to filter by (prefix matching supported)
  std::vector<std::string> authors;
  
  // Event kinds to filter by
  std::vector<int> kinds;
  
  // Time range filters (Unix timestamps)
  std::optional<int64_t> since;
  std::optional<int64_t> until;
  
  // Maximum number of events to return
  std::optional<int> limit;
  
  // Tag filters (e.g., {"e": ["event_id"], "p": ["pubkey"]})
  std::map<std::string, std::vector<std::string>> tags;
  
  NostrFilter();
  ~NostrFilter();
  
  // Serialization
  base::Value ToValue() const;
  static std::unique_ptr<NostrFilter> FromValue(const base::Value::Dict& value);
  
  // Generate SQL WHERE clause components
  std::string ToSQLWhereClause() const;
  
  // Check if filter is empty (matches everything)
  bool IsEmpty() const;
};

}  // namespace nostr

#endif  // COMPONENTS_NOSTR_NOSTR_FILTER_H_