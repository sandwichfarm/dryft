// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nostr_operation_rate_limiter.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"

namespace nostr {

namespace {

// Default rate limits for different operations
constexpr int kDefaultRequestsPerMinute = 60;
constexpr int kDefaultRequestsPerHour = 1000;

// More restrictive limits for sensitive operations
constexpr int kSignEventRequestsPerMinute = 30;
constexpr int kSignEventRequestsPerHour = 500;

constexpr int kEncryptDecryptRequestsPerMinute = 20;
constexpr int kEncryptDecryptRequestsPerHour = 300;

constexpr int kBlossomUploadRequestsPerMinute = 10;
constexpr int kBlossomUploadRequestsPerHour = 100;

// Cleanup interval (5 minutes)
constexpr base::TimeDelta kCleanupInterval = base::Minutes(5);

// Maximum age for inactive origins (1 hour)
constexpr base::TimeDelta kMaxInactiveAge = base::Hours(1);

}  // namespace

NostrOperationRateLimiter::NostrOperationRateLimiter() {
  // Initialize default configurations
  configs_[OperationType::kGetPublicKey] = {kDefaultRequestsPerMinute, kDefaultRequestsPerHour, true};
  configs_[OperationType::kSignEvent] = {kSignEventRequestsPerMinute, kSignEventRequestsPerHour, true};
  configs_[OperationType::kGetRelays] = {kDefaultRequestsPerMinute, kDefaultRequestsPerHour, true};
  
  configs_[OperationType::kNip04Encrypt] = {kEncryptDecryptRequestsPerMinute, kEncryptDecryptRequestsPerHour, true};
  configs_[OperationType::kNip04Decrypt] = {kEncryptDecryptRequestsPerMinute, kEncryptDecryptRequestsPerHour, true};
  configs_[OperationType::kNip44Encrypt] = {kEncryptDecryptRequestsPerMinute, kEncryptDecryptRequestsPerHour, true};
  configs_[OperationType::kNip44Decrypt] = {kEncryptDecryptRequestsPerMinute, kEncryptDecryptRequestsPerHour, true};
  
  configs_[OperationType::kPermissionRequest] = {10, 100, true};  // Strict limits for permission requests
  configs_[OperationType::kBlossomUpload] = {kBlossomUploadRequestsPerMinute, kBlossomUploadRequestsPerHour, true};
  configs_[OperationType::kBlossomDownload] = {kDefaultRequestsPerMinute * 2, kDefaultRequestsPerHour * 2, true};
  
  configs_[OperationType::kLocalRelayQuery] = {kDefaultRequestsPerMinute, kDefaultRequestsPerHour, true};
  configs_[OperationType::kLocalRelayPublish] = {kSignEventRequestsPerMinute, kSignEventRequestsPerHour, true};

  // Start cleanup timer
  cleanup_timer_.Start(FROM_HERE, kCleanupInterval,
                      base::BindRepeating(&NostrOperationRateLimiter::CleanupOldEntries,
                                        weak_factory_.GetWeakPtr()));
}

NostrOperationRateLimiter::~NostrOperationRateLimiter() = default;

bool NostrOperationRateLimiter::IsAllowed(const GURL& origin, OperationType operation) {
  auto config_it = configs_.find(operation);
  if (config_it == configs_.end() || !config_it->second.enabled) {
    return true;  // No rate limit configured or disabled
  }

  const auto& config = config_it->second;
  auto& origin_data = origin_data_[origin.DeprecatedGetOriginAsURL().spec()];
  auto& op_data = origin_data.operations[operation];

  UpdateTimeWindows(&op_data);

  // Check minute limit
  if (op_data.minute_count >= config.requests_per_minute) {
    LOG(WARNING) << "Rate limit exceeded for origin " << origin 
                 << " operation " << static_cast<int>(operation)
                 << " (minute limit)";
    return false;
  }

  // Check hour limit
  if (op_data.hour_count >= config.requests_per_hour) {
    LOG(WARNING) << "Rate limit exceeded for origin " << origin 
                 << " operation " << static_cast<int>(operation)
                 << " (hour limit)";
    return false;
  }

  return true;
}

void NostrOperationRateLimiter::RecordOperation(const GURL& origin, OperationType operation) {
  auto& origin_data = origin_data_[origin.DeprecatedGetOriginAsURL().spec()];
  auto& op_data = origin_data.operations[operation];

  UpdateTimeWindows(&op_data);

  op_data.minute_count++;
  op_data.hour_count++;
  origin_data.last_access = base::Time::Now();
}

void NostrOperationRateLimiter::SetConfig(OperationType operation, const RateLimitConfig& config) {
  configs_[operation] = config;
}

NostrOperationRateLimiter::RateLimitStatus NostrOperationRateLimiter::GetStatus(
    const GURL& origin, OperationType operation) const {
  RateLimitStatus status;

  auto config_it = configs_.find(operation);
  if (config_it == configs_.end() || !config_it->second.enabled) {
    // No limit
    return status;
  }

  const auto& config = config_it->second;
  auto origin_it = origin_data_.find(origin.DeprecatedGetOriginAsURL().spec());
  
  if (origin_it != origin_data_.end()) {
    auto op_it = origin_it->second.operations.find(operation);
    if (op_it != origin_it->second.operations.end()) {
      auto op_data = op_it->second;  // Copy to allow const method
      const_cast<NostrOperationRateLimiter*>(this)->UpdateTimeWindows(&op_data);
      
      status.requests_this_minute = op_data.minute_count;
      status.requests_this_hour = op_data.hour_count;
      status.reset_minute = op_data.minute_window_start + base::Minutes(1);
      status.reset_hour = op_data.hour_window_start + base::Hours(1);
    }
  }

  status.remaining_this_minute = std::max(0, config.requests_per_minute - status.requests_this_minute);
  status.remaining_this_hour = std::max(0, config.requests_per_hour - status.requests_this_hour);

  return status;
}

void NostrOperationRateLimiter::ClearOrigin(const GURL& origin) {
  origin_data_.erase(origin.DeprecatedGetOriginAsURL().spec());
}

void NostrOperationRateLimiter::ClearAll() {
  origin_data_.clear();
}

void NostrOperationRateLimiter::UpdateTimeWindows(OriginRateLimitData::OperationData* data) {
  base::Time now = base::Time::Now();

  // Initialize windows if needed
  if (data->minute_window_start.is_null()) {
    data->minute_window_start = now;
  }
  if (data->hour_window_start.is_null()) {
    data->hour_window_start = now;
  }

  // Reset minute window if needed
  if (now - data->minute_window_start >= base::Minutes(1)) {
    data->minute_count = 0;
    data->minute_window_start = now;
  }

  // Reset hour window if needed
  if (now - data->hour_window_start >= base::Hours(1)) {
    data->hour_count = 0;
    data->hour_window_start = now;
  }
}

void NostrOperationRateLimiter::CleanupOldEntries() {
  base::Time now = base::Time::Now();
  auto it = origin_data_.begin();
  
  while (it != origin_data_.end()) {
    if (now - it->second.last_access > kMaxInactiveAge) {
      it = origin_data_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace nostr