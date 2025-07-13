// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOSSOM_BLOSSOM_USER_SERVER_MANAGER_H_
#define COMPONENTS_BLOSSOM_BLOSSOM_USER_SERVER_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "url/gurl.h"

namespace nostr {
struct NostrEvent;
}  // namespace nostr

namespace blossom {

// Represents a Blossom server entry from kind 10063 events
struct BlossomServer {
  GURL url;
  std::string name;  // Optional server name
  base::Time last_success;
  base::Time last_failure;
  int consecutive_failures = 0;
  bool is_available = true;
  
  BlossomServer(const GURL& server_url, const std::string& server_name = "");
  ~BlossomServer();
  
  // Calculate server health score (0.0 = unhealthy, 1.0 = perfect)
  double GetHealthScore() const;
  
  // Mark server as failed
  void MarkFailure();
  
  // Mark server as successful
  void MarkSuccess();
};

// Manages user's Blossom server lists from kind 10063 events
class BlossomUserServerManager {
 public:
  // Configuration for server management
  struct Config {
    base::TimeDelta server_list_ttl = base::Hours(1);
    size_t max_servers_per_user = 50;
    size_t max_concurrent_checks = 10;
    base::TimeDelta server_timeout = base::Seconds(30);
    std::vector<GURL> default_servers;  // Fallback servers
  };

  using ServerListCallback = base::OnceCallback<void(
      const std::vector<std::unique_ptr<BlossomServer>>& servers)>;
  using ServerCheckCallback = base::OnceCallback<void(bool available)>;

  explicit BlossomUserServerManager(const Config& config);
  ~BlossomUserServerManager();

  // Get server list for a user (fetches from cache or relays)
  void GetUserServers(const std::string& pubkey, ServerListCallback callback);

  // Update server list from kind 10063 event
  void UpdateServerList(const std::string& pubkey, 
                       std::unique_ptr<nostr::NostrEvent> event);

  // Check server availability
  void CheckServerHealth(const GURL& server_url, ServerCheckCallback callback);

  // Get best available servers for a user (sorted by health score)
  virtual std::vector<BlossomServer*> GetBestServers(const std::string& pubkey,
                                                    size_t max_count = 5);

  // Clear cached server lists
  void ClearCache();

 private:
  // Parse kind 10063 event into server list
  std::vector<std::unique_ptr<BlossomServer>> ParseServerListEvent(
      const nostr::NostrEvent& event);

  // Validate server URL
  bool IsValidServerUrl(const GURL& url);

  // Periodic cache cleanup
  void CleanupCache();

  // Fetch server list from relays (placeholder for future implementation)
  void FetchServerListFromRelays(const std::string& pubkey,
                                ServerListCallback callback);

  // Configuration
  Config config_;

  // Cache of server lists by pubkey
  struct ServerListEntry {
    std::vector<std::unique_ptr<BlossomServer>> servers;
    base::Time cached_at;
  };
  std::map<std::string, std::unique_ptr<ServerListEntry>> server_cache_;

  // Default server objects for fallback
  std::vector<std::unique_ptr<BlossomServer>> default_server_objects_;

  // Periodic cleanup timer
  base::RepeatingTimer cleanup_timer_;

  // Thread checker
  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointer factory
  base::WeakPtrFactory<BlossomUserServerManager> weak_factory_{this};
};

}  // namespace blossom

#endif  // COMPONENTS_BLOSSOM_BLOSSOM_USER_SERVER_MANAGER_H_