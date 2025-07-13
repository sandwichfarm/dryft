// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NSITE_NSITE_SECURITY_UTILS_H_
#define CHROME_BROWSER_NOSTR_NSITE_NSITE_SECURITY_UTILS_H_

#include <string>

#include "base/time/time.h"

namespace nostr {

// Security utilities for nsite streaming server operations.
// Provides path validation, input sanitization, and rate limiting.
class NsiteSecurityUtils {
 public:
  // Path traversal prevention
  static bool IsPathSafe(const std::string& path);
  static std::string SanitizePath(const std::string& path);

  // Input validation
  static bool IsValidNpub(const std::string& npub);
  static bool IsValidSessionId(const std::string& session_id);
  static std::string SanitizeInput(const std::string& input, size_t max_length = 1024);

  // Rate limiting support
  class RateLimiter {
   public:
    explicit RateLimiter(int max_requests_per_minute = 60);
    ~RateLimiter();

    // Check if request is allowed for given client ID
    bool IsAllowed(const std::string& client_id);

    // Clear old entries (called periodically)
    void Cleanup();

   private:
    struct ClientInfo {
      int request_count = 0;
      base::Time window_start;
    };

    const int max_requests_per_minute_;
    std::map<std::string, ClientInfo> clients_;
    base::Lock lock_;
  };

  // Secure error responses (no information leakage)
  static std::string GetSafeErrorMessage(int status_code);

  // Constant-time string comparison for sensitive data
  static bool SecureStringEquals(const std::string& a, const std::string& b);

 private:
  // Internal validation helpers
  static bool ContainsOnlyValidChars(const std::string& str, const std::string& valid_chars);
  static bool HasPathTraversalPatterns(const std::string& path);
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NSITE_NSITE_SECURITY_UTILS_H_