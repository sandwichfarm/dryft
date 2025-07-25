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
%USERPROFILE%\AppData\Local\Tungsten\User Data\Default\Preferences
%USERPROFILE%\AppData\Local\Tungsten\User Data\Default\Local State
```

### macOS
```
~/Library/Application Support/Tungsten/Default/Preferences
~/Library/Application Support/Tungsten/Default/Local State
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
  "tungsten.nostr.accounts": [
    {
      "pubkey": "a1b2c3d4e5f6...",
      "display_name": "Personal Account",
      "avatar": "https://example.com/avatar.jpg",
      "created": 1672531200,
      "active": true
    }
  ],
  "tungsten.nostr.active_account": "a1b2c3d4e5f6...",
  "tungsten.nostr.account_defaults": {
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
  "tungsten.permissions.nip07": {
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
  "tungsten.permissions.default_policy": "ask",
  "tungsten.permissions.remember_duration_days": 30,
  "tungsten.permissions.require_password": false
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
  "tungsten.local_relay.enabled": true,
  "tungsten.local_relay.port": 8081,
  "tungsten.local_relay.bind_address": "127.0.0.1",
  "tungsten.local_relay.max_events": 100000,
  "tungsten.local_relay.storage_path": "",
  "tungsten.local_relay.sync_enabled": true,
  "tungsten.local_relay.sync_relays": [
    "wss://relay.damus.io",
    "wss://nos.lol"
  ],
  
  "tungsten.blossom.enabled": true,
  "tungsten.blossom.port": 8080,
  "tungsten.blossom.bind_address": "127.0.0.1",
  "tungsten.blossom.storage_path": "",
  "tungsten.blossom.max_file_size": 104857600,
  "tungsten.blossom.max_storage": 10737418240,
  "tungsten.blossom.cleanup_policy": "lru"
}
```

### 4. Security Settings

```json
{
  "tungsten.security.key_storage_backend": "platform",
  "tungsten.security.require_secure_context": true,
  "tungsten.security.allow_insecure_origins": [],
  "tungsten.security.csp_enforcement": "strict",
  "tungsten.security.rate_limits": {
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
[HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Tungsten]
"NostrAccountCreationAllowed"=dword:00000001
"DefaultRelayList"="wss://corporate-relay.company.com"
"LocalRelayEnabled"=dword:00000001
"BlossomEnabled"=dword:00000000
"RequireKeyPassphrase"=dword:00000001

[HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Tungsten\PermissionDefaults]
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
            <string>com.tungsten.browser</string>
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
tungsten --enable-logging --v=1 --vmodule=nostr*=2

# Use custom ports
tungsten --nostr-relay-port=9081 --blossom-port=9080

# Disable services
tungsten --disable-local-relay --disable-blossom

# Use custom data directory
tungsten --user-data-dir=/tmp/tungsten-test

# Allow insecure origins (development only)
tungsten --allow-insecure-nostr-origins=http://localhost:3000
```

## Common Configuration Scenarios

### 1. Personal Use - Maximum Privacy

```json
{
  "tungsten.permissions.default_policy": "ask",
  "tungsten.permissions.remember_duration_days": 1,
  "tungsten.security.require_secure_context": true,
  "tungsten.local_relay.sync_enabled": false,
  "tungsten.telemetry.enabled": false,
  "tungsten.nostr.account_defaults.require_passphrase": true
}
```

### 2. Corporate Environment - Controlled Access

```json
{
  "tungsten.permissions.default_policy": "deny",
  "tungsten.permissions.nip07": {
    "corporate-app.company.com": { "*": "allow" },
    "*.company.com": { "getPublicKey": "allow", "signEvent": "ask" }
  },
  "tungsten.local_relay.sync_relays": ["wss://corporate-relay.company.com"],
  "tungsten.blossom.enabled": false,
  "tungsten.security.allow_insecure_origins": []
}
```

### 3. Developer Environment - Full Access

```json
{
  "tungsten.permissions.default_policy": "allow",
  "tungsten.security.allow_insecure_origins": [
    "http://localhost:3000",
    "http://localhost:8000"
  ],
  "tungsten.local_relay.enabled": true,
  "tungsten.blossom.enabled": true,
  "dryft.dev.debug_logging": true
}
```

### 4. Public Kiosk - Restricted

```json
{
  "tungsten.nostr.account_creation_allowed": false,
  "tungsten.permissions.default_policy": "deny",
  "tungsten.local_relay.enabled": false,
  "tungsten.blossom.enabled": false,
  "tungsten.accounts.max_accounts": 0
}
```

## Advanced Configuration

### Performance Tuning

For high-performance scenarios:

```json
{
  "tungsten.local_relay.max_connections": 1000,
  "tungsten.local_relay.query_timeout": 5000,
  "tungsten.local_relay.cache_size": 536870912,
  "tungsten.blossom.cache_size": 1073741824,
  "tungsten.performance.preload_libraries": true,
  "tungsten.performance.lazy_load_accounts": false
}
```

### Custom Storage Paths

```json
{
  "tungsten.local_relay.storage_path": "/var/lib/dryft/relay",
  "tungsten.blossom.storage_path": "/var/lib/dryft/blossom",
  "tungsten.accounts.storage_path": "/var/lib/dryft/accounts"
}
```

### Network Configuration

```json
{
  "tungsten.network.relay_timeout": 10000,
  "tungsten.network.max_concurrent_connections": 20,
  "tungsten.network.retry_attempts": 3,
  "tungsten.network.user_agent": "TungstenBrowser/1.0",
  "tungsten.network.proxy_settings": {
    "type": "socks5",
    "host": "127.0.0.1",
    "port": 9050
  }
}
```

## Validation and Testing

### Validate Configuration

Use Tungsten's built-in configuration validator:

```bash
tungsten --validate-config --config-file=tungsten-config.json
```

### Test Configuration

```bash
# Test with specific config
tungsten --config-test-mode --user-data-dir=/tmp/test

# Check current settings
tungsten --dump-config

# Verify policies are applied
tungsten --list-policies
```

### Debug Configuration Issues

```bash
# Enable verbose policy logging
tungsten --enable-logging --vmodule=policy*=2

# Show configuration source
tungsten --log-config-source

# Reset to defaults
tungsten --reset-configuration
```

## Troubleshooting

### Common Issues

**Settings not applying:**
1. Check configuration hierarchy - policies may override user settings
2. Verify JSON syntax in configuration files
3. Ensure proper file permissions
4. Restart browser after policy changes

**Permission errors:**
1. Check `tungsten.permissions.nip07` configuration
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
tungsten --help-config

# Show current configuration
tungsten --show-config

# Export current settings
tungsten --export-config=tungsten-backup.json
```

For more help, see the [Troubleshooting Guide](../troubleshooting/README.md).