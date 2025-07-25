// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/local_relay/local_relay_config.h"

#include "base/json/values_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/base/ip_address.h"

namespace nostr {
namespace local_relay {

LocalRelayConfigManager::LocalRelayConfigManager(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
}

LocalRelayConfigManager::~LocalRelayConfigManager() = default;

// static
void LocalRelayConfigManager::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // Network preferences
  registry->RegisterBooleanPref(kRelayEnabledPref, false);
  registry->RegisterIntegerPref(kRelayPortPref, kDefaultPort);
  registry->RegisterStringPref(kRelayInterfacePref, "127.0.0.1");
  registry->RegisterBooleanPref(kRelayExternalAccessPref, false);
  
  // Storage preferences
  registry->RegisterIntegerPref(kMaxStorageGBPref, kDefaultMaxStorageGB);
  registry->RegisterIntegerPref(kMaxEventsPref, kDefaultMaxEvents);
  registry->RegisterIntegerPref(kRetentionDaysPref, kDefaultRetentionDays);
  
  // Performance preferences
  registry->RegisterIntegerPref(kMaxConnectionsPref, kDefaultMaxConnections);
  registry->RegisterIntegerPref(kMaxSubscriptionsPerConnectionPref, 
                               kDefaultMaxSubscriptionsPerConnection);
  registry->RegisterIntegerPref(kMaxMessageSizePref, kDefaultMaxMessageSize);
  registry->RegisterIntegerPref(kMaxEventSizePref, kDefaultMaxEventSize);
  
  // Rate limiting
  registry->RegisterIntegerPref(kMaxEventsPerMinutePref, kDefaultMaxEventsPerMinute);
  registry->RegisterIntegerPref(kMaxReqPerMinutePref, kDefaultMaxReqPerMinute);
  
  // Access control
  registry->RegisterListPref(kAllowedOriginsPref);
  registry->RegisterBooleanPref(kRequireAuthPref, false);
  registry->RegisterListPref(kBlockedPubkeysPref);
  registry->RegisterListPref(kAllowedKindsPref);
}

LocalRelayConfig LocalRelayConfigManager::GetConfig() const {
  LocalRelayConfig config;
  
  // Network configuration
  config.bind_address = GetInterface();
  config.port = GetPort();
  
  // Connection limits
  config.max_connections = GetMaxConnections();
  config.max_subscriptions_per_connection = GetMaxSubscriptionsPerConnection();
  
  // Message limits
  config.max_message_size = GetMaxMessageSize();
  config.max_event_size = GetMaxEventSize();
  
  // Rate limiting
  config.max_events_per_minute = GetMaxEventsPerMinute();
  config.max_req_per_minute = GetMaxReqPerMinute();
  
  // Database configuration
  config.database_config.max_size_gb = GetMaxStorageGB();
  config.database_config.max_events = GetMaxEvents();
  config.database_config.retention_days = GetRetentionDays();
  
  return config;
}

bool LocalRelayConfigManager::IsEnabled() const {
  return pref_service_->GetBoolean(kRelayEnabledPref);
}

int LocalRelayConfigManager::GetPort() const {
  int port = pref_service_->GetInteger(kRelayPortPref);
  return IsValidPort(port) ? port : kDefaultPort;
}

std::string LocalRelayConfigManager::GetInterface() const {
  std::string interface = pref_service_->GetString(kRelayInterfacePref);
  return IsValidInterface(interface) ? interface : "127.0.0.1";
}

bool LocalRelayConfigManager::AllowsExternalAccess() const {
  return pref_service_->GetBoolean(kRelayExternalAccessPref);
}

int LocalRelayConfigManager::GetMaxStorageGB() const {
  int gb = pref_service_->GetInteger(kMaxStorageGBPref);
  return IsValidStorageLimit(gb) ? gb : kDefaultMaxStorageGB;
}

int LocalRelayConfigManager::GetMaxEvents() const {
  int max_events = pref_service_->GetInteger(kMaxEventsPref);
  return (max_events > 0) ? max_events : kDefaultMaxEvents;
}

int LocalRelayConfigManager::GetRetentionDays() const {
  int days = pref_service_->GetInteger(kRetentionDaysPref);
  return (days > 0) ? days : kDefaultRetentionDays;
}

int LocalRelayConfigManager::GetMaxConnections() const {
  int max_conn = pref_service_->GetInteger(kMaxConnectionsPref);
  return (max_conn > 0 && max_conn <= 1000) ? max_conn : kDefaultMaxConnections;
}

int LocalRelayConfigManager::GetMaxSubscriptionsPerConnection() const {
  int max_subs = pref_service_->GetInteger(kMaxSubscriptionsPerConnectionPref);
  return (max_subs > 0 && max_subs <= 100) ? max_subs : kDefaultMaxSubscriptionsPerConnection;
}

size_t LocalRelayConfigManager::GetMaxMessageSize() const {
  int size = pref_service_->GetInteger(kMaxMessageSizePref);
  return (size > 0) ? static_cast<size_t>(size) : kDefaultMaxMessageSize;
}

size_t LocalRelayConfigManager::GetMaxEventSize() const {
  int size = pref_service_->GetInteger(kMaxEventSizePref);
  return (size > 0) ? static_cast<size_t>(size) : kDefaultMaxEventSize;
}

int LocalRelayConfigManager::GetMaxEventsPerMinute() const {
  int rate = pref_service_->GetInteger(kMaxEventsPerMinutePref);
  return (rate > 0) ? rate : kDefaultMaxEventsPerMinute;
}

int LocalRelayConfigManager::GetMaxReqPerMinute() const {
  int rate = pref_service_->GetInteger(kMaxReqPerMinutePref);
  return (rate > 0) ? rate : kDefaultMaxReqPerMinute;
}

std::vector<std::string> LocalRelayConfigManager::GetAllowedOrigins() const {
  std::vector<std::string> origins;
  const base::Value::List& list = pref_service_->GetList(kAllowedOriginsPref);
  
  for (const auto& value : list) {
    if (value.is_string()) {
      origins.push_back(value.GetString());
    }
  }
  
  // Default to allowing all origins if none specified
  if (origins.empty()) {
    origins.push_back("*");
  }
  
  return origins;
}

bool LocalRelayConfigManager::RequiresAuth() const {
  return pref_service_->GetBoolean(kRequireAuthPref);
}

std::vector<std::string> LocalRelayConfigManager::GetBlockedPubkeys() const {
  std::vector<std::string> pubkeys;
  const base::Value::List& list = pref_service_->GetList(kBlockedPubkeysPref);
  
  for (const auto& value : list) {
    if (value.is_string() && value.GetString().length() == 64) {
      pubkeys.push_back(value.GetString());
    }
  }
  
  return pubkeys;
}

std::vector<int> LocalRelayConfigManager::GetAllowedKinds() const {
  std::vector<int> kinds;
  const base::Value::List& list = pref_service_->GetList(kAllowedKindsPref);
  
  for (const auto& value : list) {
    if (value.is_int()) {
      kinds.push_back(value.GetInt());
    }
  }
  
  // If no kinds specified, allow all
  return kinds;
}

void LocalRelayConfigManager::SetEnabled(bool enabled) {
  pref_service_->SetBoolean(kRelayEnabledPref, enabled);
}

void LocalRelayConfigManager::SetPort(int port) {
  if (IsValidPort(port)) {
    pref_service_->SetInteger(kRelayPortPref, port);
  }
}

void LocalRelayConfigManager::SetInterface(const std::string& interface) {
  if (IsValidInterface(interface)) {
    pref_service_->SetString(kRelayInterfacePref, interface);
  }
}

void LocalRelayConfigManager::SetExternalAccess(bool allowed) {
  pref_service_->SetBoolean(kRelayExternalAccessPref, allowed);
}

// static
bool LocalRelayConfigManager::IsValidPort(int port) {
  return port >= 1024 && port <= 65535;
}

// static
bool LocalRelayConfigManager::IsValidInterface(const std::string& interface) {
  if (interface.empty()) {
    return false;
  }
  
  // Check for special values
  if (interface == "localhost" || interface == "0.0.0.0") {
    return true;
  }
  
  // Validate IP address
  net::IPAddress addr;
  return addr.AssignFromIPLiteral(interface);
}

// static
bool LocalRelayConfigManager::IsValidStorageLimit(int gb) {
  // Allow 0 for unlimited, or 1-100 GB
  return gb == 0 || (gb >= 1 && gb <= 100);
}

base::Value::Dict LocalRelayConfigManager::GetStatistics() const {
  base::Value::Dict stats;
  
  // Configuration summary
  stats.Set("enabled", IsEnabled());
  stats.Set("port", GetPort());
  stats.Set("interface", GetInterface());
  stats.Set("external_access", AllowsExternalAccess());
  
  // Storage settings
  base::Value::Dict storage;
  storage.Set("max_gb", GetMaxStorageGB());
  storage.Set("max_events", GetMaxEvents());
  storage.Set("retention_days", GetRetentionDays());
  stats.Set("storage", std::move(storage));
  
  // Performance settings
  base::Value::Dict performance;
  performance.Set("max_connections", GetMaxConnections());
  performance.Set("max_subscriptions_per_connection", GetMaxSubscriptionsPerConnection());
  performance.Set("max_message_size", static_cast<int>(GetMaxMessageSize()));
  performance.Set("max_event_size", static_cast<int>(GetMaxEventSize()));
  stats.Set("performance", std::move(performance));
  
  // Rate limiting
  base::Value::Dict rate_limits;
  rate_limits.Set("max_events_per_minute", GetMaxEventsPerMinute());
  rate_limits.Set("max_req_per_minute", GetMaxReqPerMinute());
  stats.Set("rate_limits", std::move(rate_limits));
  
  return stats;
}

}  // namespace local_relay
}  // namespace nostr