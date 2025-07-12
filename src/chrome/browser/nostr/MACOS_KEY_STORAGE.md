# macOS Key Storage Implementation

This document describes the macOS-specific implementation of Nostr key storage using macOS Keychain Services.

## Overview

The macOS implementation (`KeyStorageMac`) uses the macOS Keychain Services API to securely store encrypted Nostr private keys. This provides:

- OS-level security with user authentication
- Persistence across browser restarts
- Protection via macOS security architecture
- Touch ID support via OS when keychain requires authentication
- Automatic keychain unlocking with user login

## Architecture

### Keychain Item Structure

Each Nostr key is stored using multiple keychain items:

1. **Key Item** (Account: `<key_id>`)
   - Service: "Tungsten Browser - Nostr Keys"
   - Contains: Encrypted private key data (JSON)
   - Access: kSecAttrAccessibleWhenUnlocked

2. **Metadata Item** (Account: `<key_id>_meta`)
   - Service: "Tungsten Browser - Nostr Keys"
   - Contains: Key metadata (name, creation time, relays, etc.)
   - Access: kSecAttrAccessibleWhenUnlocked

3. **Default Key Item** (Account: `_default_key`)
   - Service: "Tungsten Browser - Nostr Keys"
   - Contains: ID of the default key
   - Access: kSecAttrAccessibleWhenUnlocked

### Implementation Details

The implementation uses:
- **Objective-C++ Bridge**: `MacKeychainManager` handles keychain API calls
- **C++ Wrapper**: `KeyStorageMac` implements the `KeyStorage` interface
- **Security Framework**: Modern Keychain Services API (not deprecated SecKeychain)
- **No iCloud Sync**: kSecAttrSynchronizable is set to NO for security

### Data Serialization

All data is serialized to JSON for storage:
- Binary data (encrypted keys, salts, IVs) is base64-encoded
- Timestamps are stored as JavaScript time (milliseconds since epoch)
- Metadata is stored as a separate keychain item for flexibility

### Security Considerations

1. **Access Control**: Uses kSecAttrAccessibleWhenUnlocked
   - Keys only accessible when device is unlocked
   - Provides good balance of security and usability

2. **Keychain Prompts**: First access may prompt user
   - "Tungsten wants to use your confidential information..."
   - User can choose "Always Allow" for convenience

3. **No iCloud Sync**: Explicitly disabled
   - Prevents keys from being uploaded to iCloud
   - Keys remain local to the device

## API Usage

### Storing a Key

```cpp
KeyStorageMac storage(profile);
KeyIdentifier id = {...};
EncryptedKey encrypted = {...};

if (storage.StoreKey(id, encrypted)) {
  // Key stored successfully in keychain
}
```

### Retrieving a Key

```cpp
auto encrypted = storage.RetrieveKey(id);
if (encrypted) {
  // Decrypt using KeyEncryption class
}
```

### Keychain Access

The keychain will automatically prompt for access on first use:
- User sees app name "Tungsten"
- Can choose "Always Allow" to avoid future prompts
- Keys are accessible when macOS is unlocked

## Error Handling

Common macOS keychain errors:

- `errSecAuthFailed` (-25293): User denied keychain access
- `errSecInteractionNotAllowed` (-25308): Keychain is locked
- `errSecItemNotFound` (-25300): Key doesn't exist
- `errSecDuplicateItem` (-25299): Key already exists

## Testing

macOS-specific tests verify:
- Keychain persistence across instances
- Proper cleanup on deletion
- Metadata updates
- Large data handling
- Access control behavior

## Limitations

1. **Keychain Locking**: Keys inaccessible when keychain is locked
2. **User Prompts**: First access requires user approval
3. **Touch ID**: Not directly integrated - macOS handles Touch ID authentication for keychain access when configured by the user
4. **Migration**: No automatic migration from other storage methods

## Migration from Extensions

When migrating from extension-based storage:
1. Export keys from extension
2. Import into Tungsten (will store in keychain)
3. First use will prompt for keychain access
4. Verify all keys are accessible

## Troubleshooting

### Common Issues

1. **"Tungsten wants to access your keychain"**
   - Click "Always Allow" for convenience
   - Or click "Allow" to approve each time

2. **Keys Not Accessible**
   - Ensure macOS is unlocked
   - Check keychain isn't locked (Keychain Access app)
   - Try resetting keychain permissions

3. **Performance**
   - First access may be slow (keychain unlock)
   - Subsequent access is fast

### Debug with Keychain Access App

1. Open `/Applications/Utilities/Keychain Access.app`
2. Search for "Tungsten"
3. Look for items with service "Tungsten Browser - Nostr Keys"
4. Double-click to view details (won't show encrypted data)

### Command Line Debugging

List Tungsten keychain items:
```bash
security find-generic-password -s "Tungsten Browser - Nostr Keys"
```

Delete all Tungsten keys (WARNING: removes all keys):
```bash
security delete-generic-password -s "Tungsten Browser - Nostr Keys"
```

## Security Best Practices

1. **Regular Backups**: Keychain items aren't included in Time Machine by default
2. **Strong Login Password**: Keychain security depends on user's macOS password
3. **FileVault**: Enable for additional disk encryption
4. **Keychain Lock**: Set keychain to lock after inactivity