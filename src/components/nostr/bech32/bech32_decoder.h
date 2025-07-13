// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOSTR_BECH32_BECH32_DECODER_H_
#define COMPONENTS_NOSTR_BECH32_BECH32_DECODER_H_

#include <memory>
#include <string>
#include <vector>
#include <map>

#include "base/types/expected.h"

namespace nostr {
namespace bech32 {

// Error types for Bech32 decoding
enum class DecodeError {
  INVALID_CHARACTER,
  INVALID_CHECKSUM,
  INVALID_LENGTH,
  UNKNOWN_HRP,
  MALFORMED_DATA,
  INVALID_TLV,
  UNSUPPORTED_ENTITY,
};

// TLV (Type-Length-Value) entry
struct TlvEntry {
  uint8_t type;
  std::vector<uint8_t> value;
};

// Decoded Nostr entity base class
struct NostrEntity {
  enum Type {
    NPUB,      // Public key
    NOTE,      // Event ID  
    NPROFILE,  // Profile with relays
    NEVENT,    // Event with relays and author
    NADDR,     // Parameterized replaceable event
    NSEC,      // Private key (should be blocked/warned)
  };
  
  Type type;
  std::string raw_data;  // Original bech32 string
  
  virtual ~NostrEntity() = default;
  
protected:
  explicit NostrEntity(Type t, const std::string& data) 
      : type(t), raw_data(data) {}
};

// Simple entities (npub, note)
struct SimpleEntity : public NostrEntity {
  std::vector<uint8_t> data;  // 32 bytes for npub/note
  
  SimpleEntity(Type type, const std::string& raw, std::vector<uint8_t> entity_data)
      : NostrEntity(type, raw), data(std::move(entity_data)) {}
};

// Complex entities with TLV data (nprofile, nevent, naddr)
struct ComplexEntity : public NostrEntity {
  std::vector<uint8_t> primary_data;  // Main data (pubkey, event ID, etc.)
  std::vector<TlvEntry> tlv_entries;   // Additional TLV data
  
  ComplexEntity(Type type, const std::string& raw, 
                std::vector<uint8_t> primary, 
                std::vector<TlvEntry> tlv)
      : NostrEntity(type, raw), 
        primary_data(std::move(primary)), 
        tlv_entries(std::move(tlv)) {}
  
  // Helper methods to extract common TLV types
  std::vector<std::string> GetRelays() const;
  std::string GetAuthor() const;  // For nevent
  std::string GetKind() const;    // For naddr
  std::string GetIdentifier() const;  // For naddr
};

// Main Bech32 decoder class
class Bech32Decoder {
 public:
  Bech32Decoder();
  ~Bech32Decoder();

  // Decode a bech32 string into a Nostr entity
  base::expected<std::unique_ptr<NostrEntity>, DecodeError> 
  DecodeNostrEntity(const std::string& bech32_str);

  // Low-level Bech32 decode (returns raw data)
  base::expected<std::pair<std::string, std::vector<uint8_t>>, DecodeError>
  DecodeBech32(const std::string& bech32_str);

  // Encode data to bech32 (for testing/utility)
  base::expected<std::string, DecodeError>
  EncodeBech32(const std::string& hrp, const std::vector<uint8_t>& data);

  // Utility methods
  static std::string GetErrorMessage(DecodeError error);
  static bool IsValidNostrEntity(const std::string& bech32_str);

 private:
  // Core Bech32 functions
  bool VerifyChecksum(const std::string& hrp, const std::vector<uint8_t>& data);
  uint32_t ComputeChecksum(const std::string& hrp, const std::vector<uint8_t>& data);
  std::vector<uint8_t> ConvertBits(const std::vector<uint8_t>& data, 
                                  int from_bits, int to_bits, bool pad);
  
  // Character mapping
  int32_t CharToInt(char c);
  char IntToChar(int32_t i);
  
  // TLV parsing
  base::expected<std::vector<TlvEntry>, DecodeError>
  ParseTlv(const std::vector<uint8_t>& data, size_t offset);
  
  // Entity-specific decoders
  std::unique_ptr<NostrEntity> DecodeSimpleEntity(
      NostrEntity::Type type, const std::string& raw_data, 
      const std::vector<uint8_t>& data);
  
  std::unique_ptr<NostrEntity> DecodeComplexEntity(
      NostrEntity::Type type, const std::string& raw_data,
      const std::vector<uint8_t>& data);

  // HRP to entity type mapping
  static const std::map<std::string, NostrEntity::Type> kHrpToType;
  static const char kBech32Charset[];
  static const uint32_t kBech32Generator[];
};

}  // namespace bech32
}  // namespace nostr

#endif  // COMPONENTS_NOSTR_BECH32_BECH32_DECODER_H_