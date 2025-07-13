// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blossom/blossom_user_server_manager.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/nostr/nostr_event.h"
#include "net/base/url_util.h"

namespace blossom {

namespace {

// Event kind for Blossom server lists as per BUD-03
constexpr int kBlossomServerListKind = 10063;

// Maximum server health score decay per day
constexpr double kHealthDecayPerDay = 0.1;

// Minimum time between server checks
constexpr base::TimeDelta kMinServerCheckInterval = base::Minutes(1);

}  // namespace

BlossomServer::BlossomServer(const GURL& server_url, const std::string& server_name)
    : url(server_url), name(server_name) {}

BlossomServer::~BlossomServer() = default;

double BlossomServer::GetHealthScore() const {
  // Base score depends on consecutive failures
  double base_score = 1.0 / (1.0 + consecutive_failures * 0.5);
  
  // Apply time-based decay for old failures
  base::Time now = base::Time::Now();
  if (!last_failure.is_null()) {
    base::TimeDelta since_failure = now - last_failure;
    double days_since_failure = since_failure.InSecondsF() / (24 * 3600);
    base_score = std::min(1.0, base_score + days_since_failure * kHealthDecayPerDay);
  }
  
  // Boost score for recent successes
  if (!last_success.is_null()) {
    base::TimeDelta since_success = now - last_success;
    if (since_success < base::Hours(1)) {
      base_score = std::min(1.0, base_score + 0.2);
    }
  }
  
  return is_available ? base_score : 0.0;
}

void BlossomServer::MarkFailure() {
  last_failure = base::Time::Now();
  consecutive_failures++;
  
  // Mark as unavailable after too many failures
  if (consecutive_failures >= 5) {
    is_available = false;
  }
}

void BlossomServer::MarkSuccess() {
  last_success = base::Time::Now();
  consecutive_failures = 0;
  is_available = true;
}

BlossomUserServerManager::BlossomUserServerManager(const Config& config)
    : config_(config) {
  DCHECK_GT(config_.max_servers_per_user, 0u);
  DCHECK_GT(config_.max_concurrent_checks, 0u);
  
  // Initialize default server objects
  for (const auto& url : config_.default_servers) {
    default_server_objects_.push_back(
        std::make_unique<BlossomServer>(url, "default"));
  }
  
  // Start periodic cleanup
  cleanup_timer_.Start(FROM_HERE, base::Minutes(10),
                      base::BindRepeating(&BlossomUserServerManager::CleanupCache,
                                         weak_factory_.GetWeakPtr()));
}

BlossomUserServerManager::~BlossomUserServerManager() = default;

void BlossomUserServerManager::GetUserServers(const std::string& pubkey,
                                              ServerListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  // Check cache first
  auto cache_it = server_cache_.find(pubkey);
  if (cache_it != server_cache_.end()) {
    const auto& entry = cache_it->second;
    
    // Check if cached entry is still valid
    if (base::Time::Now() - entry->cached_at < config_.server_list_ttl) {
      // Return cached servers (copy for thread safety)
      std::vector<std::unique_ptr<BlossomServer>> servers;
      for (const auto& server : entry->servers) {
        servers.push_back(std::make_unique<BlossomServer>(*server));
      }
      std::move(callback).Run(servers);
      return;
    }
    
    // Remove expired entry
    server_cache_.erase(cache_it);
  }
  
  // No valid cache entry, try to fetch from relays
  FetchServerListFromRelays(pubkey, std::move(callback));
}

void BlossomUserServerManager::UpdateServerList(
    const std::string& pubkey,
    std::unique_ptr<nostr::NostrEvent> event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!event || event->kind != kBlossomServerListKind) {
    LOG(WARNING) << "Invalid server list event kind: " 
                 << (event ? event->kind : -1);
    return;
  }
  
  // Parse server list from event
  auto servers = ParseServerListEvent(*event);
  if (servers.empty()) {
    LOG(WARNING) << "No valid servers found in event";
    return;
  }
  
  // Limit number of servers
  if (servers.size() > config_.max_servers_per_user) {
    servers.resize(config_.max_servers_per_user);
  }
  
  // Update cache
  auto entry = std::make_unique<ServerListEntry>();
  entry->servers = std::move(servers);
  entry->cached_at = base::Time::Now();
  
  server_cache_[pubkey] = std::move(entry);
  
  LOG(INFO) << "Updated server list for " << pubkey 
            << " with " << entry->servers.size() << " servers";
}

void BlossomUserServerManager::CheckServerHealth(const GURL& server_url,
                                                 ServerCheckCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!IsValidServerUrl(server_url)) {
    std::move(callback).Run(false);
    return;
  }
  
  // TODO: Implement actual HTTP health check
  // For now, we'll simulate a health check
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), true),  // Assume healthy for now
      base::Milliseconds(100));  // Simulate network delay
}

std::vector<BlossomServer*> BlossomUserServerManager::GetBestServers(
    const std::string& pubkey, size_t max_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  std::vector<BlossomServer*> result;
  
  auto cache_it = server_cache_.find(pubkey);
  if (cache_it == server_cache_.end()) {
    // Return default servers if no user servers
    size_t count = std::min(max_count, default_server_objects_.size());
    for (size_t i = 0; i < count; i++) {
      result.push_back(default_server_objects_[i].get());
    }
    return result;
  }
  
  // Get user servers and sort by health score
  auto& servers = cache_it->second->servers;
  std::vector<BlossomServer*> candidates;
  
  for (const auto& server : servers) {
    if (server->is_available) {
      candidates.push_back(server.get());
    }
  }
  
  // Sort by health score (descending)
  std::sort(candidates.begin(), candidates.end(),
           [](const BlossomServer* a, const BlossomServer* b) {
             return a->GetHealthScore() > b->GetHealthScore();
           });
  
  // Return top servers
  size_t count = std::min(max_count, candidates.size());
  result.assign(candidates.begin(), candidates.begin() + count);
  
  return result;
}

void BlossomUserServerManager::ClearCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  server_cache_.clear();
}

std::vector<std::unique_ptr<BlossomServer>> 
BlossomUserServerManager::ParseServerListEvent(const nostr::NostrEvent& event) {
  std::vector<std::unique_ptr<BlossomServer>> servers;
  
  // Parse 'server' tags from the event
  for (const auto& tag : event.tags) {
    if (!tag.is_list() || tag.GetList().empty()) {
      continue;
    }
    
    const auto& tag_list = tag.GetList();
    const std::string* tag_name = tag_list[0].GetIfString();
    if (!tag_name || *tag_name != "server") {
      continue;
    }
    
    // Server tag format: ["server", "https://server.com", "optional_name"]
    if (tag_list.size() < 2) {
      continue;
    }
    
    const std::string* url_str = tag_list[1].GetIfString();
    if (!url_str) {
      continue;
    }
    
    GURL server_url(*url_str);
    if (!IsValidServerUrl(server_url)) {
      LOG(WARNING) << "Invalid server URL: " << *url_str;
      continue;
    }
    
    // Optional server name
    std::string server_name;
    if (tag_list.size() >= 3) {
      if (const std::string* name = tag_list[2].GetIfString()) {
        server_name = *name;
      }
    }
    
    servers.push_back(std::make_unique<BlossomServer>(server_url, server_name));
  }
  
  return servers;
}

bool BlossomUserServerManager::IsValidServerUrl(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }
  
  // Must be HTTPS or HTTP
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  
  // Must have a host
  if (url.host().empty()) {
    return false;
  }
  
  // Reject localhost/loopback unless explicitly allowed
  if (net::IsLocalhost(url)) {
    // For development, we might want to allow localhost
    // return false;
  }
  
  return true;
}

void BlossomUserServerManager::CleanupCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  base::Time now = base::Time::Now();
  
  // Remove expired cache entries
  for (auto it = server_cache_.begin(); it != server_cache_.end();) {
    if (now - it->second->cached_at >= config_.server_list_ttl) {
      it = server_cache_.erase(it);
    } else {
      ++it;
    }
  }
}

void BlossomUserServerManager::FetchServerListFromRelays(
    const std::string& pubkey, ServerListCallback callback) {
  // TODO: Implement relay fetching
  // For now, return default servers or empty list
  
  LOG(INFO) << "Fetching server list for " << pubkey << " from relays (not implemented)";
  
  // Return default servers as fallback
  std::vector<std::unique_ptr<BlossomServer>> servers;
  for (const auto& default_url : config_.default_servers) {
    servers.push_back(std::make_unique<BlossomServer>(default_url, "default"));
  }
  
  std::move(callback).Run(servers);
}

}  // namespace blossom