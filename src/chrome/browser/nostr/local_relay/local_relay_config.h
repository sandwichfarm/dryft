// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_LOCAL_RELAY_LOCAL_RELAY_CONFIG_H_
#define CHROME_BROWSER_NOSTR_LOCAL_RELAY_LOCAL_RELAY_CONFIG_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/nostr/local_relay/nostr_database.h"

class PrefRegistrySimple;
class PrefService;

namespace nostr {
namespace local_relay {

// Configuration manager for the local Nostr relay
class LocalRelayConfigManager {
 public:
  // Preference keys
  static constexpr char kRelayEnabledPref[] = "tungsten.relay.enabled";
  static constexpr char kRelayPortPref[] = "tungsten.relay.port";
  static constexpr char kRelayInterfacePref[] = "tungsten.relay.interface";
  static constexpr char kRelayExternalAccessPref[] = "tungsten.relay.external_access";
  
  // Storage preferences
  static constexpr char kMaxStorageGBPref[] = "tungsten.relay.max_storage_gb";
  static constexpr char kMaxEventsPref[] = "tungsten.relay.max_events";
  static constexpr char kRetentionDaysPref[] = "tungsten.relay.retention_days";
  
  // Performance preferences  
  static constexpr char kMaxConnectionsPref[] = "tungsten.relay.max_connections";
  static constexpr char kMaxSubscriptionsPerConnectionPref[] = "tungsten.relay.max_subs_per_conn";
  static constexpr char kMaxMessageSizePref[] = "tungsten.relay.max_message_size";
  static constexpr char kMaxEventSizePref[] = "tungsten.relay.max_event_size";
  
  // Rate limiting preferences
  static constexpr char kMaxEventsPerMinutePref[] = "tungsten.relay.max_events_per_minute";
  static constexpr char kMaxReqPerMinutePref[] = "tungsten.relay.max_req_per_minute";
  
  // Access control preferences
  static constexpr char kAllowedOriginsPref[] = "tungsten.relay.allowed_origins";
  static constexpr char kRequireAuthPref[] = "tungsten.relay.require_auth";
  static constexpr char kBlockedPubkeysPref[] = "tungsten.relay.blocked_pubkeys";
  static constexpr char kAllowedKindsPref[] = "tungsten.relay.allowed_kinds";
  
  // Default values
  static constexpr int kDefaultPort = 8081;
  static constexpr int kDefaultMaxConnections = 100;
  static constexpr int kDefaultMaxSubscriptionsPerConnection = 20;
  static constexpr int kDefaultMaxStorageGB = 1;
  static constexpr int kDefaultMaxEvents = 100000;
  static constexpr int kDefaultRetentionDays = 30;
  static constexpr int kDefaultMaxEventsPerMinute = 100;
  static constexpr int kDefaultMaxReqPerMinute = 60;
  static constexpr size_t kDefaultMaxMessageSize = 512 * 1024;  // 512KB
  static constexpr size_t kDefaultMaxEventSize = 256 * 1024;    // 256KB
  
  explicit LocalRelayConfigManager(PrefService* pref_service);
  ~LocalRelayConfigManager();
  
  // Register preferences
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  
  // Get configuration
  LocalRelayConfig GetConfig() const;
  
  // Configuration getters
  bool IsEnabled() const;
  int GetPort() const;
  std::string GetInterface() const;
  bool AllowsExternalAccess() const;
  
  // Storage configuration
  int GetMaxStorageGB() const;
  int GetMaxEvents() const;
  int GetRetentionDays() const;
  
  // Performance configuration
  int GetMaxConnections() const;
  int GetMaxSubscriptionsPerConnection() const;
  size_t GetMaxMessageSize() const;
  size_t GetMaxEventSize() const;
  
  // Rate limiting
  int GetMaxEventsPerMinute() const;
  int GetMaxReqPerMinute() const;
  
  // Access control
  std::vector<std::string> GetAllowedOrigins() const;
  bool RequiresAuth() const;
  std::vector<std::string> GetBlockedPubkeys() const;
  
  // Get allowed event kinds. Returns empty vector if all kinds are allowed.
  // When empty, the relay accepts all event kinds. When populated, only
  // events with kinds in this list will be accepted.
  std::vector<int> GetAllowedKinds() const;
  
  // Configuration setters (for testing)
  void SetEnabled(bool enabled);
  void SetPort(int port);
  void SetInterface(const std::string& interface);
  void SetExternalAccess(bool allowed);
  
  // Validation
  static bool IsValidPort(int port);
  static bool IsValidInterface(const std::string& interface);
  static bool IsValidStorageLimit(int gb);
  
  // Get statistics for display
  base::Value::Dict GetStatistics() const;
  
 private:
  PrefService* pref_service_;  // Not owned
};

}  // namespace local_relay
}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_LOCAL_RELAY_LOCAL_RELAY_CONFIG_H_