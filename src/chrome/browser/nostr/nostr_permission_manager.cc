// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nostr_permission_manager.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace nostr {

namespace {

// Preference keys for permission storage
constexpr char kNostrPermissionsKey[] = "dryft.permissions.nip07";
constexpr char kDefaultPolicyKey[] = "dryft.permissions.default_policy";
constexpr char kRememberDurationKey[] = "dryft.permissions.remember_duration_days";

// Default values
constexpr int kDefaultRememberDurationDays = 30;
constexpr int kDefaultRequestsPerMinute = 60;
constexpr int kDefaultSignsPerHour = 20;

// Convert Method enum to string
std::string MethodToString(NIP07Permission::Method method) {
  switch (method) {
    case NIP07Permission::Method::GET_PUBLIC_KEY:
      return "getPublicKey";
    case NIP07Permission::Method::SIGN_EVENT:
      return "signEvent";
    case NIP07Permission::Method::GET_RELAYS:
      return "getRelays";
    case NIP07Permission::Method::NIP04_ENCRYPT:
      return "nip04.encrypt";
    case NIP07Permission::Method::NIP04_DECRYPT:
      return "nip04.decrypt";
  }
  return "unknown";
}

// Convert string to Method enum
std::optional<NIP07Permission::Method> StringToMethod(const std::string& str) {
  if (str == "getPublicKey") return NIP07Permission::Method::GET_PUBLIC_KEY;
  if (str == "signEvent") return NIP07Permission::Method::SIGN_EVENT;
  if (str == "getRelays") return NIP07Permission::Method::GET_RELAYS;
  if (str == "nip04.encrypt") return NIP07Permission::Method::NIP04_ENCRYPT;
  if (str == "nip04.decrypt") return NIP07Permission::Method::NIP04_DECRYPT;
  return std::nullopt;
}

// Convert Policy enum to string
std::string PolicyToString(NIP07Permission::Policy policy) {
  switch (policy) {
    case NIP07Permission::Policy::ASK:
      return "ask";
    case NIP07Permission::Policy::ALLOW:
      return "allow";
    case NIP07Permission::Policy::DENY:
      return "deny";
  }
  return "ask";
}

// Convert string to Policy enum
NIP07Permission::Policy StringToPolicy(const std::string& str) {
  if (str == "allow") return NIP07Permission::Policy::ALLOW;
  if (str == "deny") return NIP07Permission::Policy::DENY;
  return NIP07Permission::Policy::ASK;
}

}  // namespace

// NIP07Permission implementation
base::Value::Dict NIP07Permission::ToValue() const {
  base::Value::Dict dict;
  
  dict.Set("origin", origin.Serialize());
  dict.Set("default_policy", PolicyToString(default_policy));
  dict.Set("granted_until", granted_until.ToJsTimeIgnoringNull());
  dict.Set("last_used", last_used.ToJsTimeIgnoringNull());
  
  // Method policies
  base::Value::Dict method_dict;
  for (const auto& [method, policy] : method_policies) {
    method_dict.Set(MethodToString(method), PolicyToString(policy));
  }
  dict.Set("method_policies", std::move(method_dict));
  
  // Kind policies
  base::Value::Dict kind_dict;
  for (const auto& [kind, policy] : kind_policies) {
    kind_dict.Set(base::NumberToString(kind), PolicyToString(policy));
  }
  dict.Set("kind_policies", std::move(kind_dict));
  
  // Rate limits
  base::Value::Dict rate_dict;
  rate_dict.Set("requests_per_minute", rate_limits.requests_per_minute);
  rate_dict.Set("signs_per_hour", rate_limits.signs_per_hour);
  rate_dict.Set("current_requests_count", rate_limits.current_requests_count);
  rate_dict.Set("current_signs_count", rate_limits.current_signs_count);
  rate_dict.Set("request_window_start", rate_limits.request_window_start.ToJsTimeIgnoringNull());
  rate_dict.Set("sign_window_start", rate_limits.sign_window_start.ToJsTimeIgnoringNull());
  dict.Set("rate_limits", std::move(rate_dict));
  
  return dict;
}

std::optional<NIP07Permission> NIP07Permission::FromValue(const base::Value::Dict& dict) {
  NIP07Permission permission;
  
  // Origin
  auto* origin_str = dict.FindString("origin");
  if (!origin_str) return std::nullopt;
  
  auto origin = url::Origin::Create(GURL(*origin_str));
  if (origin.opaque()) return std::nullopt;
  permission.origin = origin;
  
  // Default policy
  auto* default_policy_str = dict.FindString("default_policy");
  if (default_policy_str) {
    permission.default_policy = StringToPolicy(*default_policy_str);
  }
  
  // Timestamps
  auto granted_until = dict.FindDouble("granted_until");
  if (granted_until) {
    permission.granted_until = base::Time::FromJsTime(*granted_until);
  }
  
  auto last_used = dict.FindDouble("last_used");
  if (last_used) {
    permission.last_used = base::Time::FromJsTime(*last_used);
  }
  
  // Method policies
  auto* method_dict = dict.FindDict("method_policies");
  if (method_dict) {
    for (const auto [method_str, policy_value] : *method_dict) {
      if (!policy_value.is_string()) continue;
      
      auto method = StringToMethod(method_str);
      if (!method) continue;
      
      permission.method_policies[*method] = StringToPolicy(policy_value.GetString());
    }
  }
  
  // Kind policies
  auto* kind_dict = dict.FindDict("kind_policies");
  if (kind_dict) {
    for (const auto [kind_str, policy_value] : *kind_dict) {
      if (!policy_value.is_string()) continue;
      
      int kind;
      if (!base::StringToInt(kind_str, &kind)) continue;
      
      permission.kind_policies[kind] = StringToPolicy(policy_value.GetString());
    }
  }
  
  // Rate limits
  auto* rate_dict = dict.FindDict("rate_limits");
  if (rate_dict) {
    auto requests_per_minute = rate_dict->FindInt("requests_per_minute");
    if (requests_per_minute) {
      permission.rate_limits.requests_per_minute = *requests_per_minute;
    }
    
    auto signs_per_hour = rate_dict->FindInt("signs_per_hour");
    if (signs_per_hour) {
      permission.rate_limits.signs_per_hour = *signs_per_hour;
    }
    
    auto current_requests = rate_dict->FindInt("current_requests_count");
    if (current_requests) {
      permission.rate_limits.current_requests_count = *current_requests;
    }
    
    auto current_signs = rate_dict->FindInt("current_signs_count");
    if (current_signs) {
      permission.rate_limits.current_signs_count = *current_signs;
    }
    
    auto request_window = rate_dict->FindDouble("request_window_start");
    if (request_window) {
      permission.rate_limits.request_window_start = base::Time::FromJsTime(*request_window);
    }
    
    auto sign_window = rate_dict->FindDouble("sign_window_start");
    if (sign_window) {
      permission.rate_limits.sign_window_start = base::Time::FromJsTime(*sign_window);
    }
  }
  
  return permission;
}

// NostrPermissionManager implementation
NostrPermissionManager::NostrPermissionManager(Profile* profile)
    : profile_(profile), permissions_loaded_(false) {
  DCHECK(profile_);
  LoadPermissions();
}

NostrPermissionManager::~NostrPermissionManager() = default;

// static
void NostrPermissionManager::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kNostrPermissionsKey);
  registry->RegisterStringPref(kDefaultPolicyKey, "ask");
  registry->RegisterIntegerPref(kRememberDurationKey, kDefaultRememberDurationDays);
}

NostrPermissionManager::PermissionResult NostrPermissionManager::CheckPermission(
    const url::Origin& origin,
    NIP07Permission::Method method,
    int event_kind) {
  if (!permissions_loaded_) {
    LoadPermissions();
  }
  
  auto it = permissions_cache_.find(origin);
  if (it == permissions_cache_.end()) {
    // No permission entry exists, default to ASK
    return PermissionResult::ASK_USER;
  }
  
  NIP07Permission& permission = it->second;
  
  // Check if permission has expired
  if (IsPermissionExpired(permission)) {
    permissions_cache_.erase(it);
    SavePermissions();
    return PermissionResult::EXPIRED;
  }
  
  // Check rate limits
  if (!CheckRateLimit(permission, method)) {
    return PermissionResult::RATE_LIMITED;
  }
  
  // Check method-specific policy
  auto method_it = permission.method_policies.find(method);
  if (method_it != permission.method_policies.end()) {
    switch (method_it->second) {
      case NIP07Permission::Policy::ALLOW:
        UpdateLastUsed(origin);
        return PermissionResult::GRANTED;
      case NIP07Permission::Policy::DENY:
        return PermissionResult::DENIED;
      case NIP07Permission::Policy::ASK:
        return PermissionResult::ASK_USER;
    }
  }
  
  // For signEvent, check kind-specific policy
  if (method == NIP07Permission::Method::SIGN_EVENT && event_kind >= 0) {
    auto kind_it = permission.kind_policies.find(event_kind);
    if (kind_it != permission.kind_policies.end()) {
      switch (kind_it->second) {
        case NIP07Permission::Policy::ALLOW:
          UpdateLastUsed(origin);
          return PermissionResult::GRANTED;
        case NIP07Permission::Policy::DENY:
          return PermissionResult::DENIED;
        case NIP07Permission::Policy::ASK:
          return PermissionResult::ASK_USER;
      }
    }
  }
  
  // Fall back to default policy
  switch (permission.default_policy) {
    case NIP07Permission::Policy::ALLOW:
      UpdateLastUsed(origin);
      return PermissionResult::GRANTED;
    case NIP07Permission::Policy::DENY:
      return PermissionResult::DENIED;
    case NIP07Permission::Policy::ASK:
      return PermissionResult::ASK_USER;
  }
  
  return PermissionResult::ASK_USER;
}

NostrPermissionManager::GrantResult NostrPermissionManager::GrantPermission(
    const url::Origin& origin,
    const NIP07Permission& permission) {
  if (origin.opaque()) {
    return GrantResult::INVALID_ORIGIN;
  }
  
  if (!permissions_loaded_) {
    LoadPermissions();
  }
  
  permissions_cache_[origin] = permission;
  
  if (!SavePermissions()) {
    // Remove from cache if save failed
    permissions_cache_.erase(origin);
    return GrantResult::STORAGE_ERROR;
  }
  
  return GrantResult::SUCCESS;
}

bool NostrPermissionManager::RevokePermission(const url::Origin& origin) {
  if (!permissions_loaded_) {
    LoadPermissions();
  }
  
  auto it = permissions_cache_.find(origin);
  if (it == permissions_cache_.end()) {
    return false;  // Permission doesn't exist
  }
  
  permissions_cache_.erase(it);
  return SavePermissions();
}

bool NostrPermissionManager::RevokeMethodPermission(
    const url::Origin& origin,
    NIP07Permission::Method method) {
  if (!permissions_loaded_) {
    LoadPermissions();
  }
  
  auto it = permissions_cache_.find(origin);
  if (it == permissions_cache_.end()) {
    return false;  // Permission doesn't exist
  }
  
  it->second.method_policies.erase(method);
  return SavePermissions();
}

std::vector<NIP07Permission> NostrPermissionManager::GetAllPermissions() {
  if (!permissions_loaded_) {
    LoadPermissions();
  }
  
  std::vector<NIP07Permission> permissions;
  for (const auto& [origin, permission] : permissions_cache_) {
    permissions.push_back(permission);
  }
  
  return permissions;
}

std::optional<NIP07Permission> NostrPermissionManager::GetPermission(
    const url::Origin& origin) {
  if (!permissions_loaded_) {
    LoadPermissions();
  }
  
  auto it = permissions_cache_.find(origin);
  if (it == permissions_cache_.end()) {
    return std::nullopt;
  }
  
  return it->second;
}

bool NostrPermissionManager::UpdateRateLimit(const url::Origin& origin,
                                            NIP07Permission::Method method) {
  if (!permissions_loaded_) {
    LoadPermissions();
  }
  
  auto it = permissions_cache_.find(origin);
  if (it == permissions_cache_.end()) {
    return false;
  }
  
  UpdateRateLimitCounters(it->second, method);
  return SavePermissions();
}

void NostrPermissionManager::CleanupExpiredPermissions() {
  if (!permissions_loaded_) {
    LoadPermissions();
  }
  
  bool changed = false;
  auto it = permissions_cache_.begin();
  while (it != permissions_cache_.end()) {
    if (IsPermissionExpired(it->second)) {
      it = permissions_cache_.erase(it);
      changed = true;
    } else {
      ++it;
    }
  }
  
  if (changed) {
    SavePermissions();
  }
}

void NostrPermissionManager::ResetRateLimits(const url::Origin& origin) {
  auto it = permissions_cache_.find(origin);
  if (it != permissions_cache_.end()) {
    it->second.rate_limits.current_requests_count = 0;
    it->second.rate_limits.current_signs_count = 0;
    it->second.rate_limits.request_window_start = base::Time::Now();
    it->second.rate_limits.sign_window_start = base::Time::Now();
    SavePermissions();
  }
}

void NostrPermissionManager::Shutdown() {
  SavePermissions();
  permissions_cache_.clear();
}

void NostrPermissionManager::LoadPermissions() {
  if (!profile_) {
    permissions_loaded_ = true;
    return;
  }
  
  PrefService* prefs = profile_->GetPrefs();
  if (!prefs) {
    permissions_loaded_ = true;
    return;
  }
  
  const base::Value::Dict& permissions_dict = prefs->GetDict(kNostrPermissionsKey);
  
  permissions_cache_.clear();
  
  for (const auto [origin_str, permission_value] : permissions_dict) {
    if (!permission_value.is_dict()) {
      continue;
    }
    
    auto permission = NIP07Permission::FromValue(permission_value.GetDict());
    if (permission) {
      permissions_cache_[permission->origin] = *permission;
    }
  }
  
  permissions_loaded_ = true;
  LOG(INFO) << "Loaded " << permissions_cache_.size() << " Nostr permissions";
}

bool NostrPermissionManager::SavePermissions() {
  if (!profile_) {
    return false;
  }
  
  PrefService* prefs = profile_->GetPrefs();
  if (!prefs) {
    return false;
  }
  
  base::Value::Dict permissions_dict;
  
  for (const auto& [origin, permission] : permissions_cache_) {
    std::string key = GetPermissionKey(origin);
    permissions_dict.Set(key, permission.ToValue());
  }
  
  prefs->SetDict(kNostrPermissionsKey, std::move(permissions_dict));
  return true;
}

std::string NostrPermissionManager::GetPermissionKey(const url::Origin& origin) const {
  return origin.Serialize();
}

bool NostrPermissionManager::IsPermissionExpired(
    const NIP07Permission& permission) const {
  if (permission.granted_until.is_null()) {
    return false;  // No expiration set
  }
  
  return base::Time::Now() > permission.granted_until;
}

void NostrPermissionManager::UpdateLastUsed(const url::Origin& origin) {
  auto it = permissions_cache_.find(origin);
  if (it != permissions_cache_.end()) {
    it->second.last_used = base::Time::Now();
    // Note: We don't save immediately here for performance reasons
    // SavePermissions() will be called periodically or on shutdown
  }
}

bool NostrPermissionManager::CheckRateLimit(const NIP07Permission& permission,
                                           NIP07Permission::Method method) const {
  base::Time now = base::Time::Now();
  
  // Check request rate limit (per minute)
  base::TimeDelta request_window = now - permission.rate_limits.request_window_start;
  if (request_window < base::Minutes(1)) {
    if (permission.rate_limits.current_requests_count >= 
        permission.rate_limits.requests_per_minute) {
      return false;
    }
  }
  
  // Check signing rate limit (per hour) for sign operations
  if (method == NIP07Permission::Method::SIGN_EVENT) {
    base::TimeDelta sign_window = now - permission.rate_limits.sign_window_start;
    if (sign_window < base::Hours(1)) {
      if (permission.rate_limits.current_signs_count >= 
          permission.rate_limits.signs_per_hour) {
        return false;
      }
    }
  }
  
  return true;
}

void NostrPermissionManager::UpdateRateLimitCounters(NIP07Permission& permission,
                                                    NIP07Permission::Method method) {
  base::Time now = base::Time::Now();
  
  // Update request counter
  base::TimeDelta request_window = now - permission.rate_limits.request_window_start;
  if (request_window >= base::Minutes(1)) {
    // Reset window
    permission.rate_limits.current_requests_count = 1;
    permission.rate_limits.request_window_start = now;
  } else {
    permission.rate_limits.current_requests_count++;
  }
  
  // Update signing counter for sign operations
  if (method == NIP07Permission::Method::SIGN_EVENT) {
    base::TimeDelta sign_window = now - permission.rate_limits.sign_window_start;
    if (sign_window >= base::Hours(1)) {
      // Reset window
      permission.rate_limits.current_signs_count = 1;
      permission.rate_limits.sign_window_start = now;
    } else {
      permission.rate_limits.current_signs_count++;
    }
  }
}

}  // namespace nostr