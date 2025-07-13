// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/protocol/nip19.h"
#include "chrome/browser/nostr/protocol/bech32.h"

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace nostr {
namespace nip19 {

namespace {

// Convert hex string to bytes
std::vector<uint8_t> HexToBytes(const std::string& hex) {
  std::vector<uint8_t> bytes;
  for (size_t i = 0; i < hex.length(); i += 2) {
    std::string byte_string = hex.substr(i, 2);
    uint8_t byte = static_cast<uint8_t>(std::stoul(byte_string, nullptr, 16));
    bytes.push_back(byte);
  }
  return bytes;
}

// Convert bytes to hex string
std::string BytesToHex(const std::vector<uint8_t>& bytes) {
  std::stringstream ss;
  for (uint8_t byte : bytes) {
    ss << std::hex << std::setw(2) << std::setfill('0') 
       << static_cast<int>(byte);
  }
  return ss.str();
}

// Encode TLV data
std::vector<uint8_t> EncodeTLV(TLVType type, const std::vector<uint8_t>& value) {
  std::vector<uint8_t> tlv;
  tlv.push_back(static_cast<uint8_t>(type));
  tlv.push_back(static_cast<uint8_t>(value.size()));
  tlv.insert(tlv.end(), value.begin(), value.end());
  return tlv;
}

}  // namespace

EntityType GetEntityType(const std::string& hrp) {
  if (hrp == "npub") return EntityType::kNpub;
  if (hrp == "nsec") return EntityType::kNsec;
  if (hrp == "note") return EntityType::kNote;
  if (hrp == "nprofile") return EntityType::kNprofile;
  if (hrp == "nevent") return EntityType::kNevent;
  if (hrp == "naddr") return EntityType::kNaddr;
  return EntityType::kUnknown;
}

bool ParseTLV(const std::vector<uint8_t>& data, ExtendedEntity& entity) {
  size_t pos = 0;
  
  while (pos < data.size()) {
    if (pos + 2 > data.size()) {
      return false;  // Not enough data for type and length
    }
    
    uint8_t type = data[pos];
    uint8_t length = data[pos + 1];
    pos += 2;
    
    if (pos + length > data.size()) {
      return false;  // Not enough data for value
    }
    
    std::vector<uint8_t> value(data.begin() + pos, data.begin() + pos + length);
    pos += length;
    
    switch (static_cast<TLVType>(type)) {
      case TLVType::kSpecial:
        // First 32 bytes are the main ID
        if (length >= 32) {
          entity.hex_id = BytesToHex(std::vector<uint8_t>(value.begin(), 
                                                          value.begin() + 32));
          // For naddr, remaining bytes are the identifier
          if (entity.type == EntityType::kNaddr && length > 32) {
            entity.identifier = std::string(value.begin() + 32, value.end());
          }
        }
        break;
        
      case TLVType::kRelay:
        entity.relays.push_back(std::string(value.begin(), value.end()));
        break;
        
      case TLVType::kAuthor:
        if (length == 32) {
          entity.author = BytesToHex(value);
        }
        break;
        
      case TLVType::kKind:
        if (length == 4) {
          entity.kind = (value[0] << 24) | (value[1] << 16) | 
                       (value[2] << 8) | value[3];
        }
        break;
        
      default:
        // Unknown TLV type, skip
        break;
    }
  }
  
  return true;
}

std::optional<DecodeResult> Decode(const std::string& bech32_entity) {
  auto decoded = bech32::Decode(bech32_entity);
  if (!decoded) {
    return std::nullopt;
  }
  
  EntityType type = GetEntityType(decoded->hrp);
  if (type == EntityType::kUnknown) {
    return std::nullopt;
  }
  
  // Convert from 5-bit to 8-bit
  auto data = bech32::ConvertBits(decoded->data, 5, 8, false);
  if (data.empty()) {
    return std::nullopt;
  }
  
  // Simple entities (npub, nsec, note)
  if (type == EntityType::kNpub || type == EntityType::kNsec || 
      type == EntityType::kNote) {
    if (data.size() != 32) {
      return std::nullopt;
    }
    Entity entity;
    entity.type = type;
    entity.hex_id = BytesToHex(data);
    return entity;
  }
  
  // Extended entities with TLV
  ExtendedEntity entity;
  entity.type = type;
  
  if (!ParseTLV(data, entity)) {
    return std::nullopt;
  }
  
  return entity;
}

std::string EncodeNpub(const std::string& hex_pubkey) {
  auto bytes = HexToBytes(hex_pubkey);
  auto data = bech32::ConvertBits(bytes, 8, 5, true);
  return bech32::Encode("npub", data);
}

std::string EncodeNote(const std::string& hex_event_id) {
  auto bytes = HexToBytes(hex_event_id);
  auto data = bech32::ConvertBits(bytes, 8, 5, true);
  return bech32::Encode("note", data);
}

std::string EncodeNprofile(const std::string& hex_pubkey,
                          const std::vector<std::string>& relays) {
  std::vector<uint8_t> tlv_data;
  
  // Add pubkey as special TLV
  auto pubkey_bytes = HexToBytes(hex_pubkey);
  auto pubkey_tlv = EncodeTLV(TLVType::kSpecial, pubkey_bytes);
  tlv_data.insert(tlv_data.end(), pubkey_tlv.begin(), pubkey_tlv.end());
  
  // Add relays
  for (const auto& relay : relays) {
    std::vector<uint8_t> relay_bytes(relay.begin(), relay.end());
    auto relay_tlv = EncodeTLV(TLVType::kRelay, relay_bytes);
    tlv_data.insert(tlv_data.end(), relay_tlv.begin(), relay_tlv.end());
  }
  
  auto data = bech32::ConvertBits(tlv_data, 8, 5, true);
  return bech32::Encode("nprofile", data);
}

std::string EncodeNevent(const std::string& hex_event_id,
                        const std::vector<std::string>& relays,
                        const std::string& author) {
  std::vector<uint8_t> tlv_data;
  
  // Add event ID as special TLV
  auto event_bytes = HexToBytes(hex_event_id);
  auto event_tlv = EncodeTLV(TLVType::kSpecial, event_bytes);
  tlv_data.insert(tlv_data.end(), event_tlv.begin(), event_tlv.end());
  
  // Add relays
  for (const auto& relay : relays) {
    std::vector<uint8_t> relay_bytes(relay.begin(), relay.end());
    auto relay_tlv = EncodeTLV(TLVType::kRelay, relay_bytes);
    tlv_data.insert(tlv_data.end(), relay_tlv.begin(), relay_tlv.end());
  }
  
  // Add author
  if (!author.empty()) {
    auto author_bytes = HexToBytes(author);
    auto author_tlv = EncodeTLV(TLVType::kAuthor, author_bytes);
    tlv_data.insert(tlv_data.end(), author_tlv.begin(), author_tlv.end());
  }
  
  auto data = bech32::ConvertBits(tlv_data, 8, 5, true);
  return bech32::Encode("nevent", data);
}

std::string EncodeNaddr(int kind,
                       const std::string& hex_pubkey,
                       const std::string& identifier,
                       const std::vector<std::string>& relays) {
  std::vector<uint8_t> tlv_data;
  
  // Add pubkey + identifier as special TLV
  std::vector<uint8_t> special_data = HexToBytes(hex_pubkey);
  special_data.insert(special_data.end(), identifier.begin(), identifier.end());
  auto special_tlv = EncodeTLV(TLVType::kSpecial, special_data);
  tlv_data.insert(tlv_data.end(), special_tlv.begin(), special_tlv.end());
  
  // Add relays
  for (const auto& relay : relays) {
    std::vector<uint8_t> relay_bytes(relay.begin(), relay.end());
    auto relay_tlv = EncodeTLV(TLVType::kRelay, relay_bytes);
    tlv_data.insert(tlv_data.end(), relay_tlv.begin(), relay_tlv.end());
  }
  
  // Add kind
  std::vector<uint8_t> kind_bytes = {
    static_cast<uint8_t>(kind >> 24),
    static_cast<uint8_t>(kind >> 16),
    static_cast<uint8_t>(kind >> 8),
    static_cast<uint8_t>(kind)
  };
  auto kind_tlv = EncodeTLV(TLVType::kKind, kind_bytes);
  tlv_data.insert(tlv_data.end(), kind_tlv.begin(), kind_tlv.end());
  
  auto data = bech32::ConvertBits(tlv_data, 8, 5, true);
  return bech32::Encode("naddr", data);
}

}  // namespace nip19
}  // namespace nostr