# Configuration Guide

Complete guide for configuring dryft browser for individual users, enterprises, and developers.

## Quick Navigation

- [User Settings](user-settings.md) - Settings available to end users
- [Enterprise Deployment](enterprise-deployment.md) - IT administrator configuration
- [Policy Templates](policy-templates.md) - Ready-made policy configurations
- [Advanced Configuration](advanced-configuration.md) - Command-line flags and registry settings
- [Performance Tuning](performance-tuning.md) - Optimization for different use cases

## Overview

dryft browser uses a layered configuration system:

1. **Default Settings**: Built-in defaults for all features
2. **User Preferences**: User-configurable settings in browser UI
3. **Policy Settings**: Administrator-defined policies (enterprise)
4. **Command Line**: Launch-time flags for debugging and testing

## Configuration Hierarchy

Settings are applied in this order (later settings override earlier ones):

```
Default Settings (compiled)
    ↓
System Policies (IT admin)
    ↓
User Preferences (end user)
    ↓
Command Line Flags (session)
```

## User Settings Location

### Windows
```
%USERPROFILE%\AppData\Local\dryft\User Data\Default\Preferences
%USERPROFILE%\AppData\Local\dryft\User Data\Default\Local State
```

### macOS
```
~/Library/Application Support/dryft/Default/Preferences
~/Library/Application Support/dryft/Default/Local State
```

### Linux
```
~/.config/dryft/Default/Preferences
~/.config/dryft/Default/Local State
```

## Core Configuration Areas

### 1. Nostr Account Settings

```json
{
  "dryft.nostr.accounts": [
    {
      "pubkey": "a1b2c3d4e5f6...",
      "display_name": "Personal Account",
      "avatar": "https://example.com/avatar.jpg",
      "created": 1672531200,
      "active": true
    }
  ],
  "dryft.nostr.active_account": "a1b2c3d4e5f6...",
  "dryft.nostr.account_defaults": {
    "auto_backup": true,
    "require_passphrase": false,
    "passphrase_timeout": 300
  }
}
```

**Key Settings**:
- `accounts`: Array of configured Nostr accounts
- `active_account`: Currently selected account pubkey
- `account_defaults`: Default settings for new accounts

### 2. Permission Management

```json
{
  "dryft.permissions.nip07": {
    "example.com": {
      "getPublicKey": "allow",
      "signEvent": "ask",
      "getRelays": "allow",
      "nip04.encrypt": "deny",
      "nip04.decrypt": "deny"
    },
    "trusted-app.com": {
      "*": "allow"
    }
  },
  "dryft.permissions.default_policy": "ask",
  "dryft.permissions.remember_duration_days": 30,
  "dryft.permissions.require_password": false
}
```

**Permission Values**:
- `"allow"`: Automatically grant permission
- `"deny"`: Automatically deny permission  
- `"ask"`: Prompt user each time
- `"*"`: Apply to all methods (wildcard)

### 3. Local Services Configuration

```json
{
  "dryft.local_relay.enabled": true,
  "dryft.local_relay.port": 8081,
  "dryft.local_relay.bind_address": "127.0.0.1",
  "dryft.local_relay.max_events": 100000,
  "dryft.local_relay.storage_path": "",
  "dryft.local_relay.sync_enabled": true,
  "dryft.local_relay.sync_relays": [
    "wss://relay.damus.io",
    "wss://nos.lol"
  ],
  
  "dryft.blossom.enabled": true,
  "dryft.blossom.port": 8080,
  "dryft.blossom.bind_address": "127.0.0.1",
  "dryft.blossom.storage_path": "",
  "dryft.blossom.max_file_size": 104857600,
  "dryft.blossom.max_storage": 10737418240,
  "dryft.blossom.cleanup_policy": "lru"
}
```

### 4. Security Settings

```json
{
  "dryft.security.key_storage_backend": "platform",
  "dryft.security.require_secure_context": true,
  "dryft.security.allow_insecure_origins": [],
  "dryft.security.csp_enforcement": "strict",
  "dryft.security.rate_limits": {
    "getPublicKey": { "requests": 10, "window": 60 },
    "signEvent": { "requests": 100, "window": 60 },
    "blossom.upload": { "requests": 50, "window": 300 }
  }
}
```

## Configuration Methods

### 1. Browser Settings UI

Most settings can be configured through the browser interface:

1. **Open Settings**: Menu (⋮) → Settings
2. **Nostr Section**: Configure accounts, permissions, and relay settings
3. **Privacy & Security**: Security and permission settings
4. **Advanced**: Developer and performance options

### 2. Enterprise Policies (Windows Registry)

For enterprise deployment, use Windows Registry or Group Policy:

```registry
[HKEY_LOCAL_MACHINE\SOFTWARE\Policies\dryft]
"NostrAccountCreationAllowed"=dword:00000001
"DefaultRelayList"="wss://corporate-relay.company.com"
"LocalRelayEnabled"=dword:00000001
"BlossomEnabled"=dword:00000000
"RequireKeyPassphrase"=dword:00000001

[HKEY_LOCAL_MACHINE\SOFTWARE\Policies\dryft\PermissionDefaults]
"corporate-app.company.com"="allow"
"*.company.com"="ask"
"*"="deny"
```

### 3. macOS Configuration Profiles

For macOS enterprise deployment:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<plist version="1.0">
<dict>
    <key>PayloadContent</key>
    <array>
        <dict>
            <key>PayloadType</key>
            <string>com.dryft.browser</string>
            <key>NostrAccountCreationAllowed</key>
            <true/>
            <key>LocalRelayEnabled</key>
            <true/>
            <key>BlossomEnabled</key>
            <false/>
            <key>DefaultRelayList</key>
            <array>
                <string>wss://corporate-relay.company.com</string>
            </array>
        </dict>
    </array>
</dict>
</plist>
```

### 4. Linux Policy Files

For Linux enterprise deployment:

```json
// /etc/dryft/policies/managed/corporate-policy.json
{
  "NostrAccountCreationAllowed": true,
  "LocalRelayEnabled": true,
  "BlossomEnabled": false,
  "DefaultRelayList": ["wss://corporate-relay.company.com"],
  "PermissionDefaults": {
    "corporate-app.company.com": "allow",
    "*.company.com": "ask",
    "*": "deny"
  }
}
```

### 5. Command Line Configuration

For testing and development:

```bash
# Enable debug logging
dryft --enable-logging --v=1 --vmodule=nostr*=2

# Use custom ports
dryft --nostr-relay-port=9081 --blossom-port=9080

# Disable services
dryft --disable-local-relay --disable-blossom

# Use custom data directory
dryft --user-data-dir=/tmp/dryft-test

# Allow insecure origins (development only)
dryft --allow-insecure-nostr-origins=http://localhost:3000
```

## Common Configuration Scenarios

### 1. Personal Use - Maximum Privacy

```json
{
  "dryft.permissions.default_policy": "ask",
  "dryft.permissions.remember_duration_days": 1,
  "dryft.security.require_secure_context": true,
  "dryft.local_relay.sync_enabled": false,
  "dryft.telemetry.enabled": false,
  "dryft.nostr.account_defaults.require_passphrase": true
}
```

### 2. Corporate Environment - Controlled Access

```json
{
  "dryft.permissions.default_policy": "deny",
  "dryft.permissions.nip07": {
    "corporate-app.company.com": { "*": "allow" },
    "*.company.com": { "getPublicKey": "allow", "signEvent": "ask" }
  },
  "dryft.local_relay.sync_relays": ["wss://corporate-relay.company.com"],
  "dryft.blossom.enabled": false,
  "dryft.security.allow_insecure_origins": []
}
```

### 3. Developer Environment - Full Access

```json
{
  "dryft.permissions.default_policy": "allow",
  "dryft.security.allow_insecure_origins": [
    "http://localhost:3000",
    "http://localhost:8000"
  ],
  "dryft.local_relay.enabled": true,
  "dryft.blossom.enabled": true,
  "dryft.dev.debug_logging": true
}
```

### 4. Public Kiosk - Restricted

```json
{
  "dryft.nostr.account_creation_allowed": false,
  "dryft.permissions.default_policy": "deny",
  "dryft.local_relay.enabled": false,
  "dryft.blossom.enabled": false,
  "dryft.accounts.max_accounts": 0
}
```

## Advanced Configuration

### Performance Tuning

For high-performance scenarios:

```json
{
  "dryft.local_relay.max_connections": 1000,
  "dryft.local_relay.query_timeout": 5000,
  "dryft.local_relay.cache_size": 536870912,
  "dryft.blossom.cache_size": 1073741824,
  "dryft.performance.preload_libraries": true,
  "dryft.performance.lazy_load_accounts": false
}
```

### Custom Storage Paths

```json
{
  "dryft.local_relay.storage_path": "/var/lib/dryft/relay",
  "dryft.blossom.storage_path": "/var/lib/dryft/blossom",
  "dryft.accounts.storage_path": "/var/lib/dryft/accounts"
}
```

### Network Configuration

```json
{
  "dryft.network.relay_timeout": 10000,
  "dryft.network.max_concurrent_connections": 20,
  "dryft.network.retry_attempts": 3,
  "dryft.network.user_agent": "TungstenBrowser/1.0",
  "dryft.network.proxy_settings": {
    "type": "socks5",
    "host": "127.0.0.1",
    "port": 9050
  }
}
```

## Validation and Testing

### Validate Configuration

Use dryft's built-in configuration validator:

```bash
dryft --validate-config --config-file=dryft-config.json
```

### Test Configuration

```bash
# Test with specific config
dryft --config-test-mode --user-data-dir=/tmp/test

# Check current settings
dryft --dump-config

# Verify policies are applied
dryft --list-policies
```

### Debug Configuration Issues

```bash
# Enable verbose policy logging
dryft --enable-logging --vmodule=policy*=2

# Show configuration source
dryft --log-config-source

# Reset to defaults
dryft --reset-configuration
```

## Troubleshooting

### Common Issues

**Settings not applying:**
1. Check configuration hierarchy - policies may override user settings
2. Verify JSON syntax in configuration files
3. Ensure proper file permissions
4. Restart browser after policy changes

**Permission errors:**
1. Check `dryft.permissions.nip07` configuration
2. Verify origin matches exactly (including protocol)
3. Clear stored permissions and test again

**Service startup failures:**
1. Check port availability (8080, 8081)
2. Verify bind address configuration
3. Check storage path permissions
4. Review startup logs for detailed errors

### Getting Configuration Help

```bash
# Show all available settings
dryft --help-config

# Show current configuration
dryft --show-config

# Export current settings
dryft --export-config=dryft-backup.json
```

For more help, see the [Troubleshooting Guide](../troubleshooting/README.md).