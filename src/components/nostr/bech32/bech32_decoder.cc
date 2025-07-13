// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nostr/bech32/bech32_decoder.h"

#include <algorithm>
#include <array>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/hex_encode.h"
#include "crypto/secure_util.h"

namespace nostr {
namespace bech32 {

namespace {

// Common TLV type constants for Nostr entities
constexpr uint8_t kTlvTypeRelay = 0;
constexpr uint8_t kTlvTypeAuthor = 1;
constexpr uint8_t kTlvTypeKind = 2;
constexpr uint8_t kTlvTypeIdentifier = 3;

}  // namespace

// Static member definitions
const char Bech32Decoder::kBech32Charset[] = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

const uint32_t Bech32Decoder::kBech32Generator[] = {
    0x3b6a57b2, 0x26508e6d, 0x1ea119fa, 0x3d4233dd, 0x2a1462b3
};

const std::map<std::string, NostrEntity::Type> Bech32Decoder::kHrpToType = {
    {"npub", NostrEntity::NPUB},
    {"note", NostrEntity::NOTE},
    {"nprofile", NostrEntity::NPROFILE},
    {"nevent", NostrEntity::NEVENT},
    {"naddr", NostrEntity::NADDR},
    {"nsec", NostrEntity::NSEC},
};

// ComplexEntity helper methods
std::vector<std::string> ComplexEntity::GetRelays() const {
  std::vector<std::string> relays;
  for (const auto& entry : tlv_entries) {
    if (entry.type == kTlvTypeRelay && !entry.value.empty()) {
      std::string relay(entry.value.begin(), entry.value.end());
      relays.push_back(relay);
    }
  }
  return relays;
}

std::string ComplexEntity::GetAuthor() const {
  for (const auto& entry : tlv_entries) {
    if (entry.type == kTlvTypeAuthor && entry.value.size() == 32) {
      // Convert 32 bytes to hex string
      return base::HexEncode(entry.value.data(), entry.value.size());
    }
  }
  return std::string();
}

std::string ComplexEntity::GetKind() const {
  for (const auto& entry : tlv_entries) {
    if (entry.type == kTlvTypeKind && !entry.value.empty()) {
      // Convert bytes to integer
      uint32_t kind = 0;
      for (size_t i = 0; i < std::min(entry.value.size(), size_t(4)); ++i) {
        kind = (kind << 8) | entry.value[i];
      }
      return base::NumberToString(kind);
    }
  }
  return std::string();
}

std::string ComplexEntity::GetIdentifier() const {
  for (const auto& entry : tlv_entries) {
    if (entry.type == kTlvTypeIdentifier && !entry.value.empty()) {
      return std::string(entry.value.begin(), entry.value.end());
    }
  }
  return std::string();
}

// Bech32Decoder implementation
Bech32Decoder::Bech32Decoder() = default;
Bech32Decoder::~Bech32Decoder() = default;

base::expected<std::unique_ptr<NostrEntity>, DecodeError> 
Bech32Decoder::DecodeNostrEntity(const std::string& bech32_str) {
  // First decode the bech32 string
  auto decode_result = DecodeBech32(bech32_str);
  if (!decode_result.has_value()) {
    return base::unexpected(decode_result.error());
  }
  
  const std::string& hrp = decode_result->first;
  const std::vector<uint8_t>& data = decode_result->second;
  
  // Map HRP to entity type
  auto type_it = kHrpToType.find(hrp);
  if (type_it == kHrpToType.end()) {
    return base::unexpected(DecodeError::UNKNOWN_HRP);
  }
  
  NostrEntity::Type entity_type = type_it->second;
  
  // Block private keys for security
  if (entity_type == NostrEntity::NSEC) {
    LOG(WARNING) << "Attempted to decode nsec (private key) - blocking for security";
    return base::unexpected(DecodeError::UNSUPPORTED_ENTITY);
  }
  
  // Decode based on entity type
  switch (entity_type) {
    case NostrEntity::NPUB:
    case NostrEntity::NOTE:
      return DecodeSimpleEntity(entity_type, bech32_str, data);
      
    case NostrEntity::NPROFILE:
    case NostrEntity::NEVENT:
    case NostrEntity::NADDR:
      return DecodeComplexEntity(entity_type, bech32_str, data);
      
    default:
      return base::unexpected(DecodeError::UNSUPPORTED_ENTITY);
  }
}

base::expected<std::pair<std::string, std::vector<uint8_t>>, DecodeError>
Bech32Decoder::DecodeBech32(const std::string& bech32_str) {
  // Convert to lowercase and validate characters
  std::string lower_str = base::ToLowerASCII(bech32_str);
  
  // Find the separator
  size_t separator_pos = lower_str.rfind('1');
  if (separator_pos == std::string::npos || separator_pos == 0 || 
      separator_pos + 7 > lower_str.length()) {
    return base::unexpected(DecodeError::INVALID_LENGTH);
  }
  
  std::string hrp = lower_str.substr(0, separator_pos);
  std::string data_part = lower_str.substr(separator_pos + 1);
  
  // Validate and convert data part
  std::vector<uint8_t> data;
  data.reserve(data_part.length());
  
  for (char c : data_part) {
    int32_t value = CharToInt(c);
    if (value < 0) {
      return base::unexpected(DecodeError::INVALID_CHARACTER);
    }
    data.push_back(static_cast<uint8_t>(value));
  }
  
  // Verify checksum
  if (!VerifyChecksum(hrp, data)) {
    return base::unexpected(DecodeError::INVALID_CHECKSUM);
  }
  
  // Remove checksum (last 6 characters) and convert from 5-bit to 8-bit
  data.resize(data.size() - 6);
  std::vector<uint8_t> decoded_data = ConvertBits(data, 5, 8, false);
  
  if (decoded_data.empty()) {
    return base::unexpected(DecodeError::MALFORMED_DATA);
  }
  
  return std::make_pair(hrp, decoded_data);
}

base::expected<std::string, DecodeError>
Bech32Decoder::EncodeBech32(const std::string& hrp, const std::vector<uint8_t>& data) {
  // Convert from 8-bit to 5-bit
  std::vector<uint8_t> converted_data = ConvertBits(data, 8, 5, true);
  if (converted_data.empty()) {
    return base::unexpected(DecodeError::MALFORMED_DATA);
  }
  
  // Compute checksum (we need to XOR with 1 to get the correct encoding checksum)
  uint32_t checksum = ComputeChecksum(hrp, converted_data) ^ 1;
  
  // Append checksum
  for (int i = 0; i < 6; ++i) {
    converted_data.push_back((checksum >> (5 * (5 - i))) & 31);
  }
  
  // Build result string
  std::string result = hrp + "1";
  for (uint8_t value : converted_data) {
    result += IntToChar(value);
  }
  
  return result;
}

std::string Bech32Decoder::GetErrorMessage(DecodeError error) {
  switch (error) {
    case DecodeError::INVALID_CHARACTER:
      return "Invalid character in bech32 string";
    case DecodeError::INVALID_CHECKSUM:
      return "Invalid bech32 checksum";
    case DecodeError::INVALID_LENGTH:
      return "Invalid bech32 string length";
    case DecodeError::UNKNOWN_HRP:
      return "Unknown human-readable part";
    case DecodeError::MALFORMED_DATA:
      return "Malformed data in bech32 string";
    case DecodeError::INVALID_TLV:
      return "Invalid TLV data";
    case DecodeError::UNSUPPORTED_ENTITY:
      return "Unsupported entity type";
    default:
      return "Unknown error";
  }
}

bool Bech32Decoder::IsValidNostrEntity(const std::string& bech32_str) {
  Bech32Decoder decoder;
  auto result = decoder.DecodeNostrEntity(bech32_str);
  return result.has_value();
}

// Private methods

bool Bech32Decoder::VerifyChecksum(const std::string& hrp, const std::vector<uint8_t>& data) {
  uint32_t computed = ComputeChecksum(hrp, data);
  return computed == 1;  // Valid Bech32 checksum should equal 1
}

uint32_t Bech32Decoder::ComputeChecksum(const std::string& hrp, const std::vector<uint8_t>& data) {
  uint32_t chk = 1;
  
  // Process HRP - high bits first
  for (char c : hrp) {
    chk ^= (static_cast<uint32_t>(c) >> 5);
    for (int i = 0; i < 5; ++i) {
      chk = (chk << 1) ^ ((chk >> 25) ? 0x3b6a57b2 : 0);
    }
  }
  
  // Separator
  for (int i = 0; i < 5; ++i) {
    chk = (chk << 1) ^ ((chk >> 25) ? 0x3b6a57b2 : 0);
  }
  
  // Process HRP - low bits
  for (char c : hrp) {
    chk ^= (static_cast<uint32_t>(c) & 0x1f);
    for (int i = 0; i < 5; ++i) {
      chk = (chk << 1) ^ ((chk >> 25) ? 0x3b6a57b2 : 0);
    }
  }
  
  // Process data
  for (uint8_t value : data) {
    chk ^= value;
    for (int i = 0; i < 5; ++i) {
      chk = (chk << 1) ^ ((chk >> 25) ? 0x3b6a57b2 : 0);
    }
  }
  
  return chk;
}

std::vector<uint8_t> Bech32Decoder::ConvertBits(const std::vector<uint8_t>& data, 
                                               int from_bits, int to_bits, bool pad) {
  std::vector<uint8_t> result;
  int acc = 0;
  int bits = 0;
  
  const int max_acc = (1 << (from_bits + to_bits - 1)) - 1;
  const int max_v = (1 << to_bits) - 1;
  
  for (uint8_t value : data) {
    if (value >> from_bits) {
      return {};  // Invalid input
    }
    
    acc = (acc << from_bits) | value;
    bits += from_bits;
    
    while (bits >= to_bits) {
      bits -= to_bits;
      result.push_back((acc >> bits) & max_v);
    }
  }
  
  if (pad) {
    if (bits) {
      result.push_back((acc << (to_bits - bits)) & max_v);
    }
  } else if (bits >= from_bits || ((acc << (to_bits - bits)) & max_v)) {
    return {};  // Invalid padding
  }
  
  return result;
}

int32_t Bech32Decoder::CharToInt(char c) {
  const char* pos = std::find(kBech32Charset, kBech32Charset + 32, c);
  if (pos == kBech32Charset + 32) {
    return -1;
  }
  return pos - kBech32Charset;
}

char Bech32Decoder::IntToChar(int32_t i) {
  if (i < 0 || i >= 32) {
    return '?';
  }
  return kBech32Charset[i];
}

base::expected<std::vector<TlvEntry>, DecodeError>
Bech32Decoder::ParseTlv(const std::vector<uint8_t>& data, size_t offset) {
  std::vector<TlvEntry> entries;
  
  while (offset < data.size()) {
    if (offset + 1 >= data.size()) {
      return base::unexpected(DecodeError::INVALID_TLV);
    }
    
    uint8_t type = data[offset++];
    uint8_t length = data[offset++];
    
    if (offset + length > data.size()) {
      return base::unexpected(DecodeError::INVALID_TLV);
    }
    
    TlvEntry entry;
    entry.type = type;
    entry.value.assign(data.begin() + offset, data.begin() + offset + length);
    entries.push_back(std::move(entry));
    
    offset += length;
  }
  
  return entries;
}

base::expected<std::unique_ptr<NostrEntity>, DecodeError>
Bech32Decoder::DecodeSimpleEntity(
    NostrEntity::Type type, const std::string& raw_data, 
    const std::vector<uint8_t>& data) {
  
  // npub and note should be exactly 32 bytes
  if (data.size() != 32) {
    return base::unexpected(DecodeError::MALFORMED_DATA);
  }
  
  return std::make_unique<SimpleEntity>(type, raw_data, data);
}

base::expected<std::unique_ptr<NostrEntity>, DecodeError> 
Bech32Decoder::DecodeComplexEntity(
    NostrEntity::Type type, const std::string& raw_data,
    const std::vector<uint8_t>& data) {
  
  // Complex entities start with 32 bytes of primary data (pubkey or event ID)
  if (data.size() < 32) {
    return base::unexpected(DecodeError::MALFORMED_DATA);
  }
  
  std::vector<uint8_t> primary_data(data.begin(), data.begin() + 32);
  
  // Parse TLV data if present
  std::vector<TlvEntry> tlv_entries;
  if (data.size() > 32) {
    auto tlv_result = ParseTlv(data, 32);
    if (!tlv_result.has_value()) {
      return base::unexpected(tlv_result.error());
    }
    tlv_entries = std::move(tlv_result.value());
  }
  
  return std::make_unique<ComplexEntity>(type, raw_data, 
                                        std::move(primary_data), 
                                        std::move(tlv_entries));
}

}  // namespace bech32
}  // namespace nostr