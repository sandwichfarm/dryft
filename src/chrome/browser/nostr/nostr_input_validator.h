// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NOSTR_INPUT_VALIDATOR_H_
#define CHROME_BROWSER_NOSTR_NOSTR_INPUT_VALIDATOR_H_

#include <string>
#include <vector>
#include <optional>

#include "base/values.h"
#include "url/gurl.h"

namespace nostr {

// Input validation utility for Nostr protocol operations
// Provides comprehensive validation and sanitization for all inputs
class NostrInputValidator {
 public:
  // Maximum lengths for various fields
  static constexpr size_t kMaxContentLength = 64000;  // ~64KB
  static constexpr size_t kMaxTagLength = 1000;
  static constexpr size_t kMaxRelayUrlLength = 255;
  static constexpr size_t kMaxNameLength = 100;
  static constexpr size_t kMaxNpubLength = 63;  // npub1 + 58 chars
  static constexpr size_t kMaxHexKeyLength = 64;
  static constexpr size_t kMaxEventIdLength = 64;
  static constexpr size_t kMaxSignatureLength = 128;
  
  // Validation methods for Nostr data types
  
  // Validate a hex-encoded public/private key (32 bytes = 64 hex chars)
  static bool IsValidHexKey(const std::string& key);
  
  // Validate an npub-encoded public key (Bech32)
  static bool IsValidNpub(const std::string& npub);
  
  // Validate a hex-encoded event ID (SHA-256 = 64 hex chars)
  static bool IsValidEventId(const std::string& event_id);
  
  // Validate a Schnorr signature (64 bytes = 128 hex chars)
  static bool IsValidSignature(const std::string& signature);
  
  // Validate relay URL
  static bool IsValidRelayUrl(const std::string& url);
  
  // Validate event kind (must be >= 0)
  static bool IsValidEventKind(int kind);
  
  // Validate Unix timestamp
  static bool IsValidTimestamp(int64_t timestamp);
  
  // Validate and sanitize event content
  static std::optional<std::string> SanitizeEventContent(
      const std::string& content,
      int event_kind);
  
  // Validate complete Nostr event structure
  static bool ValidateEvent(const base::Value::Dict& event,
                           std::string* error_message = nullptr);
  
  // Validate event for signing (unsigned event)
  static bool ValidateUnsignedEvent(const base::Value::Dict& event,
                                   std::string* error_message = nullptr);
  
  // Validate event tags array
  static bool ValidateEventTags(const base::Value::List& tags,
                               std::string* error_message = nullptr);
  
  // Sanitize methods
  
  // Sanitize a string by removing control characters and limiting length
  static std::string SanitizeString(const std::string& input,
                                   size_t max_length);
  
  // Sanitize user-facing name
  static std::string SanitizeName(const std::string& name);
  
  // Sanitize URL for display
  static std::string SanitizeUrl(const std::string& url);
  
  // Helper methods
  
  // Check if string contains only hex characters
  static bool IsHexString(const std::string& str);
  
  // Check if string contains control characters
  static bool ContainsControlCharacters(const std::string& str);
  
  // Remove null bytes and other dangerous characters
  static std::string RemoveDangerousCharacters(const std::string& str);
  
  // Validate JSON structure depth (prevent deep nesting attacks)
  static bool ValidateJsonDepth(const base::Value& value,
                               int max_depth = 10,
                               int current_depth = 0);
  
 private:
  NostrInputValidator() = default;
  ~NostrInputValidator() = default;
  
  DISALLOW_COPY_AND_ASSIGN(NostrInputValidator);
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NOSTR_INPUT_VALIDATOR_H_