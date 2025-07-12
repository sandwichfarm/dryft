// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NOSTR_PERMISSION_MANAGER_H_
#define CHROME_BROWSER_NOSTR_NOSTR_PERMISSION_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

class PrefRegistrySimple;

class Profile;

namespace nostr {

// Represents a specific NIP-07 permission for an origin
struct NIP07Permission {
  enum class Policy {
    ASK,      // Prompt user for permission
    ALLOW,    // Automatically allow
    DENY      // Automatically deny
  };

  enum class Method {
    GET_PUBLIC_KEY,
    SIGN_EVENT,
    GET_RELAYS,
    NIP04_ENCRYPT,
    NIP04_DECRYPT
  };

  url::Origin origin;
  Policy default_policy = Policy::ASK;
  std::map<Method, Policy> method_policies;
  std::map<int, Policy> kind_policies;  // For signEvent operations
  base::Time granted_until;
  base::Time last_used;
  
  // Rate limiting counters
  struct RateLimits {
    int requests_per_minute = 60;
    int signs_per_hour = 20;
    int current_requests_count = 0;
    int current_signs_count = 0;
    base::Time request_window_start;
    base::Time sign_window_start;
  };
  RateLimits rate_limits;
  
  // Convert to/from Value for storage
  base::Value::Dict ToValue() const;
  static std::optional<NIP07Permission> FromValue(const base::Value::Dict& dict);
};

// Manages Nostr permissions for NIP-07 operations
class NostrPermissionManager : public KeyedService {
 public:
  // Permission check result
  enum class PermissionResult {
    GRANTED,           // Permission allowed
    DENIED,            // Permission explicitly denied
    ASK_USER,          // Need to prompt user
    RATE_LIMITED,      // Request blocked by rate limiting
    EXPIRED            // Previously granted permission has expired
  };

  // Permission grant result
  enum class GrantResult {
    SUCCESS,           // Permission granted successfully
    INVALID_ORIGIN,    // Origin not valid for permission
    STORAGE_ERROR,     // Failed to store permission
    ALREADY_EXISTS     // Permission already exists with same settings
  };

  explicit NostrPermissionManager(Profile* profile);
  ~NostrPermissionManager() override;

  // Check if an operation is permitted for the given origin
  PermissionResult CheckPermission(const url::Origin& origin,
                                  NIP07Permission::Method method,
                                  int event_kind = -1);

  // Grant permission for an origin with specified settings
  GrantResult GrantPermission(const url::Origin& origin,
                             const NIP07Permission& permission);

  // Revoke permission for an origin (all methods)
  bool RevokePermission(const url::Origin& origin);

  // Revoke specific method permission for an origin
  bool RevokeMethodPermission(const url::Origin& origin,
                             NIP07Permission::Method method);

  // Get all permissions for management UI
  std::vector<NIP07Permission> GetAllPermissions();

  // Get permission for specific origin
  std::optional<NIP07Permission> GetPermission(const url::Origin& origin);

  // Update rate limiting counters for an operation
  bool UpdateRateLimit(const url::Origin& origin,
                      NIP07Permission::Method method);

  // Clear expired permissions
  void CleanupExpiredPermissions();

  // Reset rate limiting counters (for testing)
  void ResetRateLimits(const url::Origin& origin);

  // Register preferences for permission storage
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // KeyedService implementation
  void Shutdown() override;

 private:
  // Load permissions from persistent storage
  void LoadPermissions();

  // Save permissions to persistent storage
  bool SavePermissions();

  // Get permission key for preferences storage
  std::string GetPermissionKey(const url::Origin& origin) const;

  // Check if permission has expired
  bool IsPermissionExpired(const NIP07Permission& permission) const;

  // Update permission last used timestamp
  void UpdateLastUsed(const url::Origin& origin);

  // Check rate limits for an operation
  bool CheckRateLimit(const NIP07Permission& permission,
                     NIP07Permission::Method method) const;

  // Update rate limit counters
  void UpdateRateLimitCounters(NIP07Permission& permission,
                              NIP07Permission::Method method);

  // Profile for storing preferences
  Profile* profile_;

  // In-memory cache of permissions
  std::map<url::Origin, NIP07Permission> permissions_cache_;

  // Whether permissions have been loaded from storage
  bool permissions_loaded_;

  base::WeakPtrFactory<NostrPermissionManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NostrPermissionManager);
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NOSTR_PERMISSION_MANAGER_H_