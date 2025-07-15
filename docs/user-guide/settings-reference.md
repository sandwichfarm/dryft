# Settings Reference

Complete reference for all user-configurable settings in Tungsten Browser.

## Overview

Tungsten Browser provides extensive customization options for Nostr features, security, and performance. This guide covers all available settings and their effects.

## Accessing Settings

### Browser Settings UI
1. Click the menu button (⋮) in the toolbar
2. Select "Settings"
3. Navigate to the relevant section

### Settings Sections
- **Nostr Accounts** - Account management and configuration
- **Privacy and Security** - Security settings and permissions
- **Advanced** - Developer and expert options
- **Sync and Services** - Cloud sync and data management

## Nostr Account Settings

### Account Management

**Location**: Settings → Nostr Accounts

**Active Account**
- **Setting**: Default account selection
- **Options**: List of configured accounts
- **Effect**: Sets which account is used by default for new websites

**Account Creation**
- **Setting**: Allow new account creation
- **Default**: Enabled
- **Effect**: Controls whether users can create new Nostr accounts

**Maximum Accounts**
- **Setting**: Maximum number of accounts
- **Default**: 10
- **Range**: 1-50
- **Effect**: Limits total number of accounts to prevent abuse

### Account Security

**Passphrase Protection**
- **Setting**: Require passphrase for signing
- **Default**: Disabled
- **Effect**: Prompts for passphrase before signing events

**Passphrase Timeout** 
- **Setting**: Auto-lock timeout
- **Default**: 5 minutes
- **Range**: 1 minute - 24 hours
- **Effect**: Time before passphrase is required again

**Biometric Unlock**
- **Setting**: Use biometrics when available
- **Default**: Enabled (if supported)
- **Effect**: Allows fingerprint/face unlock for accounts

**Key Storage Backend**
- **Setting**: Where to store private keys
- **Options**: Platform (recommended), File, Memory
- **Default**: Platform
- **Effect**: Controls security level of key storage

### Backup and Recovery

**Automatic Backup**
- **Setting**: Auto-backup account data
- **Default**: Enabled
- **Effect**: Creates encrypted backups of account metadata

**Backup Frequency**
- **Setting**: How often to backup
- **Default**: Weekly
- **Options**: Daily, Weekly, Monthly
- **Effect**: Frequency of automatic backups

**Backup Location**
- **Setting**: Where to store backups
- **Default**: Browser profile directory
- **Effect**: Location of encrypted backup files

## Permission Settings

### Default Permissions

**Location**: Settings → Privacy and Security → Site Permissions → Nostr API

**Default Policy**
- **Setting**: Default response to permission requests
- **Options**: Ask (recommended), Allow, Deny
- **Default**: Ask
- **Effect**: How to handle new permission requests

**Permission Duration**
- **Setting**: How long to remember permissions
- **Default**: 30 days
- **Range**: 1 day - Never expire
- **Effect**: When permissions automatically expire

**Require User Confirmation**
- **Setting**: Always show confirmation dialogs
- **Default**: Enabled for signing operations
- **Effect**: Forces user interaction for security-sensitive operations

### Method-Specific Permissions

**Get Public Key**
- **Setting**: Permission for `getPublicKey()` calls
- **Default**: Ask
- **Effect**: Controls access to user's public key

**Sign Events**
- **Setting**: Permission for `signEvent()` calls  
- **Default**: Ask
- **Effect**: Controls ability to sign Nostr events

**Relay Access**
- **Setting**: Permission for `getRelays()` calls
- **Default**: Ask
- **Effect**: Controls access to user's relay configuration

**Encryption Operations**
- **Setting**: Permission for NIP-04/NIP-44 encryption
- **Default**: Ask
- **Effect**: Controls encryption/decryption operations

### Per-Site Permissions

**Trusted Sites**
- **Setting**: Sites with automatic permissions
- **Default**: Empty
- **Effect**: Sites that don't require permission prompts

**Blocked Sites**
- **Setting**: Sites denied all Nostr access
- **Default**: Empty
- **Effect**: Sites completely blocked from Nostr features

**Permission History**
- **Setting**: View and manage site permissions
- **Effect**: See all granted permissions and modify them

## Local Services Settings

### Local Relay Configuration

**Location**: Settings → Advanced → Local Relay

**Enable Local Relay**
- **Setting**: Run built-in relay server
- **Default**: Enabled
- **Effect**: Starts/stops local WebSocket relay

**Relay Port**
- **Setting**: Port for WebSocket server
- **Default**: 8081
- **Range**: 1024-65535
- **Effect**: Changes local relay URL

**Bind Address**
- **Setting**: Network interface to bind
- **Default**: 127.0.0.1 (localhost only)
- **Options**: 127.0.0.1, 0.0.0.0 (all interfaces)
- **Effect**: Controls network accessibility

**Maximum Events**
- **Setting**: Maximum events to store
- **Default**: 100,000
- **Range**: 1,000 - 10,000,000
- **Effect**: Storage limit for local relay

**Database Path**
- **Setting**: Location of relay database
- **Default**: Browser profile directory
- **Effect**: Where relay data is stored

### Sync Configuration

**Enable Relay Sync**
- **Setting**: Sync with external relays
- **Default**: Enabled
- **Effect**: Automatically fetches events from user's relays

**Sync Frequency**
- **Setting**: How often to sync
- **Default**: Every 5 minutes
- **Range**: 1 minute - 24 hours
- **Effect**: Frequency of background sync

**Sync Relays**
- **Setting**: Which relays to sync from
- **Default**: User's read relays
- **Effect**: Sources for automatic sync

**Max Sync Events**
- **Setting**: Maximum events per sync
- **Default**: 1,000
- **Range**: 100 - 10,000
- **Effect**: Limits sync batch size

### Blossom Server Configuration

**Location**: Settings → Advanced → Blossom Server

**Enable Blossom Server**
- **Setting**: Run built-in file server
- **Default**: Enabled
- **Effect**: Starts/stops local HTTP file server

**Server Port**
- **Setting**: Port for HTTP server
- **Default**: 8080
- **Range**: 1024-65535
- **Effect**: Changes local Blossom URL

**Storage Path**
- **Setting**: Location for file storage
- **Default**: Browser profile directory
- **Effect**: Where uploaded files are stored

**Maximum File Size**
- **Setting**: Largest allowed upload
- **Default**: 100 MB
- **Range**: 1 MB - 1 GB
- **Effect**: Upload size limit

**Maximum Storage**
- **Setting**: Total storage limit
- **Default**: 10 GB
- **Range**: 100 MB - 100 GB
- **Effect**: Overall storage capacity

**Cleanup Policy**
- **Setting**: How to manage storage when full
- **Options**: LRU (least recently used), Size (largest first), Age (oldest first)
- **Default**: LRU
- **Effect**: Which files to delete when space needed

## Privacy Settings

### Data Collection

**Location**: Settings → Privacy and Security → Privacy

**Telemetry Collection**
- **Setting**: Send usage statistics
- **Default**: Disabled
- **Effect**: Whether anonymous usage data is collected

**Crash Reporting**
- **Setting**: Send crash reports
- **Default**: Enabled
- **Effect**: Helps developers fix bugs

**Performance Metrics**
- **Setting**: Collect performance data
- **Default**: Disabled
- **Effect**: Whether performance metrics are tracked

### Network Privacy

**Use Tor/Proxy for Relays**
- **Setting**: Route relay connections through proxy
- **Default**: Disabled
- **Effect**: Enhanced privacy for relay connections

**Rotate Connections**
- **Setting**: Change connection patterns
- **Default**: Disabled
- **Effect**: Makes network analysis harder

**DNS over HTTPS**
- **Setting**: Encrypt DNS requests
- **Default**: Enabled
- **Effect**: Prevents DNS monitoring

### Content Privacy

**Block External Resources in Nsites**
- **Setting**: Prevent external content loading
- **Default**: Enabled
- **Effect**: Blocks tracking and improves privacy

**Disable Referrer Headers**
- **Setting**: Don't send referrer information
- **Default**: Enabled for Nsites
- **Effect**: Prevents referrer tracking

## Security Settings

### Content Security

**Location**: Settings → Privacy and Security → Security

**Content Security Policy**
- **Setting**: CSP enforcement level
- **Options**: Strict (default), Moderate, Permissive
- **Effect**: Controls security restrictions for web content

**Allow Insecure Origins**
- **Setting**: Permit HTTP origins for Nostr
- **Default**: Disabled (HTTPS only)
- **Effect**: Security vs compatibility tradeoff

**Block Mixed Content**
- **Setting**: Prevent HTTP content on HTTPS pages
- **Default**: Enabled
- **Effect**: Prevents downgrade attacks

### Key Security

**Hardware Security Module**
- **Setting**: Use hardware-backed security
- **Default**: Enabled (if available)
- **Effect**: Enhanced key protection

**Key Rotation Period**
- **Setting**: Suggest key rotation frequency
- **Default**: 1 year
- **Range**: 30 days - Never
- **Effect**: Security reminder frequency

**Failed Login Attempts**
- **Setting**: Maximum failed passphrase attempts
- **Default**: 3
- **Range**: 1-10
- **Effect**: Account lockout after failures

## Performance Settings

### General Performance

**Location**: Settings → Advanced → Performance

**Hardware Acceleration**
- **Setting**: Use GPU acceleration
- **Default**: Enabled
- **Effect**: Better performance for graphics

**Preload Libraries**
- **Setting**: Load Nostr libraries at startup
- **Default**: Enabled
- **Effect**: Faster library access vs memory usage

**Background Processing**
- **Setting**: Process events in background
- **Default**: Enabled
- **Effect**: Better responsiveness vs CPU usage

### Cache Settings

**Local Relay Cache Size**
- **Setting**: Memory cache for relay data
- **Default**: 256 MB
- **Range**: 64 MB - 2 GB
- **Effect**: Query performance vs memory usage

**Blossom Cache Size**
- **Setting**: Memory cache for files
- **Default**: 512 MB
- **Range**: 128 MB - 4 GB
- **Effect**: File access speed vs memory usage

**Library Cache**
- **Setting**: Cache imported libraries
- **Default**: Enabled
- **Effect**: Faster imports vs memory usage

### Network Performance

**Maximum Concurrent Connections**
- **Setting**: Parallel relay connections
- **Default**: 10
- **Range**: 1-50
- **Effect**: Sync speed vs resource usage

**Connection Timeout**
- **Setting**: Timeout for relay connections
- **Default**: 30 seconds
- **Range**: 5-120 seconds
- **Effect**: Reliability vs responsiveness

**Retry Attempts**
- **Setting**: Connection retry count
- **Default**: 3
- **Range**: 0-10
- **Effect**: Reliability vs performance

## Developer Settings

### Debug Options

**Location**: Settings → Advanced → Developer

**Debug Mode**
- **Setting**: Enable debug features
- **Default**: Disabled
- **Effect**: Shows debug information and tools

**Verbose Logging**
- **Setting**: Detailed log output
- **Default**: Disabled
- **Effect**: More logging for troubleshooting

**Development Extensions**
- **Setting**: Load unpacked extensions
- **Default**: Disabled
- **Effect**: Allows loading development extensions

### API Settings

**Allow Unsafe Operations**
- **Setting**: Enable experimental APIs
- **Default**: Disabled
- **Effect**: Access to unstable features

**Bypass Rate Limits**
- **Setting**: Disable API rate limiting
- **Default**: Disabled
- **Effect**: Unlimited API calls (development only)

**Mock Services**
- **Setting**: Use mock implementations
- **Default**: Disabled
- **Effect**: Testing without real services

## Import/Export Settings

### Export Settings

**Export All Settings**
- **Action**: Save current configuration
- **Format**: JSON file
- **Contents**: All preferences except private keys

**Export Account Data**
- **Action**: Backup account information
- **Format**: Encrypted file
- **Contents**: Account metadata and settings

**Export Permissions**
- **Action**: Save permission configuration
- **Format**: JSON file
- **Contents**: All site permissions and policies

### Import Settings

**Import Configuration**
- **Action**: Restore settings from file
- **Effect**: Overwrites current settings

**Import Account Data**
- **Action**: Restore accounts from backup
- **Effect**: Adds accounts to current profile

**Merge Permissions**
- **Action**: Add permissions from file
- **Effect**: Combines with existing permissions

## Reset Options

### Selective Reset

**Reset Nostr Settings**
- **Action**: Reset only Nostr-related settings
- **Effect**: Keeps general browser settings

**Reset Permissions**
- **Action**: Clear all site permissions
- **Effect**: All sites will prompt for permissions again

**Reset Local Data**
- **Action**: Clear relay and Blossom data
- **Effect**: Removes all locally stored events and files

### Complete Reset

**Reset to Defaults**
- **Action**: Restore all default settings
- **Effect**: Returns to fresh installation state

**Factory Reset**
- **Action**: Delete all data and settings
- **Effect**: Complete clean slate (requires confirmation)

## Command Line Settings

For advanced users, many settings can be controlled via command line:

```bash
# Enable debug mode
tungsten --enable-nostr-debug

# Custom ports
tungsten --nostr-relay-port=9081 --blossom-port=9080

# Disable services
tungsten --disable-local-relay --disable-blossom

# Custom data directory
tungsten --user-data-dir=/custom/path

# Security settings
tungsten --require-nostr-https-origins

# Performance settings
tungsten --max-relay-connections=20
```

## Enterprise Settings

For enterprise deployments, additional settings are available through Group Policy (Windows) or Configuration Profiles (macOS/Linux):

```json
{
  "NostrAccountCreationAllowed": true,
  "MaxNostrAccounts": 5,
  "DefaultPermissionPolicy": "ask",
  "LocalRelayEnabled": true,
  "BlossomEnabled": false,
  "RequirePassphraseForSigning": true,
  "AllowedOrigins": ["https://*.company.com"],
  "BlockedOrigins": ["*"]
}
```

For complete enterprise configuration, see the [Enterprise Deployment Guide](../configuration/enterprise-deployment.md).