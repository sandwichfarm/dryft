// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_RESOLVER_PROFILE_EVENT_RESOLVER_H_
#define CHROME_BROWSER_NOSTR_RESOLVER_PROFILE_EVENT_RESOLVER_H_

#include <memory>
#include <string>
#include <vector>
#include <map>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/nostr/relay_client/relay_connection.h"
#include "components/nostr/nostr_event.h"
#include "components/nostr/nostr_filter.h"
#include "url/gurl.h"

namespace nostr {

// Forward declarations
class NostrDatabase;

// Types of entities that can be resolved
enum class EntityType {
  PROFILE,    // npub, nprofile
  EVENT,      // note, nevent
  ADDRESS,    // naddr (parameterized replaceable event)
};

// Bech32 entity information (placeholder until D-2 is merged)
struct EntityInfo {
  EntityType type;
  std::string primary_data;  // hex-encoded pubkey or event ID
  std::vector<std::string> relay_hints;
  std::string author;        // for nevent
  std::string kind;          // for naddr
  std::string identifier;    // for naddr
  std::string raw_bech32;    // original bech32 string
};

// Result of a resolution operation
struct ResolutionResult {
  bool success = false;
  std::string error_message;
  std::unique_ptr<NostrEvent> event;     // For event resolution
  std::unique_ptr<NostrEvent> profile;   // For profile resolution (kind 0)
  std::vector<std::unique_ptr<NostrEvent>> related_events;  // Additional context
  std::vector<std::string> relays_used;
  base::TimeDelta resolution_time;
};

// Configuration for resolution behavior
struct ResolverConfig {
  base::TimeDelta timeout = base::Seconds(30);
  int max_concurrent_queries = 5;
  int max_relays_per_query = 3;
  bool cache_results = true;
  base::TimeDelta cache_ttl = base::Hours(1);
  bool use_relay_hints = true;
  bool fallback_to_default_relays = true;
};

// Main service for resolving Nostr entities from bech32 strings
class ProfileEventResolver {
 public:
  using ResolutionCallback = base::OnceCallback<void(std::unique_ptr<ResolutionResult>)>;
  
  explicit ProfileEventResolver(NostrDatabase* database);
  ~ProfileEventResolver();

  // Main resolution API
  void ResolveEntity(const std::string& bech32_string, 
                     ResolutionCallback callback);
  void ResolveEntityWithRelays(const std::string& bech32_string,
                              const std::vector<std::string>& relay_urls,
                              ResolutionCallback callback);
  
  // Batch resolution
  void ResolveMultipleEntities(const std::vector<std::string>& bech32_strings,
                              base::OnceCallback<void(std::vector<std::unique_ptr<ResolutionResult>>)> callback);
  
  // Configuration
  void SetConfig(const ResolverConfig& config) { config_ = config; }
  const ResolverConfig& config() const { return config_; }
  
  // Default relays management
  void SetDefaultRelays(const std::vector<std::string>& relay_urls);
  const std::vector<std::string>& default_relays() const { return default_relays_; }
  
  // Cache management
  void ClearCache();
  void SetCacheEnabled(bool enabled) { config_.cache_results = enabled; }
  
 private:
  // Internal resolution state
  struct ResolutionRequest {
    std::string request_id;
    std::string bech32_string;
    EntityInfo entity_info;
    std::vector<std::string> relay_urls;
    ResolutionCallback callback;
    base::TimeTicks start_time;
    std::unique_ptr<base::OneShotTimer> timeout_timer;
    
    // Query state
    std::map<std::string, std::unique_ptr<RelayConnection>> relay_connections;
    std::map<std::string, bool> relay_query_complete;
    std::unique_ptr<ResolutionResult> result;
    bool completed = false;
  };
  
  // Bech32 decoding (placeholder until D-2 is merged)
  std::unique_ptr<EntityInfo> DecodeBech32Entity(const std::string& bech32_string);
  
  // Resolution pipeline
  void StartResolution(std::unique_ptr<ResolutionRequest> request);
  void PrepareRelayList(ResolutionRequest* request);
  void QueryRelays(ResolutionRequest* request);
  void OnRelayQueryComplete(const std::string& request_id, 
                           const std::string& relay_url,
                           std::unique_ptr<QueryResult> query_result);
  void OnEventReceived(const std::string& request_id,
                      std::unique_ptr<NostrEvent> event);
  void CheckResolutionComplete(const std::string& request_id);
  void CompleteResolution(const std::string& request_id, bool success, const std::string& error = "");
  
  // Cache operations
  std::string GetCacheKey(const EntityInfo& entity_info);
  std::unique_ptr<ResolutionResult> GetFromCache(const std::string& cache_key);
  void StoreInCache(const std::string& cache_key, const ResolutionResult& result);
  
  // Filter creation for different entity types
  std::vector<NostrFilter> CreateFiltersForProfile(const EntityInfo& entity_info);
  std::vector<NostrFilter> CreateFiltersForEvent(const EntityInfo& entity_info);
  std::vector<NostrFilter> CreateFiltersForAddress(const EntityInfo& entity_info);
  
  // Relay connection management
  std::unique_ptr<RelayConnection> CreateRelayConnection(const std::string& relay_url);
  void OnRelayConnected(const std::string& request_id,
                       const std::string& relay_url,
                       bool success, const std::string& error);
  
  // Timeout handling
  void OnResolutionTimeout(const std::string& request_id);
  
  NostrDatabase* database_;  // Not owned
  ResolverConfig config_;
  std::vector<std::string> default_relays_;
  
  // Active resolution requests
  std::map<std::string, std::unique_ptr<ResolutionRequest>> active_requests_;
  
  // Cache storage
  struct CacheEntry {
    std::unique_ptr<ResolutionResult> result;
    base::TimeTicks timestamp;
  };
  std::map<std::string, std::unique_ptr<CacheEntry>> cache_;
  
  // Request ID generation
  int next_request_id_ = 1;
  std::string GenerateRequestId();
  
  base::WeakPtrFactory<ProfileEventResolver> weak_factory_{this};
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_RESOLVER_PROFILE_EVENT_RESOLVER_H_