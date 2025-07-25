// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/nostr_messages.h"

// NostrPermissionRequest implementation
NostrPermissionRequest::NostrPermissionRequest() = default;

NostrPermissionRequest::NostrPermissionRequest(
    const NostrPermissionRequest& other) = default;

NostrPermissionRequest::~NostrPermissionRequest() = default;

// NostrEvent implementation
NostrEvent::NostrEvent() : created_at(0), kind(0) {}

NostrEvent::NostrEvent(const NostrEvent& other) = default;

NostrEvent::~NostrEvent() = default;

// NostrRelayPolicy implementation
NostrRelayPolicy::NostrRelayPolicy() = default;

NostrRelayPolicy::NostrRelayPolicy(const NostrRelayPolicy& other) = default;

NostrRelayPolicy::~NostrRelayPolicy() = default;

// BlossomUploadResult implementation
BlossomUploadResult::BlossomUploadResult() : size(0) {}

BlossomUploadResult::BlossomUploadResult(const BlossomUploadResult& other) = 
    default;

BlossomUploadResult::~BlossomUploadResult() = default;

// NostrRateLimitInfo implementation
NostrRateLimitInfo::NostrRateLimitInfo()
    : requests_per_minute(0),
      signs_per_hour(0),
      current_count(0) {}

NostrRateLimitInfo::NostrRateLimitInfo(const NostrRateLimitInfo& other) = 
    default;

NostrRateLimitInfo::~NostrRateLimitInfo() = default;