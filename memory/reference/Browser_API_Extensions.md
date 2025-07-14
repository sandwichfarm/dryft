# Browser API Extensions
## Tungsten Browser - Extended Web APIs for Nostr, Blossom, and Developer Tools

### 1. Extended window.nostr API

```typescript
interface WindowNostr {
  // Standard NIP-07 methods
  getPublicKey(): Promise<string>;
  signEvent(event: UnsignedEvent): Promise<SignedEvent>;
  getRelays(): Promise<RelayPolicy>;
  nip04: {
    encrypt(pubkey: string, plaintext: string): Promise<string>;
    decrypt(pubkey: string, ciphertext: string): Promise<string>;
  };
  
  // Tungsten Extensions
  relay: {
    // Local relay URL (read-only)
    readonly url: string;  // e.g., "ws://localhost:8081"
    
    // Local relay status
    readonly connected: boolean;
    readonly eventCount: number;
    readonly storageUsed: number; // bytes
    
    // Local relay methods
    query(filter: NostrFilter): Promise<NostrEvent[]>;
    count(filter: NostrFilter): Promise<number>;
    deleteEvents(filter: NostrFilter): Promise<number>; // returns count deleted
  };
  
  // Pre-loaded Nostr libraries (returns importable URLs)
  libs: {
    ndk: string;                      // e.g., "chrome://resources/js/nostr/ndk.js"
    'nostr-tools': string;            // e.g., "chrome://resources/js/nostr/nostr-tools.js"
    'applesauce-core': string;        // e.g., "chrome://resources/js/nostr/applesauce-core.js"
    'applesauce-content': string;     // e.g., "chrome://resources/js/nostr/applesauce-content.js"
    'applesauce-lists': string;       // e.g., "chrome://resources/js/nostr/applesauce-lists.js"
    'alby-sdk': string;               // e.g., "chrome://resources/js/nostr/alby-sdk.js"
    
    // Version information
    versions: {
      ndk: string;                    // e.g., "2.0.0"
      'nostr-tools': string;          // e.g., "1.17.0"
      'applesauce-core': string;      // e.g., "0.3.4"
      'applesauce-content': string;   // e.g., "0.3.4"
      'applesauce-lists': string;     // e.g., "0.3.4"
      'alby-sdk': string;             // e.g., "3.0.0"
    };
  };
  
  // Extended capabilities
  capabilities: {
    nips: number[];  // Supported NIPs
    blossomEnabled: boolean;
    localRelay: boolean;
    multiAccount: boolean;
    hardwareWallet: boolean;
  };
  
  // Account management
  accounts: {
    list(): Promise<NostrAccount[]>;
    current(): Promise<NostrAccount>;
    switch(pubkey: string): Promise<void>;
    create(options?: AccountOptions): Promise<NostrAccount>;
    import(options: ImportOptions): Promise<NostrAccount>;
  };
}
```

### 2. window.blossom API

```typescript
interface WindowBlossom {
  // Upload content
  upload(file: File | Blob | ArrayBuffer): Promise<BlossomUploadResult>;
  uploadMultiple(files: Array<File | Blob>): Promise<BlossomUploadResult[]>;
  
  // Retrieve content
  get(hash: string): Promise<Blob>;
  getUrl(hash: string): string;  // Returns local URL if cached
  
  // Check availability
  has(hash: string): Promise<boolean>;
  hasMultiple(hashes: string[]): Promise<Record<string, boolean>>;
  
  // User's Blossom servers (BUD-03)
  servers: {
    list(): Promise<BlossomServer[]>;
    add(url: string): Promise<void>;
    remove(url: string): Promise<void>;
    test(url: string): Promise<BlossomServerStatus>;
  };
  
  // Local Blossom server
  local: {
    readonly url: string;  // e.g., "http://localhost:8080"
    readonly enabled: boolean;
    readonly storageUsed: number;
    readonly fileCount: number;
    
    // Management
    clear(): Promise<void>;
    prune(olderThan?: Date): Promise<number>; // returns count removed
    setQuota(bytes: number): Promise<void>;
  };
  
  // Mirror content across servers
  mirror(hash: string, servers?: string[]): Promise<MirrorResult>;
  
  // Authorization
  createAuth(params: {
    verb: 'upload' | 'delete' | 'list';
    files?: string[];  // hashes
    expiration?: number;
  }): Promise<NostrEvent>;  // Returns signed kind 24242 event
}

interface BlossomUploadResult {
  hash: string;
  url: string;
  size: number;
  type: string;
  servers: string[];  // Which servers accepted it
}
```

### 3. Implementation Details

```cpp
// browser/nostr_api_extensions.cc
class NostrAPIExtensions : public gin::Wrappable<NostrAPIExtensions> {
 public:
  static gin::WrapperInfo kWrapperInfo;
  
  // Install extended APIs
  static void Install(v8::Local<v8::Object> global,
                     content::RenderFrame* render_frame) {
    v8::Isolate* isolate = global->GetIsolate();
    v8::HandleScope handle_scope(isolate);
    
    // Get or create window.nostr
    v8::Local<v8::Object> nostr;
    if (!global->Get(gin::StringToV8(isolate, "nostr")).ToLocal(&nostr)) {
      nostr = v8::Object::New(isolate);
      global->Set(gin::StringToV8(isolate, "nostr"), nostr);
    }
    
    // Add relay property
    InstallRelayAPI(isolate, nostr, render_frame);
    
    // Add libs property
    InstallLibraries(isolate, nostr);
    
    // Add extended methods
    InstallExtendedMethods(isolate, nostr, render_frame);
    
    // Install window.blossom
    InstallBlossomAPI(isolate, global, render_frame);
  }
  
 private:
  static void InstallRelayAPI(v8::Isolate* isolate,
                             v8::Local<v8::Object> nostr,
                             content::RenderFrame* render_frame) {
    v8::Local<v8::Object> relay = v8::Object::New(isolate);
    
    // URL getter
    relay->SetAccessor(
        gin::StringToV8(isolate, "url"),
        [](v8::Local<v8::String> property,
           const v8::PropertyCallbackInfo<v8::Value>& info) {
          std::string url = GetLocalRelayURL();
          info.GetReturnValue().Set(gin::StringToV8(info.GetIsolate(), url));
        });
    
    // Connected getter
    relay->SetAccessor(
        gin::StringToV8(isolate, "connected"),
        [](v8::Local<v8::String> property,
           const v8::PropertyCallbackInfo<v8::Value>& info) {
          bool connected = IsLocalRelayConnected();
          info.GetReturnValue().Set(v8::Boolean::New(info.GetIsolate(), connected));
        });
    
    // Add methods
    relay->Set(
        gin::StringToV8(isolate, "query"),
        gin::CreateFunctionTemplate(isolate,
            base::BindRepeating(&QueryLocalRelay, render_frame))
            ->GetFunction(isolate->GetCurrentContext()).ToLocalChecked());
    
    nostr->Set(gin::StringToV8(isolate, "relay"), relay);
  }
  
  static void InstallLibraries(v8::Isolate* isolate,
                              v8::Local<v8::Object> nostr) {
    v8::Local<v8::Object> libs = v8::Object::New(isolate);
    
    // Set paths to bundled libraries
    libs->Set(gin::StringToV8(isolate, "ndk"),
             gin::StringToV8(isolate, "chrome://resources/js/nostr/ndk.js"));
    
    libs->Set(gin::StringToV8(isolate, "nostrTools"),
             gin::StringToV8(isolate, "chrome://resources/js/nostr/nostr-tools.js"));
    
    libs->Set(gin::StringToV8(isolate, "applesauce"),
             gin::StringToV8(isolate, "chrome://resources/js/nostr/applesauce.js"));
             
    libs->Set(gin::StringToV8(isolate, "nostrify"),
             gin::StringToV8(isolate, "chrome://resources/js/nostr/nostrify.js"));
             
    libs->Set(gin::StringToV8(isolate, "alby"),
             gin::StringToV8(isolate, "chrome://resources/js/nostr/alby-sdk.js"));
    
    nostr->Set(gin::StringToV8(isolate, "libs"), libs);
  }
};
```

### 4. Robust Preferences System

```cpp
// Preference definitions for all Nostr/Blossom features
namespace prefs {

// Account Management
const char kNostrAccounts[] = "nostr.accounts";
const char kNostrActiveAccount[] = "nostr.active_account";
const char kNostrAccountSettings[] = "nostr.account_settings";

// NIP-07 Permissions (per-origin, per-account)
const char kNIP07Permissions[] = "nostr.nip07.permissions";
const char kNIP07DefaultPolicy[] = "nostr.nip07.default_policy";
const char kNIP07RememberDecisions[] = "nostr.nip07.remember_decisions";
const char kNIP07RequirePassword[] = "nostr.nip07.require_password";
const char kNIP07RateLimits[] = "nostr.nip07.rate_limits";

// Local Relay Settings
const char kLocalRelayEnabled[] = "nostr.local_relay.enabled";
const char kLocalRelayPort[] = "nostr.local_relay.port";
const char kLocalRelayInterface[] = "nostr.local_relay.interface";
const char kLocalRelayMaxConnections[] = "nostr.local_relay.max_connections";
const char kLocalRelayMaxEvents[] = "nostr.local_relay.max_events";
const char kLocalRelayMaxStorageGB[] = "nostr.local_relay.max_storage_gb";
const char kLocalRelayRetentionDays[] = "nostr.local_relay.retention_days";
const char kLocalRelayAllowedOrigins[] = "nostr.local_relay.allowed_origins";
const char kLocalRelayIndexes[] = "nostr.local_relay.indexes";

// Blossom Settings
const char kBlossomEnabled[] = "blossom.enabled";
const char kBlossomLocalServerEnabled[] = "blossom.local_server.enabled";
const char kBlossomLocalServerPort[] = "blossom.local_server.port";
const char kBlossomMaxStorageGB[] = "blossom.max_storage_gb";
const char kBlossomAllowedMimeTypes[] = "blossom.allowed_mime_types";
const char kBlossomAutoMirror[] = "blossom.auto_mirror";
const char kBlossomServers[] = "blossom.servers";
const char kBlossomUploadDefaults[] = "blossom.upload_defaults";

// Library Loading
const char kNostrLibsEnabled[] = "nostr.libs.enabled";
const char kNostrLibsWhitelist[] = "nostr.libs.whitelist";
const char kNostrLibsVersions[] = "nostr.libs.versions";

// Security Settings
const char kNostrKeyTimeout[] = "nostr.security.key_timeout";
const char kNostrKeyRequireAuth[] = "nostr.security.require_auth";
const char kNostrKeyAuthMethod[] = "nostr.security.auth_method";
const char kNostrAuditLog[] = "nostr.security.audit_log";

// Network Settings
const char kNostrDefaultRelays[] = "nostr.network.default_relays";
const char kNostrRelaySettings[] = "nostr.network.relay_settings";
const char kNostrProxySettings[] = "nostr.network.proxy";
const char kNostrWebRTCPolicy[] = "nostr.network.webrtc_policy";

// Developer Settings
const char kNostrDevMode[] = "nostr.dev.enabled";
const char kNostrDevTools[] = "nostr.dev.tools";
const char kNostrDebugLogging[] = "nostr.dev.debug_logging";

}  // namespace prefs
```

### 5. Preferences UI Schema

```json
{
  "nostr_preferences": {
    "accounts": {
      "type": "array",
      "items": {
        "pubkey": "string",
        "name": "string",
        "nip05": "string",
        "created_at": "timestamp",
        "settings": {
          "default_relays": ["string"],
          "auto_publish_kind_0": "boolean",
          "sign_events_with_password": "boolean"
        }
      }
    },
    
    "nip07_permissions": {
      "type": "object",
      "per_origin": {
        "[origin]": {
          "policy": "allow|deny|ask",
          "exceptions": {
            "getPublicKey": "allow|deny|ask",
            "signEvent": {
              "policy": "allow|deny|ask",
              "kinds": {
                "[kind]": "allow|deny|ask"
              }
            },
            "nip04": "allow|deny|ask"
          },
          "rate_limits": {
            "requests_per_minute": "number",
            "signs_per_hour": "number"
          },
          "remember_until": "timestamp"
        }
      }
    },
    
    "local_relay": {
      "enabled": "boolean",
      "settings": {
        "port": "number",
        "interface": "127.0.0.1|0.0.0.0",
        "storage": {
          "max_gb": "number",
          "max_events": "number",
          "retention_days": "number"
        },
        "filters": {
          "allowed_kinds": ["number"],
          "blocked_pubkeys": ["string"],
          "require_pow": "number"
        },
        "access_control": {
          "allow_external": "boolean",
          "allowed_origins": ["string"],
          "require_auth": "boolean"
        }
      }
    },
    
    "blossom": {
      "enabled": "boolean",
      "local_server": {
        "enabled": "boolean",
        "port": "number",
        "max_file_size_mb": "number",
        "allowed_types": ["string"]
      },
      "remote_servers": {
        "[server_url]": {
          "name": "string",
          "auto_upload": "boolean",
          "max_file_size": "number"
        }
      },
      "policies": {
        "auto_mirror": "boolean",
        "mirror_threshold_mb": "number",
        "compression": "boolean",
        "encrypt_uploads": "boolean"
      }
    }
  }
}
```

### 6. Settings UI Implementation

```typescript
// Settings page for comprehensive Nostr/Blossom configuration
class NostrSettingsPage extends React.Component {
  render() {
    return (
      <SettingsSection title="Nostr & Blossom Settings">
        {/* Account Management */}
        <AccountsSection />
        
        {/* NIP-07 Permissions */}
        <PermissionsSection />
        
        {/* Local Relay Configuration */}
        <LocalRelaySection />
        
        {/* Blossom Settings */}
        <BlossomSection />
        
        {/* Developer Tools */}
        <DeveloperSection />
        
        {/* Import/Export Settings */}
        <SettingsPortability />
      </SettingsSection>
    );
  }
}

// Granular permission controls
class PermissionsSection extends React.Component {
  renderOriginPermissions(origin: string, permissions: OriginPermissions) {
    return (
      <div className="origin-permissions">
        <h4>{origin}</h4>
        
        <PermissionToggle
          label="Get Public Key"
          value={permissions.getPublicKey}
          onChange={(value) => this.updatePermission(origin, 'getPublicKey', value)}
        />
        
        <div className="sign-event-permissions">
          <h5>Event Signing</h5>
          <PermissionToggle
            label="All Events"
            value={permissions.signEvent.policy}
            onChange={(value) => this.updatePermission(origin, 'signEvent.policy', value)}
          />
          
          {/* Per-kind permissions */}
          <details>
            <summary>Advanced: Per-Kind Permissions</summary>
            {KNOWN_KINDS.map(kind => (
              <PermissionToggle
                key={kind.id}
                label={`Kind ${kind.id}: ${kind.name}`}
                value={permissions.signEvent.kinds[kind.id]}
                onChange={(value) => this.updateKindPermission(origin, kind.id, value)}
              />
            ))}
          </details>
        </div>
        
        <RateLimitSettings
          limits={permissions.rateLimits}
          onChange={(limits) => this.updateRateLimits(origin, limits)}
        />
      </div>
    );
  }
}
```

### 7. Advanced Local Relay Configuration

```typescript
interface LocalRelayAdvancedSettings {
  // Performance
  performance: {
    maxConnections: number;
    connectionTimeout: number;
    messageQueueSize: number;
    workerThreads: number;
  };
  
  // Storage Management
  storage: {
    maxSizeGB: number;
    maxEvents: number;
    retentionPolicy: {
      defaultDays: number;
      perKind: Record<number, number>;
    };
    compression: boolean;
    indexes: {
      author: boolean;
      kind: boolean;
      tags: boolean;
      fullText: boolean;
    };
  };
  
  // Filtering
  filtering: {
    allowedKinds: number[] | null;
    blockedKinds: number[];
    blockedPubkeys: string[];
    requiredPow: number;
    maxEventSize: number;
    rateLimits: {
      eventsPerMinute: number;
      subscriptionsPerConnection: number;
    };
  };
  
  // Access Control
  access: {
    allowExternal: boolean;
    allowedOrigins: string[];
    requireAuth: boolean;
    authMethods: ('nip42' | 'password' | 'clientCert')[];
    ipWhitelist: string[];
    corsEnabled: boolean;
  };
}
```

### 8. Blossom Advanced Configuration

```typescript
interface BlossomAdvancedSettings {
  // Storage Policies
  storage: {
    sharding: {
      enabled: boolean;
      shardSize: number;
    };
    deduplication: boolean;
    compression: {
      enabled: boolean;
      algorithms: ('gzip' | 'brotli' | 'zstd')[];
      threshold: number;  // Min size to compress
    };
  };
  
  // Upload Policies
  upload: {
    maxFileSizeMB: number;
    allowedMimeTypes: string[];
    virusScan: boolean;
    autoThumbnails: {
      enabled: boolean;
      sizes: number[];
      formats: ('webp' | 'jpeg' | 'png')[];
    };
  };
  
  // Mirroring
  mirroring: {
    auto: boolean;
    servers: string[];
    redundancy: number;  // How many copies
    verifyUploads: boolean;
    retryFailures: boolean;
  };
  
  // CDN Integration
  cdn: {
    enabled: boolean;
    providers: {
      cloudflare?: CloudflareConfig;
      fastly?: FastlyConfig;
      custom?: CustomCDNConfig;
    };
    cacheHeaders: Record<string, string>;
  };
}
```

### 9. Library Usage Examples

```javascript
// How developers use the bundled libraries
async function loadNostrLibraries() {
  // Dynamic import using the provided paths
  const NDK = await import(window.nostr.libs.ndk);
  const { SimplePool, nip19 } = await import(window.nostr.libs.nostrTools);
  const Applesauce = await import(window.nostr.libs.applesauce);
  
  // Use the libraries
  const ndk = new NDK.NDK({
    explicitRelayUrls: ["wss://relay.damus.io", "wss://nos.lol"]
  });
  
  await ndk.connect();
  
  // Or use local relay directly
  const localNDK = new NDK.NDK({
    explicitRelayUrls: [window.nostr.relay.url]
  });
}

// Alternative: direct script tag (for older code)
function loadViaScriptTag() {
  const script = document.createElement('script');
  script.type = 'module';
  script.textContent = `
    import NDK from '${window.nostr.libs.ndk}';
    window.NDK = NDK;
  `;
  document.head.appendChild(script);
}
```

### 10. Chrome Resource Handler

```cpp
// chrome/browser/resources/nostr_resource_handler.cc
// Handles serving the bundled libraries at chrome:// URLs

class NostrResourceHandler : public content::URLDataSource {
 public:
  std::string GetSource() override {
    return "resources/js/nostr";
  }
  
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override {
    std::string path = url.path();
    
    // Map paths to resource IDs
    int resource_id = 0;
    if (path == "/ndk.js") {
      resource_id = IDR_NOSTR_NDK_JS;
    } else if (path == "/nostr-tools.js") {
      resource_id = IDR_NOSTR_TOOLS_JS;
    } else if (path == "/applesauce.js") {
      resource_id = IDR_APPLESAUCE_JS;
    } else if (path == "/nostrify.js") {
      resource_id = IDR_NOSTRIFY_JS;
    } else if (path == "/alby-sdk.js") {
      resource_id = IDR_ALBY_SDK_JS;
    }
    
    if (resource_id == 0) {
      std::move(callback).Run(nullptr);
      return;
    }
    
    // Load the resource
    scoped_refptr<base::RefCountedMemory> data =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
            resource_id);
    
    std::move(callback).Run(data);
  }
  
  std::string GetMimeType(const GURL& url) override {
    return "application/javascript";
  }
  
  bool ShouldServeMimeTypeAsContentTypeHeader() override {
    return true;
  }
  
  // Allow cross-origin requests from any origin
  void AddResponseHeaders(const GURL& url,
                         net::HttpResponseHeaders* headers) override {
    headers->AddHeader("Access-Control-Allow-Origin", "*");
    headers->AddHeader("Cache-Control", "public, max-age=31536000");
  }
};

// Registration in browser process
void RegisterNostrResources() {
  content::URLDataSource::Add(
      ProfileManager::GetActiveUserProfile(),
      std::make_unique<NostrResourceHandler>());
}
```

### 11. Library Build Configuration

```python
# BUILD.gn configuration for bundling libraries
import("//build/config/features.gni")

# Download and bundle Nostr libraries
action("bundle_nostr_libraries") {
  script = "//chrome/browser/resources/nostr/bundle_libraries.py"
  
  sources = [
    "//third_party/nostr/ndk-2.0.0.js",
    "//third_party/nostr/nostr-tools-1.17.0.js",
    "//third_party/nostr/applesauce-0.5.0.js",
    "//third_party/nostr/nostrify-1.2.0.js",
    "//third_party/nostr/alby-sdk-3.0.0.js",
  ]
  
  outputs = [
    "$target_gen_dir/ndk.js",
    "$target_gen_dir/nostr-tools.js",
    "$target_gen_dir/applesauce.js",
    "$target_gen_dir/nostrify.js",
    "$target_gen_dir/alby-sdk.js",
  ]
  
  args = [
    "--optimize",
    "--source-map",
    "--output-dir", rebase_path(target_gen_dir, root_build_dir),
  ]
}

grit("nostr_resources") {
  source = "nostr_resources.grd"
  outputs = [
    "grit/nostr_resources.h",
    "nostr_resources.pak",
  ]
  
  deps = [ ":bundle_nostr_libraries" ]
}
```

### 12. Library Management Settings

```typescript
class LibraryManager {
  // Configure which libraries are available
  async configureLibraries(config: LibraryConfig) {
    const available = {
      ndk: {
        version: '2.0.0',
        size: 245 * 1024,  // 245KB
        path: 'chrome://resources/js/nostr/ndk.js',
        hash: 'sha256-abcdef123456...'
      },
      nostrTools: {
        version: '1.17.0',
        size: 89 * 1024,
        path: 'chrome://resources/js/nostr/nostr-tools.js',
        hash: 'sha256-fedcba654321...'
      },
      applesauce: {
        version: '0.5.0',
        size: 156 * 1024,
        path: 'chrome://resources/js/nostr/applesauce.js',
        hash: 'sha256-123456abcdef...'
      }
    };
    
    // Allow users to disable specific libraries
    for (const [name, enabled] of Object.entries(config.enabled)) {
      if (!enabled) {
        delete window.nostr.libs[name];
      }
    }
    
    return available;
  }
  
  // Verify library integrity
  async verifyLibrary(name: string): Promise<boolean> {
    const response = await fetch(window.nostr.libs[name]);
    const buffer = await response.arrayBuffer();
    const hash = await crypto.subtle.digest('SHA-256', buffer);
    const hashHex = Array.from(new Uint8Array(hash))
      .map(b => b.toString(16).padStart(2, '0'))
      .join('');
    
    return hashHex === this.expectedHashes[name];
  }
}
```

### 10. Migration and Import/Export

```typescript
class SettingsMigration {
  // Export all Nostr/Blossom settings
  async exportSettings(): Promise<ExportedSettings> {
    const settings = {
      version: '1.0',
      timestamp: Date.now(),
      browser: 'Tungsten',
      data: {
        accounts: await this.exportAccounts(),
        permissions: await this.exportPermissions(),
        localRelay: await this.exportLocalRelaySettings(),
        blossom: await this.exportBlossomSettings(),
        preferences: await this.exportPreferences()
      }
    };
    
    return settings;
  }
  
  // Import settings with validation
  async importSettings(data: ExportedSettings) {
    // Validate version compatibility
    if (!this.isCompatibleVersion(data.version)) {
      throw new Error('Incompatible settings version');
    }
    
    // Import with rollback capability
    const backup = await this.exportSettings();
    
    try {
      await this.importAccounts(data.data.accounts);
      await this.importPermissions(data.data.permissions);
      await this.importLocalRelaySettings(data.data.localRelay);
      await this.importBlossomSettings(data.data.blossom);
      await this.importPreferences(data.data.preferences);
    } catch (error) {
      // Rollback on failure
      await this.importSettings(backup);
      throw error;
    }
  }
}
```

This comprehensive system provides:

1. **Extended Browser APIs**: window.nostr enhancements and new window.blossom API
2. **Pre-loaded Libraries**: Popular Nostr libraries available without CDN requests
3. **Granular Permissions**: Per-origin, per-method, per-kind control
4. **Advanced Configuration**: Deep settings for power users
5. **Import/Export**: Settings portability between devices

### 6. Usage Examples

#### Using Pre-loaded Libraries

```javascript
// Dynamic import of NDK library
const NDK = await import(window.nostr.libs.ndk);
const ndk = new NDK.NDK();

// Import nostr-tools
const NostrTools = await import(window.nostr.libs['nostr-tools']);
const { generatePrivateKey, getPublicKey } = NostrTools;

// Import multiple libraries
const [{ NDK }, { generatePrivateKey }] = await Promise.all([
  import(window.nostr.libs.ndk),
  import(window.nostr.libs['nostr-tools'])
]);

// Check library versions
console.log('NDK version:', window.nostr.libs.versions.ndk); // "2.0.0"
console.log('nostr-tools version:', window.nostr.libs.versions['nostr-tools']); // "1.17.0"

// Use applesauce components
const ApplesauceCore = await import(window.nostr.libs['applesauce-core']);
const ApplesauceContent = await import(window.nostr.libs['applesauce-content']);

// Import Alby SDK
const AlbySDK = await import(window.nostr.libs['alby-sdk']);
```

#### Error Handling

```javascript
try {
  const NDK = await import(window.nostr.libs.ndk);
  // Use NDK...
} catch (error) {
  console.error('Failed to load NDK:', error);
  // Fallback to CDN or handle error
}
```

6. **Performance Controls**: Rate limiting, storage quotas, connection limits
7. **Developer Tools**: Debug modes, logging, testing utilities