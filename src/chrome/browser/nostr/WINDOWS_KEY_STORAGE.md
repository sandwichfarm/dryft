# Windows Key Storage Implementation

This document describes the Windows-specific implementation of Nostr key storage using the Windows Credential Manager.

## Overview

The Windows implementation (`KeyStorageWindows`) uses the Windows Credential Management API to securely store encrypted Nostr private keys. This provides:

- OS-level security with user account isolation
- Persistence across browser restarts
- Protection from other applications
- Integration with Windows security features

## Architecture

### Credential Structure

Each Nostr key is stored using multiple credentials:

1. **Key Credential** (`Tungsten_Nostr_<key_id>`)
   - Contains the encrypted private key data
   - Username: Nostr public key (hex)
   - Blob: Serialized `EncryptedKey` structure as JSON

2. **Metadata Credential** (`Tungsten_Nostr_Meta_<key_id>`)
   - Contains key metadata (name, creation time, relays, etc.)
   - Username: Key name
   - Blob: Serialized `KeyIdentifier` structure as JSON

3. **Default Key Credential** (`Tungsten_Nostr_Default`)
   - Stores the ID of the default key
   - Username: "default"
   - Blob: Key ID string

### Data Serialization

All data is serialized to JSON for storage:
- Binary data (encrypted keys, salts, IVs) is base64-encoded
- Timestamps are stored as JavaScript time (milliseconds since epoch)
- Arrays are stored as JSON arrays

### Security Considerations

1. **Credential Type**: Uses `CRED_TYPE_GENERIC` for application-specific data
2. **Persistence**: Uses `CRED_PERSIST_LOCAL_MACHINE` for machine-wide persistence
3. **Access Control**: Credentials are accessible only to the current user
4. **Encryption**: Keys are already encrypted before storage (double protection)

## API Usage

### Storing a Key

```cpp
KeyStorageWindows storage(profile);
KeyIdentifier id = {...};
EncryptedKey encrypted = {...};

if (storage.StoreKey(id, encrypted)) {
  // Key stored successfully
}
```

### Retrieving a Key

```cpp
auto encrypted = storage.RetrieveKey(id);
if (encrypted) {
  // Decrypt using KeyEncryption class
}
```

### Enumerating Keys

```cpp
auto keys = storage.ListKeys();
for (const auto& key : keys) {
  // Process each key
}
```

## Error Handling

The implementation handles various Windows-specific errors:

- `ERROR_NOT_FOUND`: Credential doesn't exist (normal for new keys)
- `ERROR_ACCESS_DENIED`: Insufficient permissions
- `ERROR_INVALID_PARAMETER`: Invalid credential data

## Testing

Windows-specific tests verify:
- Credential persistence across instances
- Proper cleanup on deletion
- Metadata updates
- Large credential handling
- Error scenarios

## Limitations

1. **Size Limit**: Maximum credential blob size is 512KB
2. **Name Length**: Credential names limited by Windows API
3. **UAC**: May require elevation in some enterprise environments
4. **Domain Policies**: Enterprise policies may restrict credential storage

## Migration

When upgrading from extension-based storage:
1. Export keys from extension
2. Import into Windows Credential Manager
3. Verify all keys are accessible
4. Remove old storage

## Troubleshooting

### Common Issues

1. **Access Denied**
   - Check user permissions
   - Verify no group policies blocking credential access
   - Run as administrator if needed

2. **Keys Not Persisting**
   - Check Event Viewer for credential manager errors
   - Verify sufficient disk space
   - Check credential manager service is running

3. **Performance**
   - Credential enumeration can be slow with many keys
   - Consider caching metadata in memory

### Debug Commands

View stored credentials:
```powershell
cmdkey /list | findstr Tungsten_Nostr
```

Remove all Tungsten credentials (WARNING: deletes all keys):
```powershell
for /f "tokens=1,2 delims= " %a in ('cmdkey /list ^| findstr Tungsten_Nostr') do cmdkey /delete:%b
```