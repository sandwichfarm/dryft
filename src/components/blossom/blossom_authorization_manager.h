// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOSSOM_BLOSSOM_AUTHORIZATION_MANAGER_H_
#define COMPONENTS_BLOSSOM_BLOSSOM_AUTHORIZATION_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/blossom/authorization_manager.h"

namespace nostr {
struct NostrEvent;
}  // namespace nostr

namespace blossom {

// Authorization event (kind 24242) cache entry
struct AuthorizationEntry {
  std::unique_ptr<nostr::NostrEvent> event;
  base::Time cached_at;
  std::vector<std::string> allowed_verbs;
  std::vector<std::string> allowed_hashes;
  base::Time expiration;
};

// Implementation of AuthorizationManager using Nostr kind 24242 events
class BlossomAuthorizationManager : public AuthorizationManager {
 public:
  // Configuration for the authorization manager
  struct Config {
    base::TimeDelta cache_ttl = base::Hours(1);
    size_t max_cache_size = 1000;
    std::string server_name;  // Expected server name in 'server' tag
    bool require_expiration = true;  // Require expiration tag
  };

  explicit BlossomAuthorizationManager(const Config& config);
  ~BlossomAuthorizationManager() override;

  // AuthorizationManager implementation
  void CheckAuthorization(
      const std::string& auth_header,
      const std::string& verb,
      const std::string& hash,
      base::OnceCallback<void(bool authorized, 
                            const std::string& reason)> callback) override;

 private:
  // Parse authorization header (e.g., "Nostr <base64_event>")
  std::unique_ptr<nostr::NostrEvent> ParseAuthorizationHeader(
      const std::string& auth_header);

  // Parse and validate kind 24242 authorization event
  std::unique_ptr<AuthorizationEntry> ParseAuthorizationEvent(
      std::unique_ptr<nostr::NostrEvent> event);

  // Verify event signature
  bool VerifyEventSignature(const nostr::NostrEvent& event);

  // Calculate event ID and verify it matches
  bool VerifyEventId(const nostr::NostrEvent& event);

  // Check if authorization allows the requested operation
  bool CheckPermission(const AuthorizationEntry& auth,
                      const std::string& verb,
                      const std::string& hash);

  // Clean up expired cache entries
  void CleanupCache();

  // Configuration
  Config config_;

  // Cache of validated authorizations by pubkey
  std::unordered_map<std::string, std::unique_ptr<AuthorizationEntry>> cache_;

  // Periodic cleanup timer
  base::RepeatingTimer cleanup_timer_;

  // Thread checker
  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointer factory
  base::WeakPtrFactory<BlossomAuthorizationManager> weak_factory_{this};
};

}  // namespace blossom

#endif  // COMPONENTS_BLOSSOM_BLOSSOM_AUTHORIZATION_MANAGER_H_