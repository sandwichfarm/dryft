// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_security_utils.h"

#include <algorithm>
#include <cctype>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "crypto/secure_util.h"

namespace nostr {

// static
bool NsiteSecurityUtils::IsPathSafe(const std::string& path) {
  if (path.empty()) {
    return false;
  }

  // Check for path traversal patterns
  if (HasPathTraversalPatterns(path)) {
    return false;
  }

  // Check for null bytes
  if (path.find('\0') != std::string::npos) {
    return false;
  }

  // Path must start with / or be empty after sanitization
  std::string normalized = SanitizePath(path);
  return !normalized.empty() && normalized[0] == '/';
}

// static
std::string NsiteSecurityUtils::SanitizePath(const std::string& path) {
  if (path.empty()) {
    return "/";
  }

  std::string result = path;

  // Remove null bytes
  result.erase(std::remove(result.begin(), result.end(), '\0'), result.end());

  // Normalize path separators
  std::replace(result.begin(), result.end(), '\\', '/');

  // Remove duplicate slashes
  std::string::size_type pos = 0;
  while ((pos = result.find("//", pos)) != std::string::npos) {
    result.replace(pos, 2, "/");
  }

  // Ensure starts with /
  if (result.empty() || result[0] != '/') {
    result = "/" + result;
  }

  // Remove trailing slash unless it's root
  if (result.length() > 1 && result.back() == '/') {
    result.pop_back();
  }

  return result;
}

// static
bool NsiteSecurityUtils::IsValidNpub(const std::string& npub) {
  // Basic npub format validation
  if (npub.length() != 63 || !base::StartsWith(npub, "npub1")) {
    return false;
  }

  // Check for valid bech32 characters (a-z, 0-9, no mixed case)
  const std::string valid_chars = "abcdefghijklmnopqrstuvwxyz0123456789";
  return ContainsOnlyValidChars(npub.substr(4), valid_chars);
}

// static
bool NsiteSecurityUtils::IsValidSessionId(const std::string& session_id) {
  // Session IDs should be UUIDs (36 chars with hyphens) or similar
  if (session_id.length() < 16 || session_id.length() > 64) {
    return false;
  }

  // Allow alphanumeric and hyphens only
  const std::string valid_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";
  return ContainsOnlyValidChars(session_id, valid_chars);
}

// static
std::string NsiteSecurityUtils::SanitizeInput(const std::string& input, size_t max_length) {
  if (input.empty()) {
    return "";
  }

  std::string result = input;

  // Truncate to max length
  if (result.length() > max_length) {
    result = result.substr(0, max_length);
  }

  // Remove null bytes and control characters
  result.erase(std::remove_if(result.begin(), result.end(), 
                              [](unsigned char c) { return c < 32 && c != '\t' && c != '\n' && c != '\r'; }), 
               result.end());

  return result;
}

// Rate Limiter Implementation
NsiteSecurityUtils::RateLimiter::RateLimiter(int max_requests_per_minute)
    : max_requests_per_minute_(max_requests_per_minute) {
}

NsiteSecurityUtils::RateLimiter::~RateLimiter() = default;

bool NsiteSecurityUtils::RateLimiter::IsAllowed(const std::string& client_id) {
  base::AutoLock lock(lock_);
  
  base::Time now = base::Time::Now();
  auto& client_info = clients_[client_id];
  
  // Reset window if more than a minute has passed
  if (now - client_info.window_start >= base::Minutes(1)) {
    client_info.request_count = 0;
    client_info.window_start = now;
  }
  
  // Check if request is allowed
  if (client_info.request_count >= max_requests_per_minute_) {
    VLOG(2) << "Rate limit exceeded for client: " << client_id;
    return false;
  }
  
  client_info.request_count++;
  return true;
}

void NsiteSecurityUtils::RateLimiter::Cleanup() {
  base::AutoLock lock(lock_);
  
  base::Time cutoff = base::Time::Now() - base::Minutes(5);
  
  auto it = clients_.begin();
  while (it != clients_.end()) {
    if (it->second.window_start < cutoff) {
      it = clients_.erase(it);
    } else {
      ++it;
    }
  }
}

// static
std::string NsiteSecurityUtils::GetSafeErrorMessage(int status_code) {
  // Return generic error messages that don't leak system information
  switch (status_code) {
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 429:
      return "Too Many Requests";
    case 500:
      return "Internal Server Error";
    case 503:
      return "Service Unavailable";
    default:
      return "Unknown Error";
  }
}

// static
bool NsiteSecurityUtils::SecureStringEquals(const std::string& a, const std::string& b) {
  // Use constant-time comparison to prevent timing attacks
  return crypto::SecureMemEqual(a.data(), b.data(), std::max(a.length(), b.length()));
}

// Private helper methods

// static
bool NsiteSecurityUtils::ContainsOnlyValidChars(const std::string& str, const std::string& valid_chars) {
  return std::all_of(str.begin(), str.end(), [&valid_chars](char c) {
    return valid_chars.find(c) != std::string::npos;
  });
}

// static
bool NsiteSecurityUtils::HasPathTraversalPatterns(const std::string& path) {
  // Check for common path traversal patterns
  const std::vector<std::string> dangerous_patterns = {
    "../",
    "..\\",
    ".../",
    "....//",
    "%2e%2e%2f",  // URL encoded ../
    "%2e%2e/",    // Partial URL encoded
    "%2e%2e%5c",  // URL encoded ..\
    "..%2f",      // Partial URL encoded
    "..%5c"       // Partial URL encoded
  };

  std::string lower_path = base::ToLowerASCII(path);
  for (const auto& pattern : dangerous_patterns) {
    if (lower_path.find(pattern) != std::string::npos) {
      return true;
    }
  }

  return false;
}

}  // namespace nostr