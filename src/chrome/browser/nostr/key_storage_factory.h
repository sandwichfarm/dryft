// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_KEY_STORAGE_FACTORY_H_
#define CHROME_BROWSER_NOSTR_KEY_STORAGE_FACTORY_H_

#include <memory>

#include "chrome/browser/nostr/key_storage_interface.h"

class Profile;

namespace nostr {

// Factory class for creating platform-specific key storage implementations
class KeyStorageFactory {
 public:
  // Storage backend types
  enum class StorageBackend {
    // Platform-specific secure storage
    PLATFORM_DEFAULT,
    
    // Windows: Credential Manager
    WINDOWS_CREDENTIAL_MANAGER,
    
    // macOS: Keychain
    MACOS_KEYCHAIN,
    
    // Linux: libsecret/GNOME Keyring
    LINUX_SECRET_SERVICE,
    
    // Fallback: Encrypted preferences
    ENCRYPTED_PREFERENCES,
    
    // In-memory only (for testing)
    IN_MEMORY
  };
  
  // Create a key storage instance for the given profile
  // Uses the appropriate platform-specific backend
  static std::unique_ptr<KeyStorage> CreateKeyStorage(Profile* profile);
  
  // Create a key storage instance with a specific backend
  // Mainly used for testing or overriding default behavior
  static std::unique_ptr<KeyStorage> CreateKeyStorage(
      Profile* profile,
      StorageBackend backend);
  
  // Get the default storage backend for the current platform
  static StorageBackend GetDefaultBackend();
  
  // Check if a specific backend is available on the current platform
  static bool IsBackendAvailable(StorageBackend backend);
  
  // Get a human-readable name for a storage backend
  static std::string GetBackendName(StorageBackend backend);
  
 private:
  // Platform-specific factory methods
  static std::unique_ptr<KeyStorage> CreateWindowsKeyStorage(Profile* profile);
  static std::unique_ptr<KeyStorage> CreateMacOSKeyStorage(Profile* profile);
  static std::unique_ptr<KeyStorage> CreateLinuxKeyStorage(Profile* profile);
  static std::unique_ptr<KeyStorage> CreateEncryptedPrefsKeyStorage(Profile* profile);
  static std::unique_ptr<KeyStorage> CreateInMemoryKeyStorage();
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_KEY_STORAGE_FACTORY_H_