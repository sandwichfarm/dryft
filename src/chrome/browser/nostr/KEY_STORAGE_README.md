# Nostr Key Storage Implementation

This directory contains the implementation of secure key storage for Nostr private keys in Tungsten browser.

## Architecture

### Core Components

1. **KeyStorage Interface** (`key_storage_interface.h`)
   - Abstract base class defining the key storage API
   - Platform-agnostic interface for storing/retrieving encrypted keys
   - Supports multiple keys with metadata

2. **KeyEncryption** (`key_encryption.h/cc`)
   - Handles encryption/decryption of private keys
   - Uses AES-256-GCM for encryption
   - PBKDF2-SHA256 for key derivation from passphrase
   - Implements OWASP-compliant security measures

3. **KeyStorageFactory** (`key_storage_factory.h/cc`)
   - Factory pattern for creating platform-specific implementations
   - Automatically selects appropriate backend for the platform
   - Supports fallback to encrypted preferences

### Platform Implementations

- **Windows**: Windows Credential Manager (`key_storage_windows.h/cc`)
  - Uses Windows Credential Management API
  - Stores credentials as CRED_TYPE_GENERIC
  - Persists with CRED_PERSIST_LOCAL_MACHINE
  - Separate credentials for key data and metadata
- **macOS**: macOS Keychain (`key_storage_mac.h/mm`)
  - Uses Security framework Keychain Services API
  - Stores as generic passwords with kSecAttrAccessibleWhenUnlocked
  - Objective-C++ bridge for keychain operations
  - Separate items for key data and metadata
- **Linux**: libsecret/GNOME Keyring (Issue B-5)
- **Fallback**: Encrypted preferences storage
- **Testing**: In-memory storage (`key_storage_in_memory.h/cc`)

## Security Features

1. **Encryption**
   - AES-256-GCM authenticated encryption
   - Random IV and salt for each encryption operation
   - Authentication tag verification prevents tampering

2. **Key Derivation**
   - PBKDF2-SHA256 with 100,000 iterations
   - 32-byte salt for each key
   - Passphrase complexity requirements

3. **Passphrase Requirements**
   - Minimum 12 characters
   - Must contain uppercase, lowercase, and numeric characters
   - Validated before encryption/decryption

4. **Memory Safety**
   - Keys are never stored in plaintext
   - Secure memory handling using Chromium's crypto utilities
   - Authentication prevents memory dump attacks

## Usage Example

```cpp
// Create key storage
auto storage = KeyStorageFactory::CreateKeyStorage(profile);

// Create key identifier
KeyIdentifier key_id;
key_id.id = "user_key_1";
key_id.name = "My Nostr Key";
key_id.public_key = nostr_public_key_hex;
key_id.created_at = base::Time::Now();

// Encrypt and store private key
KeyEncryption encryptor;
auto encrypted = encryptor.EncryptKey(private_key_bytes, passphrase);
if (encrypted) {
  storage->StoreKey(key_id, *encrypted);
}

// Retrieve and decrypt
auto retrieved = storage->RetrieveKey(key_id);
if (retrieved) {
  auto decrypted = encryptor.DecryptKey(*retrieved, passphrase);
  // Use decrypted private key
}
```

## Testing

Run unit tests:
```bash
./out/Default/chrome_test --gtest_filter="KeyEncryptionTest.*"
./out/Default/chrome_test --gtest_filter="KeyStorageIntegrationTest.*"

# Windows-specific tests
./out/Default/chrome_test --gtest_filter="KeyStorageWindowsTest.*"

# macOS-specific tests
./out/Default/chrome_test --gtest_filter="KeyStorageMacTest.*"
```

## Future Improvements

1. Hardware security module (HSM) support
2. Key rotation mechanisms
3. Backup/restore functionality
4. Multi-device sync (encrypted)
5. Biometric authentication integration