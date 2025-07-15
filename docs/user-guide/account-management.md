# Account Management

Tungsten Browser supports multiple Nostr accounts with seamless switching and granular privacy controls.

## Overview

Tungsten's account system provides:
- **Multiple Accounts**: Manage different identities for different purposes
- **Secure Storage**: Keys stored in platform-native secure storage
- **Account Switching**: Quick switching between accounts per website
- **Granular Permissions**: Different permissions for each account
- **Import/Export**: Backup and restore account data

## Creating Your First Account

### Method 1: Generate New Account

1. **Open Settings**: Click the menu (⋮) → Settings → Nostr Accounts
2. **Add Account**: Click "Add New Account"
3. **Generate Keys**: Click "Generate New Key Pair"
4. **Account Details**:
   - **Display Name**: Choose a friendly name (e.g., "Personal", "Work")
   - **Avatar**: Optional profile picture URL
   - **About**: Brief description
5. **Save Account**: Click "Create Account"

**Important**: Your private key is shown once. Back it up securely!

### Method 2: Import Existing Account

1. **Open Settings**: Menu → Settings → Nostr Accounts
2. **Add Account**: Click "Add New Account"
3. **Import Option**: Click "Import Existing Account"
4. **Enter Private Key**: Paste your nsec key or hex private key
5. **Account Details**: Add display name and profile information
6. **Save Account**: Click "Import Account"

### Supported Key Formats

Tungsten accepts private keys in multiple formats:

```
# nsec format (bech32)
nsec1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq

# Hex format
a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0

# Mnemonic phrase (12/24 words)
abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about
```

## Managing Multiple Accounts

### Account Switcher

The account switcher appears in the browser toolbar when you have multiple accounts:

1. **Current Account**: Shows the active account's display name and avatar
2. **Switch Accounts**: Click to see all available accounts
3. **Quick Switch**: Click any account to switch immediately
4. **Per-Site Memory**: Tungsten remembers your preferred account per website

### Account Organization

**Recommended Account Structure**:
- **Personal**: Your main identity for social networking
- **Professional**: Work-related identity
- **Anonymous**: For privacy-focused browsing
- **Testing**: For development and experimentation

### Default Account Settings

Configure which account is used by default:

1. **Settings** → **Nostr Accounts** → **Default Behavior**
2. **Options**:
   - **Ask each time**: Prompt for account selection
   - **Use last selected**: Remember last choice per site
   - **Always use**: Specify a default account
   - **Domain-based**: Different defaults per domain

## Account Security

### Private Key Protection

Tungsten stores private keys securely:

**Windows**: Windows Credential Manager
- Keys encrypted with DPAPI
- Requires user login to access
- Isolated per Windows user account

**macOS**: Keychain Services
- Keys stored in user's keychain
- Protected by macOS security policies
- Can require Touch ID/password for access

**Linux**: Secret Service API (GNOME Keyring/KWallet)
- Integration with desktop keyring
- Encrypted with login password
- Falls back to encrypted file storage

### Backup and Recovery

#### Export Account

1. **Settings** → **Nostr Accounts** → Select account
2. **Export Options**:
   - **Private Key Only**: Just the nsec key
   - **Full Account Data**: Keys + profile + settings
   - **QR Code**: For mobile import
3. **Security**: Export is encrypted with a password you choose

#### Backup Best Practices

- **Multiple Copies**: Store backups in different locations
- **Offline Storage**: Keep one copy completely offline
- **Test Recovery**: Verify backups work before you need them
- **Regular Updates**: Backup after significant profile changes

#### Recovery Process

1. **Fresh Installation**: Install Tungsten on new device
2. **Import Account**: Settings → Nostr Accounts → Import
3. **Select Backup**: Choose your backup file or enter private key
4. **Decrypt**: Enter backup password if using encrypted export
5. **Verify**: Confirm account details and test signing

### Account Passphrase Protection

Add extra security with account passphrases:

1. **Settings** → **Nostr Accounts** → **Security**
2. **Enable Passphrase**: Toggle "Require passphrase for signing"
3. **Passphrase Options**:
   - **Per session**: Enter once per browser session
   - **Per operation**: Enter for each signing operation
   - **Time-based**: Re-enter after X minutes of inactivity

## Permission Management

### Per-Account Permissions

Each account has independent permission settings:

1. **Account Settings**: Select account → Permissions
2. **Default Policy**: How to handle new permission requests
   - **Always ask**: Prompt for each operation
   - **Allow all**: Automatically approve (not recommended)
   - **Deny all**: Block all operations
3. **Per-Site Overrides**: Custom permissions for specific websites

### Permission Types

**Signing Permissions**:
- **Profile events** (kind 0): Update profile information
- **Text notes** (kind 1): Post messages and replies
- **Direct messages** (kind 4): Send private messages
- **Reactions** (kind 7): Like and react to posts
- **Custom kinds**: Application-specific event types

**Relay Permissions**:
- **Read relays**: Query your relay list
- **Update relays**: Modify your relay configuration
- **Publish events**: Send events to your relays

**Encryption Permissions**:
- **NIP-04**: Legacy encryption for DMs
- **NIP-44**: Modern encryption standard
- **Key derivation**: Generate conversation keys

### Managing Website Permissions

View and modify permissions per website:

1. **Settings** → **Privacy and Security** → **Site Permissions**
2. **Nostr Permissions**: Click to expand
3. **Per-Site List**: Shows all sites with Nostr permissions
4. **Edit Permissions**: Click any site to modify its permissions
5. **Bulk Actions**: Reset all permissions or export for backup

## Advanced Account Features

### Account Profiles and Metadata

Tungsten supports rich account profiles:

**Profile Information** (NIP-05):
- Display name and bio
- Avatar and banner images
- Contact information
- Custom fields

**Verification** (NIP-05):
- Link accounts to domains
- Verify ownership with DNS records
- Display verification badges

### Relay Management Per Account

Each account can have different relay configurations:

1. **Account Settings** → **Relays**
2. **Read Relays**: Where to fetch your events
3. **Write Relays**: Where to publish your events
4. **Discovery Relays**: For finding new connections
5. **Inbox Relays**: For receiving messages (NIP-65)

### Account Synchronization

Sync account settings across devices:

1. **Settings** → **Sync and Services**
2. **Account Sync**: Enable account synchronization
3. **Sync Options**:
   - **Profile data**: Display name, avatar, bio
   - **Relay lists**: Your relay configuration
   - **Permissions**: Website permission settings
   - **Preferences**: Account-specific browser preferences

**Note**: Private keys are never synced - only metadata and settings.

## Troubleshooting Account Issues

### Can't Access Account

**Symptoms**: Account shows as locked or inaccessible

**Solutions**:
1. **Check OS Authentication**: Ensure you're logged into your OS account
2. **Platform Security**: On macOS, check System Preferences → Security
3. **Keyring Issues**: On Linux, restart GNOME Keyring or KWallet
4. **Reinstall Account**: As last resort, delete and re-import the account

### Permission Errors

**Symptoms**: Websites can't access Nostr functions

**Solutions**:
1. **Check Account Selection**: Ensure correct account is active
2. **Reset Permissions**: Clear site permissions and try again
3. **Check Browser Settings**: Ensure Nostr features are enabled
4. **Site Compatibility**: Verify site supports NIP-07 correctly

### Import/Export Problems

**Symptoms**: Can't import or export account data

**Solutions**:
1. **File Format**: Ensure using supported formats (nsec, hex, mnemonic)
2. **Password Issues**: Double-check backup password
3. **File Corruption**: Try a different backup if available
4. **Browser Permissions**: Ensure Tungsten can access the file system

For more help, see the [Troubleshooting Guide](../troubleshooting/README.md).