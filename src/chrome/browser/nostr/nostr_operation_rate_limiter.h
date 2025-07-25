// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NOSTR_OPERATION_RATE_LIMITER_H_
#define CHROME_BROWSER_NOSTR_NOSTR_OPERATION_RATE_LIMITER_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "url/gurl.h"

namespace nostr {

// Provides rate limiting for Nostr operations to prevent abuse
class NostrOperationRateLimiter {
 public:
  // Operation types that can be rate limited
  enum class OperationType {
    kGetPublicKey,
    kSignEvent,
    kGetRelays,
    kNip04Encrypt,
    kNip04Decrypt,
    kNip44Encrypt,
    kNip44Decrypt,
    kPermissionRequest,
    kBlossomUpload,
    kBlossomDownload,
    kLocalRelayQuery,
    kLocalRelayPublish
  };

  // Rate limit configuration
  struct RateLimitConfig {
    int requests_per_minute = 60;
    int requests_per_hour = 1000;
    bool enabled = true;
  };

  NostrOperationRateLimiter();
  ~NostrOperationRateLimiter();

  // Check if an operation is allowed for the given origin
  bool IsAllowed(const GURL& origin, OperationType operation);

  // Record that an operation was performed
  void RecordOperation(const GURL& origin, OperationType operation);

  // Set rate limit configuration for a specific operation type
  void SetConfig(OperationType operation, const RateLimitConfig& config);

  // Get current rate limit status for an origin and operation
  struct RateLimitStatus {
    int requests_this_minute = 0;
    int requests_this_hour = 0;
    int remaining_this_minute = 0;
    int remaining_this_hour = 0;
    base::Time reset_minute;
    base::Time reset_hour;
  };
  RateLimitStatus GetStatus(const GURL& origin, OperationType operation) const;

  // Clear rate limit data for an origin
  void ClearOrigin(const GURL& origin);

  // Clear all rate limit data
  void ClearAll();

 private:
  struct OriginRateLimitData {
    struct OperationData {
      int minute_count = 0;
      int hour_count = 0;
      base::Time minute_window_start;
      base::Time hour_window_start;
    };
    
    std::map<OperationType, OperationData> operations;
    base::Time last_access;
  };

  // Check and update time windows
  void UpdateTimeWindows(OriginRateLimitData::OperationData* data);

  // Clean up old entries periodically
  void CleanupOldEntries();

  // Rate limit configurations per operation type
  std::map<OperationType, RateLimitConfig> configs_;

  // Rate limit data per origin
  std::map<std::string, OriginRateLimitData> origin_data_;

  // Timer for periodic cleanup
  base::RepeatingTimer cleanup_timer_;

  base::WeakPtrFactory<NostrOperationRateLimiter> weak_factory_{this};
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NOSTR_OPERATION_RATE_LIMITER_H_