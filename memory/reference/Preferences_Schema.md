# Preferences Schema
## Tungsten Browser - Comprehensive Configuration System

### 1. Preferences Overview

Tungsten uses a hierarchical preference system with:
- **User Preferences**: Stored per-profile
- **Policy Preferences**: Set by administrators
- **Default Preferences**: Built-in defaults
- **Synced Preferences**: Optionally synced across devices

### 2. Complete Preference Registry

```cpp
// chrome/common/pref_names_tungsten.h
namespace tungsten {
namespace prefs {

// ============================================================================
// Account Management
// ============================================================================
struct AccountPrefs {
  // List of all Nostr accounts
  static constexpr char kAccounts[] = "tungsten.nostr.accounts";
  // Currently active account pubkey
  static constexpr char kActiveAccount[] = "tungsten.nostr.active_account";
  // Per-account settings (dict keyed by pubkey)
  static constexpr char kAccountSettings[] = "tungsten.nostr.account_settings";
  // Account creation defaults
  static constexpr char kAccountDefaults[] = "tungsten.nostr.account_defaults";
};

// ============================================================================
// NIP-07 Permissions
// ============================================================================
struct PermissionPrefs {
  // Per-origin permissions (nested dict)
  static constexpr char kNIP07Permissions[] = "tungsten.permissions.nip07";
  // Default permission policy: "ask" | "deny" | "allow"
  static constexpr char kDefaultPolicy[] = "tungsten.permissions.default_policy";
  // Remember permission decisions
  static constexpr char kRememberDuration[] = "tungsten.permissions.remember_duration_days";
  // Require password for signing
  static constexpr char kRequirePassword[] = "tungsten.permissions.require_password";
  // Per-kind signing permissions
  static constexpr char kKindPermissions[] = "tungsten.permissions.kind_permissions";
  // Rate limiting
  static constexpr char kRateLimits[] = "tungsten.permissions.rate_limits";
};

// ============================================================================
// Local Relay Configuration  
// ============================================================================
struct LocalRelayPrefs {
  // Enable/disable local relay
  static constexpr char kEnabled[] = "tungsten.relay.enabled";
  // Network settings
  static constexpr char kPort[] = "tungsten.relay.port";
  static constexpr char kInterface[] = "tungsten.relay.interface";
  static constexpr char kExternalAccess[] = "tungsten.relay.external_access";
  
  // Storage settings
  static constexpr char kMaxStorageGB[] = "tungsten.relay.max_storage_gb";
  static constexpr char kMaxEvents[] = "tungsten.relay.max_events";
  static constexpr char kRetentionDays[] = "tungsten.relay.retention_days";
  static constexpr char kCompressionEnabled[] = "tungsten.relay.compression";
  
  // Performance settings
  static constexpr char kMaxConnections[] = "tungsten.relay.max_connections";
  static constexpr char kMaxSubscriptionsPerConnection[] = "tungsten.relay.max_subs_per_conn";
  static constexpr char kMessageQueueSize[] = "tungsten.relay.message_queue_size";
  static constexpr char kWorkerThreads[] = "tungsten.relay.worker_threads";
  
  // Filtering
  static constexpr char kAllowedKinds[] = "tungsten.relay.allowed_kinds";
  static constexpr char kBlockedKinds[] = "tungsten.relay.blocked_kinds";
  static constexpr char kBlockedPubkeys[] = "tungsten.relay.blocked_pubkeys";
  static constexpr char kRequiredPOW[] = "tungsten.relay.required_pow";
  static constexpr char kMaxEventSize[] = "tungsten.relay.max_event_size";
  
  // Access control
  static constexpr char kAllowedOrigins[] = "tungsten.relay.allowed_origins";
  static constexpr char kRequireAuth[] = "tungsten.relay.require_auth";
  static constexpr char kAuthMethods[] = "tungsten.relay.auth_methods";
};

// ============================================================================
// Blossom Configuration
// ============================================================================
struct BlossomPrefs {
  // Enable/disable Blossom
  static constexpr char kEnabled[] = "tungsten.blossom.enabled";
  // Local server settings
  static constexpr char kLocalServerEnabled[] = "tungsten.blossom.local_server";
  static constexpr char kLocalServerPort[] = "tungsten.blossom.local_port";
  
  // Storage settings
  static constexpr char kMaxStorageGB[] = "tungsten.blossom.max_storage_gb";
  static constexpr char kCacheDuration[] = "tungsten.blossom.cache_duration_days";
  static constexpr char kShardingEnabled[] = "tungsten.blossom.sharding";
  
  // Upload settings
  static constexpr char kMaxFileSizeMB[] = "tungsten.blossom.max_file_size_mb";
  static constexpr char kAllowedMimeTypes[] = "tungsten.blossom.allowed_types";
  static constexpr char kAutoCompress[] = "tungsten.blossom.auto_compress";
  static constexpr char kCompressionThreshold[] = "tungsten.blossom.compress_threshold_kb";
  
  // Server list (BUD-03)
  static constexpr char kUserServers[] = "tungsten.blossom.user_servers";
  static constexpr char kTrustedServers[] = "tungsten.blossom.trusted_servers";
  
  // Mirroring
  static constexpr char kAutoMirror[] = "tungsten.blossom.auto_mirror";
  static constexpr char kMirrorServers[] = "tungsten.blossom.mirror_servers";
  static constexpr char kMirrorRedundancy[] = "tungsten.blossom.mirror_redundancy";
};

// ============================================================================
// Library Management
// ============================================================================
struct LibraryPrefs {
  // Enable/disable library injection
  static constexpr char kEnabled[] = "tungsten.libs.enabled";
  // Which libraries to load
  static constexpr char kEnabledLibs[] = "tungsten.libs.enabled_list";
  // Library versions
  static constexpr char kLibVersions[] = "tungsten.libs.versions";
  // Whitelist origins that can use libs
  static constexpr char kWhitelistedOrigins[] = "tungsten.libs.whitelist";
};

// ============================================================================
// Security Settings
// ============================================================================
struct SecurityPrefs {
  // Key management
  static constexpr char kKeyUnlockTimeout[] = "tungsten.security.key_timeout_minutes";
  static constexpr char kRequireAuthForSigning[] = "tungsten.security.require_auth";
  static constexpr char kAuthMethod[] = "tungsten.security.auth_method";
  static constexpr char kHardwareWalletEnabled[] = "tungsten.security.hardware_wallet";
  
  // Audit logging
  static constexpr char kAuditEnabled[] = "tungsten.security.audit_enabled";
  static constexpr char kAuditRetentionDays[] = "tungsten.security.audit_retention";
  static constexpr char kAuditEvents[] = "tungsten.security.audit_events";
  
  // Content security
  static constexpr char kStrictCSP[] = "tungsten.security.strict_csp";
  static constexpr char kBlockMixedContent[] = "tungsten.security.block_mixed";
};

// ============================================================================
// Network Settings
// ============================================================================
struct NetworkPrefs {
  // Default relays
  static constexpr char kDefaultRelays[] = "tungsten.network.default_relays";
  // Per-relay settings
  static constexpr char kRelaySettings[] = "tungsten.network.relay_settings";
  // Proxy configuration
  static constexpr char kProxyEnabled[] = "tungsten.network.proxy_enabled";
  static constexpr char kProxySettings[] = "tungsten.network.proxy_settings";
  // WebRTC
  static constexpr char kWebRTCPolicy[] = "tungsten.network.webrtc_policy";
  // DNS
  static constexpr char kDNSOverHTTPS[] = "tungsten.network.dns_over_https";
};

// ============================================================================
// Developer Settings
// ============================================================================
struct DeveloperPrefs {
  // Developer mode
  static constexpr char kDevModeEnabled[] = "tungsten.dev.enabled";
  // Debug logging
  static constexpr char kDebugLogging[] = "tungsten.dev.debug_logging";
  static constexpr char kLogCategories[] = "tungsten.dev.log_categories";
  // Testing features
  static constexpr char kTestRelayEnabled[] = "tungsten.dev.test_relay";
  static constexpr char kMockDataEnabled[] = "tungsten.dev.mock_data";
};

}  // namespace prefs
}  // namespace tungsten
```

### 3. Preference Types and Validation

```cpp
// Preference registration with types and validation
class TungstenPrefsRegistration {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry) {
    // Account preferences
    registry->RegisterDictionaryPref(
        prefs::AccountPrefs::kAccounts,
        base::Value::Dict(),
        PrefRegistry::SYNCABLE);
    
    registry->RegisterStringPref(
        prefs::AccountPrefs::kActiveAccount,
        "",
        PrefRegistry::SYNCABLE);
    
    // Permission preferences with validation
    registry->RegisterDictionaryPref(
        prefs::PermissionPrefs::kNIP07Permissions,
        CreateDefaultPermissions());
    
    registry->RegisterIntegerPref(
        prefs::PermissionPrefs::kRememberDuration,
        7,  // Default 7 days
        PrefRegistry::SYNCABLE,
        base::BindRepeating(&ValidateRememberDuration));
    
    // Local relay with constraints
    registry->RegisterIntegerPref(
        prefs::LocalRelayPrefs::kPort,
        8081,
        PrefRegistry::SYNCABLE,
        base::BindRepeating(&ValidatePort));
    
    registry->RegisterDoublePref(
        prefs::LocalRelayPrefs::kMaxStorageGB,
        1.0,  // Default 1GB
        PrefRegistry::SYNCABLE,
        base::BindRepeating(&ValidateStorageSize));
    
    // Blossom preferences
    registry->RegisterListPref(
        prefs::BlossomPrefs::kAllowedMimeTypes,
        CreateDefaultMimeTypes());
    
    // Security preferences
    registry->RegisterIntegerPref(
        prefs::SecurityPrefs::kKeyUnlockTimeout,
        5,  // 5 minutes default
        PrefRegistry::SYNCABLE,
        base::BindRepeating(&ValidateTimeout));
  }
  
 private:
  static bool ValidatePort(const base::Value* value) {
    if (!value->is_int()) return false;
    int port = value->GetInt();
    return port >= 1024 && port <= 65535;
  }
  
  static bool ValidateStorageSize(const base::Value* value) {
    if (!value->is_double()) return false;
    double size = value->GetDouble();
    return size >= 0.1 && size <= 1000.0;  // 100MB to 1TB
  }
  
  static base::Value::Dict CreateDefaultPermissions() {
    base::Value::Dict defaults;
    defaults.Set("default_policy", "ask");
    defaults.Set("remember_decisions", true);
    return defaults;
  }
};
```

### 4. Settings UI Components

```typescript
// React components for settings UI
interface SettingsSection {
  id: string;
  title: string;
  icon: string;
  component: React.ComponentType;
  requiresRestart?: boolean;
  advanced?: boolean;
}

const SETTINGS_SECTIONS: SettingsSection[] = [
  {
    id: 'accounts',
    title: 'Nostr Accounts',
    icon: '👤',
    component: AccountSettings
  },
  {
    id: 'permissions',
    title: 'Permissions & Privacy',
    icon: '🔒',
    component: PermissionSettings
  },
  {
    id: 'local-relay',
    title: 'Local Relay',
    icon: '📡',
    component: LocalRelaySettings,
    requiresRestart: true
  },
  {
    id: 'blossom',
    title: 'Blossom Storage',
    icon: '🌸',
    component: BlossomSettings
  },
  {
    id: 'network',
    title: 'Network & Connections',
    icon: '🌐',
    component: NetworkSettings
  },
  {
    id: 'security',
    title: 'Security',
    icon: '🛡️',
    component: SecuritySettings
  },
  {
    id: 'developer',
    title: 'Developer Tools',
    icon: '🛠️',
    component: DeveloperSettings,
    advanced: true
  }
];

// Account management component
class AccountSettings extends React.Component {
  render() {
    return (
      <div className="settings-page">
        <h2>Nostr Accounts</h2>
        
        <AccountList 
          accounts={this.props.accounts}
          activeAccount={this.props.activeAccount}
          onSwitch={this.handleAccountSwitch}
          onEdit={this.handleAccountEdit}
          onDelete={this.handleAccountDelete}
        />
        
        <div className="account-actions">
          <button onClick={this.handleCreateAccount}>
            Create New Account
          </button>
          <button onClick={this.handleImportAccount}>
            Import Account
          </button>
          <button onClick={this.handleExportAccount}>
            Export Current Account
          </button>
        </div>
        
        <AccountDefaults 
          defaults={this.props.accountDefaults}
          onChange={this.handleDefaultsChange}
        />
      </div>
    );
  }
}

// Permission management with granular controls
class PermissionSettings extends React.Component {
  render() {
    return (
      <div className="settings-page">
        <h2>Permissions & Privacy</h2>
        
        <div className="permission-section">
          <h3>Default Permission Policy</h3>
          <RadioGroup
            value={this.props.defaultPolicy}
            onChange={this.handlePolicyChange}
            options={[
              { value: 'ask', label: 'Always ask' },
              { value: 'allow', label: 'Allow all (not recommended)' },
              { value: 'deny', label: 'Deny all' }
            ]}
          />
        </div>
        
        <div className="permission-section">
          <h3>Site Permissions</h3>
          <SitePermissionsList 
            permissions={this.props.sitePermissions}
            onEdit={this.handleEditPermission}
            onRevoke={this.handleRevokePermission}
          />
        </div>
        
        <div className="permission-section">
          <h3>Event Kind Permissions</h3>
          <KindPermissionMatrix
            permissions={this.props.kindPermissions}
            onChange={this.handleKindPermissionChange}
          />
        </div>
        
        <div className="permission-section">
          <h3>Rate Limiting</h3>
          <RateLimitSettings
            limits={this.props.rateLimits}
            onChange={this.handleRateLimitChange}
          />
        </div>
      </div>
    );
  }
}
```

### 5. Preference Sync and Backup

```cpp
class PreferenceSyncManager {
 public:
  // Export preferences for backup
  base::Value::Dict ExportPreferences() {
    base::Value::Dict export_data;
    
    // Version info
    export_data.Set("version", "1.0");
    export_data.Set("timestamp", base::Time::Now().ToJsTime());
    export_data.Set("browser", "Tungsten");
    
    // Export each category
    export_data.Set("accounts", ExportAccountPrefs());
    export_data.Set("permissions", ExportPermissionPrefs());
    export_data.Set("relay", ExportRelayPrefs());
    export_data.Set("blossom", ExportBlossomPrefs());
    export_data.Set("security", ExportSecurityPrefs());
    
    return export_data;
  }
  
  // Import preferences with validation
  bool ImportPreferences(const base::Value::Dict& import_data) {
    // Validate version
    const std::string* version = import_data.FindString("version");
    if (!version || !IsCompatibleVersion(*version)) {
      return false;
    }
    
    // Create backup before import
    auto backup = ExportPreferences();
    
    try {
      // Import each category
      if (auto* accounts = import_data.FindDict("accounts")) {
        ImportAccountPrefs(*accounts);
      }
      
      if (auto* permissions = import_data.FindDict("permissions")) {
        ImportPermissionPrefs(*permissions);
      }
      
      // ... import other categories
      
      return true;
    } catch (const std::exception& e) {
      // Restore from backup on failure
      RestoreFromBackup(backup);
      return false;
    }
  }
  
  // Sync preferences across devices (optional)
  void EnableSync(const std::string& sync_passphrase) {
    // Encrypt sensitive preferences
    auto encrypted = EncryptPreferences(
        GetSyncablePreferences(),
        sync_passphrase);
    
    // Upload to sync server
    sync_client_->UploadPreferences(encrypted);
  }
};
```

### 6. Enterprise Policy Support

```xml
<!-- Administrative template for enterprise deployment -->
<policyDefinitions>
  <categories>
    <category name="TungstenNostr" displayName="Tungsten Nostr Settings"/>
  </categories>
  
  <policies>
    <policy name="NostrEnabled" class="Both" displayName="Enable Nostr Features"
            explainText="Controls whether Nostr features are available">
      <parentCategory ref="TungstenNostr"/>
      <supportedOn ref="SUPPORTED_TUNGSTEN_1_0"/>
      <enabledValue><decimal value="1"/></enabledValue>
      <disabledValue><decimal value="0"/></disabledValue>
    </policy>
    
    <policy name="LocalRelayPolicy" class="Both" displayName="Local Relay Configuration">
      <parentCategory ref="TungstenNostr"/>
      <elements>
        <boolean id="Enabled" valueName="LocalRelayEnabled"/>
        <decimal id="Port" valueName="LocalRelayPort" minValue="1024" maxValue="65535"/>
        <decimal id="MaxStorageGB" valueName="MaxStorage" minValue="0.1" maxValue="100"/>
        <list id="AllowedOrigins" key="Software\Policies\Tungsten\LocalRelay\AllowedOrigins"/>
      </elements>
    </policy>
    
    <policy name="MandatoryRelays" class="Both" displayName="Mandatory Relay List">
      <parentCategory ref="TungstenNostr"/>
      <elements>
        <list id="RelayList" key="Software\Policies\Tungsten\Nostr\MandatoryRelays"/>
      </elements>
    </policy>
  </policies>
</policyDefinitions>
```

### 7. Preference Migration

```cpp
class PreferenceMigration {
 public:
  // Migrate from older versions
  void MigratePreferences(int from_version, int to_version) {
    if (from_version < 2 && to_version >= 2) {
      MigrateV1ToV2();
    }
    
    if (from_version < 3 && to_version >= 3) {
      MigrateV2ToV3();
    }
  }
  
 private:
  void MigrateV1ToV2() {
    // Example: Migrate flat permission structure to hierarchical
    auto old_permissions = prefs_->GetDict("nostr.permissions");
    base::Value::Dict new_permissions;
    
    for (auto [key, value] : old_permissions) {
      // Convert old format to new format
      url::Origin origin = url::Origin::Create(GURL(key));
      new_permissions.SetByDottedPath(
          origin.Serialize() + ".policy",
          value.Clone());
    }
    
    prefs_->Set(prefs::PermissionPrefs::kNIP07Permissions, 
                std::move(new_permissions));
  }
};
```

This comprehensive preference system provides:

1. **Hierarchical Organization**: Clear categorization of all settings
2. **Type Safety**: Validated preference types with constraints
3. **Default Values**: Sensible defaults for all preferences
4. **Sync Support**: Optional synchronization across devices
5. **Enterprise Policy**: Group policy support for managed deployments
6. **Import/Export**: Full preference portability
7. **Migration Support**: Smooth upgrades between versions
8. **Granular Control**: Fine-grained permissions and settings