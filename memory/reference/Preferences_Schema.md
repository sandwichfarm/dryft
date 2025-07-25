# Preferences Schema
## dryft browser - Comprehensive Configuration System

### 1. Preferences Overview

dryft uses a hierarchical preference system with:
- **User Preferences**: Stored per-profile
- **Policy Preferences**: Set by administrators
- **Default Preferences**: Built-in defaults
- **Synced Preferences**: Optionally synced across devices

### 2. Complete Preference Registry

```cpp
// chrome/common/pref_names_tungsten.h
namespace dryft {
namespace prefs {

// ============================================================================
// Account Management
// ============================================================================
struct AccountPrefs {
  // List of all Nostr accounts
  static constexpr char kAccounts[] = "dryft.nostr.accounts";
  // Currently active account pubkey
  static constexpr char kActiveAccount[] = "dryft.nostr.active_account";
  // Per-account settings (dict keyed by pubkey)
  static constexpr char kAccountSettings[] = "dryft.nostr.account_settings";
  // Account creation defaults
  static constexpr char kAccountDefaults[] = "dryft.nostr.account_defaults";
};

// ============================================================================
// NIP-07 Permissions
// ============================================================================
struct PermissionPrefs {
  // Per-origin permissions (nested dict)
  static constexpr char kNIP07Permissions[] = "dryft.permissions.nip07";
  // Default permission policy: "ask" | "deny" | "allow"
  static constexpr char kDefaultPolicy[] = "dryft.permissions.default_policy";
  // Remember permission decisions
  static constexpr char kRememberDuration[] = "dryft.permissions.remember_duration_days";
  // Require password for signing
  static constexpr char kRequirePassword[] = "dryft.permissions.require_password";
  // Per-kind signing permissions
  static constexpr char kKindPermissions[] = "dryft.permissions.kind_permissions";
  // Rate limiting
  static constexpr char kRateLimits[] = "dryft.permissions.rate_limits";
};

// ============================================================================
// Local Relay Configuration  
// ============================================================================
struct LocalRelayPrefs {
  // Enable/disable local relay
  static constexpr char kEnabled[] = "dryft.relay.enabled";
  // Network settings
  static constexpr char kPort[] = "dryft.relay.port";
  static constexpr char kInterface[] = "dryft.relay.interface";
  static constexpr char kExternalAccess[] = "dryft.relay.external_access";
  
  // Storage settings
  static constexpr char kMaxStorageGB[] = "dryft.relay.max_storage_gb";
  static constexpr char kMaxEvents[] = "dryft.relay.max_events";
  static constexpr char kRetentionDays[] = "dryft.relay.retention_days";
  static constexpr char kCompressionEnabled[] = "dryft.relay.compression";
  
  // Performance settings
  static constexpr char kMaxConnections[] = "dryft.relay.max_connections";
  static constexpr char kMaxSubscriptionsPerConnection[] = "dryft.relay.max_subs_per_conn";
  static constexpr char kMessageQueueSize[] = "dryft.relay.message_queue_size";
  static constexpr char kWorkerThreads[] = "dryft.relay.worker_threads";
  
  // Filtering
  static constexpr char kAllowedKinds[] = "dryft.relay.allowed_kinds";
  static constexpr char kBlockedKinds[] = "dryft.relay.blocked_kinds";
  static constexpr char kBlockedPubkeys[] = "dryft.relay.blocked_pubkeys";
  static constexpr char kRequiredPOW[] = "dryft.relay.required_pow";
  static constexpr char kMaxEventSize[] = "dryft.relay.max_event_size";
  
  // Access control
  static constexpr char kAllowedOrigins[] = "dryft.relay.allowed_origins";
  static constexpr char kRequireAuth[] = "dryft.relay.require_auth";
  static constexpr char kAuthMethods[] = "dryft.relay.auth_methods";
};

// ============================================================================
// Blossom Configuration
// ============================================================================
struct BlossomPrefs {
  // Enable/disable Blossom
  static constexpr char kEnabled[] = "dryft.blossom.enabled";
  // Local server settings
  static constexpr char kLocalServerEnabled[] = "dryft.blossom.local_server";
  static constexpr char kLocalServerPort[] = "dryft.blossom.local_port";
  
  // Storage settings
  static constexpr char kMaxStorageGB[] = "dryft.blossom.max_storage_gb";
  static constexpr char kCacheDuration[] = "dryft.blossom.cache_duration_days";
  static constexpr char kShardingEnabled[] = "dryft.blossom.sharding";
  
  // Upload settings
  static constexpr char kMaxFileSizeMB[] = "dryft.blossom.max_file_size_mb";
  static constexpr char kAllowedMimeTypes[] = "dryft.blossom.allowed_types";
  static constexpr char kAutoCompress[] = "dryft.blossom.auto_compress";
  static constexpr char kCompressionThreshold[] = "dryft.blossom.compress_threshold_kb";
  
  // Server list (BUD-03)
  static constexpr char kUserServers[] = "dryft.blossom.user_servers";
  static constexpr char kTrustedServers[] = "dryft.blossom.trusted_servers";
  
  // Mirroring
  static constexpr char kAutoMirror[] = "dryft.blossom.auto_mirror";
  static constexpr char kMirrorServers[] = "dryft.blossom.mirror_servers";
  static constexpr char kMirrorRedundancy[] = "dryft.blossom.mirror_redundancy";
};

// ============================================================================
// Library Management
// ============================================================================
struct LibraryPrefs {
  // Enable/disable library injection
  static constexpr char kEnabled[] = "dryft.libs.enabled";
  // Which libraries to load
  static constexpr char kEnabledLibs[] = "dryft.libs.enabled_list";
  // Library versions
  static constexpr char kLibVersions[] = "dryft.libs.versions";
  // Whitelist origins that can use libs
  static constexpr char kWhitelistedOrigins[] = "dryft.libs.whitelist";
};

// ============================================================================
// Security Settings
// ============================================================================
struct SecurityPrefs {
  // Key management
  static constexpr char kKeyUnlockTimeout[] = "dryft.security.key_timeout_minutes";
  static constexpr char kRequireAuthForSigning[] = "dryft.security.require_auth";
  static constexpr char kAuthMethod[] = "dryft.security.auth_method";
  static constexpr char kHardwareWalletEnabled[] = "dryft.security.hardware_wallet";
  
  // Audit logging
  static constexpr char kAuditEnabled[] = "dryft.security.audit_enabled";
  static constexpr char kAuditRetentionDays[] = "dryft.security.audit_retention";
  static constexpr char kAuditEvents[] = "dryft.security.audit_events";
  
  // Content security
  static constexpr char kStrictCSP[] = "dryft.security.strict_csp";
  static constexpr char kBlockMixedContent[] = "dryft.security.block_mixed";
};

// ============================================================================
// Network Settings
// ============================================================================
struct NetworkPrefs {
  // Default relays
  static constexpr char kDefaultRelays[] = "dryft.network.default_relays";
  // Per-relay settings
  static constexpr char kRelaySettings[] = "dryft.network.relay_settings";
  // Proxy configuration
  static constexpr char kProxyEnabled[] = "dryft.network.proxy_enabled";
  static constexpr char kProxySettings[] = "dryft.network.proxy_settings";
  // WebRTC
  static constexpr char kWebRTCPolicy[] = "dryft.network.webrtc_policy";
  // DNS
  static constexpr char kDNSOverHTTPS[] = "dryft.network.dns_over_https";
};

// ============================================================================
// Developer Settings
// ============================================================================
struct DeveloperPrefs {
  // Developer mode
  static constexpr char kDevModeEnabled[] = "dryft.dev.enabled";
  // Debug logging
  static constexpr char kDebugLogging[] = "dryft.dev.debug_logging";
  static constexpr char kLogCategories[] = "dryft.dev.log_categories";
  // Testing features
  static constexpr char kTestRelayEnabled[] = "dryft.dev.test_relay";
  static constexpr char kMockDataEnabled[] = "dryft.dev.mock_data";
};

}  // namespace prefs
}  // namespace dryft
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
    export_data.Set("browser", "dryft");
    
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
    <category name="TungstenNostr" displayName="dryft Nostr Settings"/>
  </categories>
  
  <policies>
    <policy name="NostrEnabled" class="Both" displayName="Enable Nostr Features"
            explainText="Controls whether Nostr features are available">
      <parentCategory ref="TungstenNostr"/>
      <supportedOn ref="SUPPORTED_DRYFT_1_0"/>
      <enabledValue><decimal value="1"/></enabledValue>
      <disabledValue><decimal value="0"/></disabledValue>
    </policy>
    
    <policy name="LocalRelayPolicy" class="Both" displayName="Local Relay Configuration">
      <parentCategory ref="TungstenNostr"/>
      <elements>
        <boolean id="Enabled" valueName="LocalRelayEnabled"/>
        <decimal id="Port" valueName="LocalRelayPort" minValue="1024" maxValue="65535"/>
        <decimal id="MaxStorageGB" valueName="MaxStorage" minValue="0.1" maxValue="100"/>
        <list id="AllowedOrigins" key="Software\Policies\dryft\LocalRelay\AllowedOrigins"/>
      </elements>
    </policy>
    
    <policy name="MandatoryRelays" class="Both" displayName="Mandatory Relay List">
      <parentCategory ref="TungstenNostr"/>
      <elements>
        <list id="RelayList" key="Software\Policies\dryft\Nostr\MandatoryRelays"/>
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