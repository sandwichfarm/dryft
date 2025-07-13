// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/resolver/profile_event_resolver.h"

#include <algorithm>
#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/nostr/local_relay/nostr_database.h"
#include "components/nostr/nostr_event.h"
#include "components/nostr/nostr_filter.h"

namespace nostr {

namespace {

// Default Nostr relays to use when no relay hints are available
const char* kDefaultRelays[] = {
    "wss://relay.damus.io",
    "wss://relay.nostr.band",
    "wss://nos.lol",
    "wss://relay.nostr.bg",
    "wss://nostr.wine",
};

// Event kinds
constexpr int kKindProfileMetadata = 0;
constexpr int kKindTextNote = 1;
constexpr int kKindRecommendedRelay = 2;
constexpr int kKindContactList = 3;
constexpr int kKindParameterizedReplaceable = 30000;

}  // namespace

ProfileEventResolver::ProfileEventResolver(NostrDatabase* database)
    : database_(database) {
  DCHECK(database_);
  
  // Initialize default relays
  for (const char* relay_url : kDefaultRelays) {
    default_relays_.push_back(relay_url);
  }
}

ProfileEventResolver::~ProfileEventResolver() {
  // Cancel all active requests
  for (auto& [request_id, request] : active_requests_) {
    CompleteResolution(request_id, false, "Resolver destroyed");
  }
}

void ProfileEventResolver::ResolveEntity(const std::string& bech32_string,
                                        ResolutionCallback callback) {
  ResolveEntityWithRelays(bech32_string, {}, std::move(callback));
}

void ProfileEventResolver::ResolveEntityWithRelays(
    const std::string& bech32_string,
    const std::vector<std::string>& relay_urls,
    ResolutionCallback callback) {
  DLOG(INFO) << "Resolving entity: " << bech32_string;
  
  // Decode bech32 entity
  auto entity_info = DecodeBech32Entity(bech32_string);
  if (!entity_info) {
    auto result = std::make_unique<ResolutionResult>();
    result->success = false;
    result->error_message = "Invalid bech32 string";
    std::move(callback).Run(std::move(result));
    return;
  }
  
  // Check cache first
  if (config_.cache_results) {
    std::string cache_key = GetCacheKey(*entity_info);
    auto cached_result = GetFromCache(cache_key);
    if (cached_result) {
      DLOG(INFO) << "Returning cached result for: " << bech32_string;
      std::move(callback).Run(std::move(cached_result));
      return;
    }
  }
  
  // Create resolution request
  auto request = std::make_unique<ResolutionRequest>();
  request->request_id = GenerateRequestId();
  request->bech32_string = bech32_string;
  request->entity_info = std::move(*entity_info);
  request->relay_urls = relay_urls;
  request->callback = std::move(callback);
  request->start_time = base::TimeTicks::Now();
  request->result = std::make_unique<ResolutionResult>();
  
  // Start resolution
  std::string request_id = request->request_id;
  active_requests_[request_id] = std::move(request);
  StartResolution(active_requests_[request_id].get());
}

void ProfileEventResolver::ResolveMultipleEntities(
    const std::vector<std::string>& bech32_strings,
    base::OnceCallback<void(std::vector<std::unique_ptr<ResolutionResult>>)> callback) {
  // TODO: Implement batch resolution with optimized relay queries
  // For now, resolve sequentially
  std::vector<std::unique_ptr<ResolutionResult>> results;
  results.reserve(bech32_strings.size());
  
  for (const auto& bech32 : bech32_strings) {
    ResolveEntity(bech32, base::BindOnce(
        [](std::vector<std::unique_ptr<ResolutionResult>>* results,
           std::unique_ptr<ResolutionResult> result) {
          results->push_back(std::move(result));
        },
        &results));
  }
  
  // This is a simplified implementation - should be made async
  std::move(callback).Run(std::move(results));
}

void ProfileEventResolver::SetDefaultRelays(const std::vector<std::string>& relay_urls) {
  default_relays_ = relay_urls;
  DLOG(INFO) << "Updated default relays, count: " << default_relays_.size();
}

void ProfileEventResolver::ClearCache() {
  cache_.clear();
  DLOG(INFO) << "Cleared resolution cache";
}

std::unique_ptr<EntityInfo> ProfileEventResolver::DecodeBech32Entity(
    const std::string& bech32_string) {
  // TODO: Use actual Bech32 decoder from D-2 when available
  // This is a placeholder implementation
  
  auto info = std::make_unique<EntityInfo>();
  info->raw_bech32 = bech32_string;
  
  // Mock implementation for testing
  if (base::StartsWith(bech32_string, "npub1")) {
    info->type = EntityType::PROFILE;
    info->primary_data = "0000000000000000000000000000000000000000000000000000000000000000";
  } else if (base::StartsWith(bech32_string, "note1")) {
    info->type = EntityType::EVENT;
    info->primary_data = "1111111111111111111111111111111111111111111111111111111111111111";
  } else if (base::StartsWith(bech32_string, "nprofile1")) {
    info->type = EntityType::PROFILE;
    info->primary_data = "2222222222222222222222222222222222222222222222222222222222222222";
    info->relay_hints = {"wss://relay.example.com", "wss://relay2.example.com"};
  } else if (base::StartsWith(bech32_string, "nevent1")) {
    info->type = EntityType::EVENT;
    info->primary_data = "3333333333333333333333333333333333333333333333333333333333333333";
    info->relay_hints = {"wss://relay.example.com"};
    info->author = "4444444444444444444444444444444444444444444444444444444444444444";
  } else if (base::StartsWith(bech32_string, "naddr1")) {
    info->type = EntityType::ADDRESS;
    info->primary_data = "5555555555555555555555555555555555555555555555555555555555555555";
    info->kind = "30023";
    info->identifier = "test-article";
  } else {
    return nullptr;
  }
  
  return info;
}

void ProfileEventResolver::StartResolution(ResolutionRequest* request) {
  DCHECK(request);
  
  // Set up timeout timer
  request->timeout_timer = std::make_unique<base::OneShotTimer>();
  request->timeout_timer->Start(
      FROM_HERE, config_.timeout,
      base::BindOnce(&ProfileEventResolver::OnResolutionTimeout,
                     weak_factory_.GetWeakPtr(), request->request_id));
  
  // Prepare relay list
  PrepareRelayList(request);
  
  // Start querying relays
  QueryRelays(request);
}

void ProfileEventResolver::PrepareRelayList(ResolutionRequest* request) {
  // Use provided relays if any
  if (!request->relay_urls.empty()) {
    return;
  }
  
  // Use relay hints from entity
  if (config_.use_relay_hints && !request->entity_info.relay_hints.empty()) {
    request->relay_urls = request->entity_info.relay_hints;
    
    // Limit number of relays
    if (request->relay_urls.size() > static_cast<size_t>(config_.max_relays_per_query)) {
      request->relay_urls.resize(config_.max_relays_per_query);
    }
  }
  
  // Fall back to default relays
  if (request->relay_urls.empty() && config_.fallback_to_default_relays) {
    request->relay_urls = default_relays_;
    
    // Limit number of relays
    if (request->relay_urls.size() > static_cast<size_t>(config_.max_relays_per_query)) {
      request->relay_urls.resize(config_.max_relays_per_query);
    }
  }
  
  DLOG(INFO) << "Using " << request->relay_urls.size() << " relays for resolution";
}

void ProfileEventResolver::QueryRelays(ResolutionRequest* request) {
  if (request->relay_urls.empty()) {
    CompleteResolution(request->request_id, false, "No relays available");
    return;
  }
  
  // Create filters based on entity type
  std::vector<NostrFilter> filters;
  switch (request->entity_info.type) {
    case EntityType::PROFILE:
      filters = CreateFiltersForProfile(request->entity_info);
      break;
    case EntityType::EVENT:
      filters = CreateFiltersForEvent(request->entity_info);
      break;
    case EntityType::ADDRESS:
      filters = CreateFiltersForAddress(request->entity_info);
      break;
  }
  
  // Query each relay
  for (const auto& relay_url : request->relay_urls) {
    auto connection = CreateRelayConnection(relay_url);
    if (!connection) {
      continue;
    }
    
    // Set event callback to receive events as they arrive
    connection->SetEventCallback(base::BindRepeating(
        &ProfileEventResolver::OnEventReceived,
        weak_factory_.GetWeakPtr(), request->request_id));
    
    // Connect and subscribe
    std::string subscription_id = request->request_id + "_" + relay_url;
    connection->Connect(base::BindOnce(
        &ProfileEventResolver::OnRelayConnected,
        weak_factory_.GetWeakPtr(), request->request_id, relay_url));
    
    // Store connection
    request->relay_connections[relay_url] = std::move(connection);
    request->relay_query_complete[relay_url] = false;
  }
}

void ProfileEventResolver::OnRelayConnected(const std::string& request_id,
                                           const std::string& relay_url,
                                           bool success, const std::string& error) {
  auto it = active_requests_.find(request_id);
  if (it == active_requests_.end()) {
    return;  // Request was cancelled
  }
  
  ResolutionRequest* request = it->second.get();
  
  if (!success) {
    DLOG(WARNING) << "Failed to connect to relay " << relay_url << ": " << error;
    request->relay_query_complete[relay_url] = true;
    CheckResolutionComplete(request_id);
    return;
  }
  
  // Create filters
  std::vector<NostrFilter> filters;
  switch (request->entity_info.type) {
    case EntityType::PROFILE:
      filters = CreateFiltersForProfile(request->entity_info);
      break;
    case EntityType::EVENT:
      filters = CreateFiltersForEvent(request->entity_info);
      break;
    case EntityType::ADDRESS:
      filters = CreateFiltersForAddress(request->entity_info);
      break;
  }
  
  // Subscribe to relay
  std::string subscription_id = request_id + "_" + relay_url;
  auto& connection = request->relay_connections[relay_url];
  connection->Subscribe(subscription_id, filters,
      base::BindOnce(&ProfileEventResolver::OnRelayQueryComplete,
                     weak_factory_.GetWeakPtr(), request_id, relay_url));
}

void ProfileEventResolver::OnRelayQueryComplete(const std::string& request_id,
                                               const std::string& relay_url,
                                               std::unique_ptr<QueryResult> query_result) {
  auto it = active_requests_.find(request_id);
  if (it == active_requests_.end()) {
    return;  // Request was cancelled
  }
  
  ResolutionRequest* request = it->second.get();
  
  if (query_result->success) {
    request->result->relays_used.push_back(relay_url);
  }
  
  request->relay_query_complete[relay_url] = true;
  CheckResolutionComplete(request_id);
}

void ProfileEventResolver::OnEventReceived(const std::string& request_id,
                                          std::unique_ptr<NostrEvent> event) {
  auto it = active_requests_.find(request_id);
  if (it == active_requests_.end()) {
    return;  // Request was cancelled
  }
  
  ResolutionRequest* request = it->second.get();
  
  // Store event based on type
  if (request->entity_info.type == EntityType::PROFILE && event->kind() == kKindProfileMetadata) {
    request->result->profile = std::move(event);
  } else if (request->entity_info.type == EntityType::EVENT) {
    request->result->event = std::move(event);
  } else {
    request->result->related_events.push_back(std::move(event));
  }
  
  // Store in database cache if enabled
  if (config_.cache_results && database_) {
    // TODO: Store event in database
  }
}

void ProfileEventResolver::CheckResolutionComplete(const std::string& request_id) {
  auto it = active_requests_.find(request_id);
  if (it == active_requests_.end()) {
    return;  // Request was cancelled
  }
  
  ResolutionRequest* request = it->second.get();
  
  // Check if all queries are complete
  bool all_complete = true;
  for (const auto& [relay_url, complete] : request->relay_query_complete) {
    if (!complete) {
      all_complete = false;
      break;
    }
  }
  
  if (!all_complete) {
    return;  // Still waiting for some relays
  }
  
  // Check if we found what we were looking for
  bool success = false;
  switch (request->entity_info.type) {
    case EntityType::PROFILE:
      success = request->result->profile != nullptr;
      break;
    case EntityType::EVENT:
      success = request->result->event != nullptr;
      break;
    case EntityType::ADDRESS:
      success = !request->result->related_events.empty();
      break;
  }
  
  CompleteResolution(request_id, success, 
                    success ? "" : "Entity not found on any relay");
}

void ProfileEventResolver::CompleteResolution(const std::string& request_id,
                                             bool success,
                                             const std::string& error) {
  auto it = active_requests_.find(request_id);
  if (it == active_requests_.end()) {
    return;  // Already completed
  }
  
  ResolutionRequest* request = it->second.get();
  
  if (request->completed) {
    return;  // Already completed
  }
  
  request->completed = true;
  request->result->success = success;
  request->result->error_message = error;
  request->result->resolution_time = base::TimeTicks::Now() - request->start_time;
  
  // Store in cache if successful
  if (success && config_.cache_results) {
    std::string cache_key = GetCacheKey(request->entity_info);
    StoreInCache(cache_key, *request->result);
  }
  
  // Close all relay connections
  request->relay_connections.clear();
  
  // Complete callback
  std::move(request->callback).Run(std::move(request->result));
  
  // Remove from active requests
  active_requests_.erase(it);
  
  DLOG(INFO) << "Resolution complete for " << request->bech32_string 
             << " - success: " << success;
}

void ProfileEventResolver::OnResolutionTimeout(const std::string& request_id) {
  DLOG(WARNING) << "Resolution timeout for request: " << request_id;
  CompleteResolution(request_id, false, "Resolution timeout");
}

std::string ProfileEventResolver::GetCacheKey(const EntityInfo& entity_info) {
  // Create cache key based on entity type and primary data
  std::string key = base::NumberToString(static_cast<int>(entity_info.type)) + "_" +
                   entity_info.primary_data;
  if (!entity_info.identifier.empty()) {
    key += "_" + entity_info.identifier;
  }
  return key;
}

std::unique_ptr<ResolutionResult> ProfileEventResolver::GetFromCache(
    const std::string& cache_key) {
  auto it = cache_.find(cache_key);
  if (it == cache_.end()) {
    return nullptr;
  }
  
  // Check if cache entry is still valid
  base::TimeDelta age = base::TimeTicks::Now() - it->second->timestamp;
  if (age > config_.cache_ttl) {
    cache_.erase(it);
    return nullptr;
  }
  
  // Return copy of cached result
  auto result = std::make_unique<ResolutionResult>();
  *result = *it->second->result;
  return result;
}

void ProfileEventResolver::StoreInCache(const std::string& cache_key,
                                       const ResolutionResult& result) {
  auto entry = std::make_unique<CacheEntry>();
  entry->result = std::make_unique<ResolutionResult>(result);
  entry->timestamp = base::TimeTicks::Now();
  
  cache_[cache_key] = std::move(entry);
  
  // TODO: Implement cache size limits and eviction
}

std::vector<NostrFilter> ProfileEventResolver::CreateFiltersForProfile(
    const EntityInfo& entity_info) {
  std::vector<NostrFilter> filters;
  
  NostrFilter filter;
  filter.authors = {entity_info.primary_data};
  filter.kinds = {kKindProfileMetadata};
  filter.limit = 1;  // Only need latest profile
  
  filters.push_back(std::move(filter));
  return filters;
}

std::vector<NostrFilter> ProfileEventResolver::CreateFiltersForEvent(
    const EntityInfo& entity_info) {
  std::vector<NostrFilter> filters;
  
  NostrFilter filter;
  filter.ids = {entity_info.primary_data};
  
  filters.push_back(std::move(filter));
  return filters;
}

std::vector<NostrFilter> ProfileEventResolver::CreateFiltersForAddress(
    const EntityInfo& entity_info) {
  std::vector<NostrFilter> filters;
  
  NostrFilter filter;
  filter.authors = {entity_info.primary_data};
  
  // Parse kind if available
  if (!entity_info.kind.empty()) {
    int kind;
    if (base::StringToInt(entity_info.kind, &kind)) {
      filter.kinds = {kind};
    }
  } else {
    // Default to parameterized replaceable event range
    filter.kinds = {kKindParameterizedReplaceable};
  }
  
  // Add #d tag for identifier
  if (!entity_info.identifier.empty()) {
    filter.AddTag("d", entity_info.identifier);
  }
  
  filter.limit = 1;  // Only need latest version
  
  filters.push_back(std::move(filter));
  return filters;
}

std::unique_ptr<RelayConnection> ProfileEventResolver::CreateRelayConnection(
    const std::string& relay_url) {
  GURL url(relay_url);
  if (!url.is_valid() || !url.SchemeIsWSOrWSS()) {
    DLOG(WARNING) << "Invalid relay URL: " << relay_url;
    return nullptr;
  }
  
  auto connection = std::make_unique<RelayConnection>(url);
  connection->SetConnectionTimeout(base::Seconds(10));
  connection->SetQueryTimeout(config_.timeout);
  
  return connection;
}

std::string ProfileEventResolver::GenerateRequestId() {
  return "req_" + base::NumberToString(next_request_id_++);
}

}  // namespace nostr