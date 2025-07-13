// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_PROTOCOL_NIP19_H_
#define CHROME_BROWSER_NOSTR_PROTOCOL_NIP19_H_

#include <string>
#include <vector>
#include <optional>
#include <variant>

namespace nostr {
namespace nip19 {

// NIP-19 entity types
enum class EntityType {
  kNpub,     // Public key
  kNsec,     // Secret key (should warn/block)
  kNote,     // Event ID
  kNprofile, // Public key + relay hints
  kNevent,   // Event ID + relay hints + author
  kNaddr,    // Parameterized replaceable event
  kUnknown
};

// TLV (Type-Length-Value) types for extended formats
enum class TLVType : uint8_t {
  kSpecial = 0,
  kRelay = 1,
  kAuthor = 2,
  kKind = 3
};

// Base entity
struct Entity {
  EntityType type;
  std::string hex_id;  // Hex-encoded pubkey or event ID
};

// Extended entity with TLV data
struct ExtendedEntity : Entity {
  std::vector<std::string> relays;
  std::optional<std::string> author;  // For nevent
  std::optional<int> kind;            // For naddr
  std::optional<std::string> identifier;  // For naddr
};

// Decode result type
using DecodeResult = std::variant<Entity, ExtendedEntity>;

// Decode a NIP-19 bech32 entity
std::optional<DecodeResult> Decode(const std::string& bech32_entity);

// Encode entities
std::string EncodeNpub(const std::string& hex_pubkey);
std::string EncodeNote(const std::string& hex_event_id);
std::string EncodeNprofile(const std::string& hex_pubkey,
                          const std::vector<std::string>& relays);
std::string EncodeNevent(const std::string& hex_event_id,
                        const std::vector<std::string>& relays,
                        const std::string& author);
std::string EncodeNaddr(int kind,
                       const std::string& hex_pubkey,
                       const std::string& identifier,
                       const std::vector<std::string>& relays);

// Get entity type from HRP
EntityType GetEntityType(const std::string& hrp);

// Parse TLV data
bool ParseTLV(const std::vector<uint8_t>& data, ExtendedEntity& entity);

}  // namespace nip19
}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_PROTOCOL_NIP19_H_