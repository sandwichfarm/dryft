// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nostr_input_validator.h"

#include <algorithm>
#include <cctype>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/url_constants.h"

namespace nostr {

namespace {

// Allowed URL schemes for relays
constexpr const char* kAllowedRelaySchemes[] = {
    url::kWsScheme,
    url::kWssScheme
};

// Check if a character is a valid hex digit
bool IsHexDigit(char c) {
  return (c >= '0' && c <= '9') ||
         (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

// Check if a character is a control character
bool IsControlCharacter(char c) {
  return (c >= 0 && c < 32) || c == 127;
}

}  // namespace

// static
bool NostrInputValidator::IsValidHexKey(const std::string& key) {
  if (key.length() != kMaxHexKeyLength) {
    return false;
  }
  
  return IsHexString(key);
}

// static
bool NostrInputValidator::IsValidNpub(const std::string& npub) {
  if (npub.empty() || npub.length() > kMaxNpubLength) {
    return false;
  }
  
  // Must start with "npub1"
  if (!base::StartsWith(npub, "npub1", base::CompareCase::SENSITIVE)) {
    return false;
  }
  
  // Validate Bech32 characters (alphanumeric except 1, b, i, o)
  for (size_t i = 5; i < npub.length(); ++i) {
    char c = npub[i];
    if (!std::isalnum(c) || (c == '1' || c == 'b' || c == 'i' || c == 'o')) {
      return false;
    }
  }
  
  return true;
}

// static
bool NostrInputValidator::IsValidEventId(const std::string& event_id) {
  return IsValidHexKey(event_id);  // Event IDs are 32-byte hex strings
}

// static
bool NostrInputValidator::IsValidSignature(const std::string& signature) {
  if (signature.length() != kMaxSignatureLength) {
    return false;
  }
  
  return IsHexString(signature);
}

// static
bool NostrInputValidator::IsValidRelayUrl(const std::string& url) {
  if (url.empty() || url.length() > kMaxRelayUrlLength) {
    return false;
  }
  
  GURL gurl(url);
  if (!gurl.is_valid()) {
    return false;
  }
  
  // Check scheme
  bool valid_scheme = false;
  for (const char* scheme : kAllowedRelaySchemes) {
    if (gurl.scheme() == scheme) {
      valid_scheme = true;
      break;
    }
  }
  
  if (!valid_scheme) {
    return false;
  }
  
  // Must have a host
  if (gurl.host().empty()) {
    return false;
  }
  
  // Check for local/private addresses (security)
  if (net::IsLocalhost(gurl)) {
    // Allow localhost only for local relay (ws://127.0.0.1:8081)
    return gurl.port() == "8081";
  }
  
  // Reject private IP ranges
  if (gurl.HostIsIPAddress()) {
    net::IPAddress ip_address;
    if (ip_address.AssignFromIPLiteral(gurl.host()) && ip_address.IsReserved()) {
      return false;  // Reject private or reserved IP ranges
    }
  }
  
  return true;
}

// static
bool NostrInputValidator::IsValidEventKind(int kind) {
  return kind >= 0 && kind <= 65535;  // Reasonable upper limit
}

// static
bool NostrInputValidator::IsValidTimestamp(int64_t timestamp) {
  // Check if timestamp is reasonable (between 2020 and 2100)
  base::Time min_time = base::Time::FromTimeT(1577836800);  // Jan 1, 2020
  base::Time max_time = base::Time::FromTimeT(4102444800);  // Jan 1, 2100
  base::Time event_time = base::Time::FromTimeT(timestamp);
  
  return event_time >= min_time && event_time <= max_time;
}

// static
std::optional<std::string> NostrInputValidator::SanitizeEventContent(
    const std::string& content,
    int event_kind) {
  if (content.length() > kMaxContentLength) {
    return std::nullopt;
  }
  
  // Remove dangerous characters
  std::string sanitized = RemoveDangerousCharacters(content);
  
  // For certain event kinds, apply additional validation
  switch (event_kind) {
    case 0:  // Metadata - should be valid JSON
    case 3:  // Contact list - should be valid JSON
      {
        auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
            sanitized, base::JSONParserOptions::JSON_PARSE_RFC);
        if (!parsed_json.has_value()) {
          return std::nullopt;  // Invalid JSON
        }
      }
      break;
    default:
      // For other kinds, just ensure no control characters
      break;
  }
  
  return sanitized;
}

// static
bool NostrInputValidator::ValidateEvent(const base::Value::Dict& event,
                                       std::string* error_message) {
  // Check required fields
  const std::string* id = event.FindString("id");
  const std::string* pubkey = event.FindString("pubkey");
  const std::string* content = event.FindString("content");
  const std::string* sig = event.FindString("sig");
  auto created_at = event.FindInt("created_at");
  auto kind = event.FindInt("kind");
  const base::Value::List* tags = event.FindList("tags");
  
  if (!id || !pubkey || !content || !sig || !created_at || !kind || !tags) {
    if (error_message) {
      *error_message = "Event missing required fields";
    }
    return false;
  }
  
  // Validate ID
  if (!IsValidEventId(*id)) {
    if (error_message) {
      *error_message = "Invalid event ID format";
    }
    return false;
  }
  
  // Validate pubkey
  if (!IsValidHexKey(*pubkey)) {
    if (error_message) {
      *error_message = "Invalid public key format";
    }
    return false;
  }
  
  // Validate signature
  if (!IsValidSignature(*sig)) {
    if (error_message) {
      *error_message = "Invalid signature format";
    }
    return false;
  }
  
  // Validate timestamp
  if (!IsValidTimestamp(*created_at)) {
    if (error_message) {
      *error_message = "Invalid timestamp";
    }
    return false;
  }
  
  // Validate kind
  if (!IsValidEventKind(*kind)) {
    if (error_message) {
      *error_message = "Invalid event kind";
    }
    return false;
  }
  
  // Validate content
  auto sanitized_content = SanitizeEventContent(*content, *kind);
  if (!sanitized_content) {
    if (error_message) {
      *error_message = "Invalid content";
    }
    return false;
  }
  
  // Validate tags
  if (!ValidateEventTags(*tags, error_message)) {
    return false;
  }
  
  // Validate JSON depth
  if (!ValidateJsonDepth(event)) {
    if (error_message) {
      *error_message = "Event structure too deeply nested";
    }
    return false;
  }
  
  return true;
}

// static
bool NostrInputValidator::ValidateUnsignedEvent(const base::Value::Dict& event,
                                               std::string* error_message) {
  // For unsigned events, we don't check id and sig
  const std::string* content = event.FindString("content");
  auto kind = event.FindInt("kind");
  const base::Value::List* tags = event.FindList("tags");
  
  if (!content || !kind || !tags) {
    if (error_message) {
      *error_message = "Event missing required fields";
    }
    return false;
  }
  
  // created_at is optional for unsigned events
  auto created_at = event.FindInt("created_at");
  if (created_at && !IsValidTimestamp(*created_at)) {
    if (error_message) {
      *error_message = "Invalid timestamp";
    }
    return false;
  }
  
  // Validate kind
  if (!IsValidEventKind(*kind)) {
    if (error_message) {
      *error_message = "Invalid event kind";
    }
    return false;
  }
  
  // Validate content
  auto sanitized_content = SanitizeEventContent(*content, *kind);
  if (!sanitized_content) {
    if (error_message) {
      *error_message = "Invalid content";
    }
    return false;
  }
  
  // Validate tags
  if (!ValidateEventTags(*tags, error_message)) {
    return false;
  }
  
  // Validate JSON depth
  if (!ValidateJsonDepth(event)) {
    if (error_message) {
      *error_message = "Event structure too deeply nested";
    }
    return false;
  }
  
  return true;
}

// static
bool NostrInputValidator::ValidateEventTags(const base::Value::List& tags,
                                           std::string* error_message) {
  for (const auto& tag_value : tags) {
    const base::Value::List* tag = tag_value.GetIfList();
    if (!tag || tag->empty()) {
      if (error_message) {
        *error_message = "Invalid tag format";
      }
      return false;
    }
    
    // Each tag element should be a string
    size_t total_length = 0;
    for (const auto& element : *tag) {
      const std::string* str = element.GetIfString();
      if (!str) {
        if (error_message) {
          *error_message = "Tag contains non-string element";
        }
        return false;
      }
      
      total_length += str->length();
      
      // Check for dangerous characters in tag values
      if (ContainsControlCharacters(*str)) {
        if (error_message) {
          *error_message = "Tag contains control characters";
        }
        return false;
      }
    }
    
    // Check total tag length
    if (total_length > kMaxTagLength) {
      if (error_message) {
        *error_message = "Tag too long";
      }
      return false;
    }
  }
  
  return true;
}

// static
std::string NostrInputValidator::SanitizeString(const std::string& input,
                                               size_t max_length) {
  std::string sanitized = RemoveDangerousCharacters(input);
  
  // Truncate if necessary
  if (sanitized.length() > max_length) {
    sanitized = sanitized.substr(0, max_length);
  }
  
  return sanitized;
}

// static
std::string NostrInputValidator::SanitizeName(const std::string& name) {
  return SanitizeString(name, kMaxNameLength);
}

// static
std::string NostrInputValidator::SanitizeUrl(const std::string& url) {
  // Basic URL sanitization
  std::string sanitized = SanitizeString(url, kMaxRelayUrlLength);
  
  // Ensure it's a valid URL
  GURL gurl(sanitized);
  if (!gurl.is_valid()) {
    return "";
  }
  
  return gurl.spec();
}

// static
bool NostrInputValidator::IsHexString(const std::string& str) {
  if (str.empty()) {
    return false;
  }
  
  for (char c : str) {
    if (!IsHexDigit(c)) {
      return false;
    }
  }
  
  return true;
}

// static
bool NostrInputValidator::ContainsControlCharacters(const std::string& str) {
  for (char c : str) {
    if (IsControlCharacter(c)) {
      return true;
    }
  }
  return false;
}

// static
std::string NostrInputValidator::RemoveDangerousCharacters(const std::string& str) {
  std::string result;
  result.reserve(str.length());
  
  for (char c : str) {
    // Skip null bytes and control characters (except newline and tab)
    if (c == '\0' || (IsControlCharacter(c) && c != '\n' && c != '\t')) {
      continue;
    }
    result.push_back(c);
  }
  
  return result;
}

// static
bool NostrInputValidator::ValidateJsonDepth(const base::Value& value,
                                           int max_depth,
                                           int current_depth) {
  if (current_depth >= max_depth) {
    return false;
  }
  
  if (value.is_dict()) {
    for (const auto& [key, val] : value.GetDict()) {
      if (!ValidateJsonDepth(val, max_depth, current_depth + 1)) {
        return false;
      }
    }
  } else if (value.is_list()) {
    for (const auto& val : value.GetList()) {
      if (!ValidateJsonDepth(val, max_depth, current_depth + 1)) {
        return false;
      }
    }
  }
  
  return true;
}

}  // namespace nostr