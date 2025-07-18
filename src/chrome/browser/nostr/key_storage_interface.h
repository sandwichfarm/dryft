// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_KEY_STORAGE_INTERFACE_H_
#define CHROME_BROWSER_NOSTR_KEY_STORAGE_INTERFACE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"

namespace nostr {

// Represents encrypted key data
struct EncryptedKey {
  // The encrypted private key data
  std::vector<uint8_t> encrypted_data;
  
  // Salt used for key derivation
  std::vector<uint8_t> salt;
  
  // Initialization vector for AES-GCM
  std::vector<uint8_t> iv;
  
  // Authentication tag for AES-GCM
  std::vector<uint8_t> auth_tag;
  
  // Key derivation algorithm (e.g., "PBKDF2-SHA256")
  std::string kdf_algorithm;
  
  // Number of iterations for key derivation
  uint32_t kdf_iterations;
  
  // Encryption algorithm (e.g., "AES-256-GCM")
  std::string encryption_algorithm;
};

// Key identifier and metadata
struct KeyIdentifier {
  // Unique identifier for the key
  std::string id;
  
  // User-friendly name for the key
  std::string name;
  
  // Nostr public key (hex encoded)
  std::string public_key;
  
  // When the key was created
  base::Time created_at;
  
  // When the key was last used
  base::Time last_used_at;
  
  // Associated relays for this key
  std::vector<std::string> relay_urls;
  
  // Whether this is the default key
  bool is_default;
  
  // Key rotation tracking
  std::string rotated_from;    // Previous key this was rotated from
  std::string rotated_to;      // New key this was rotated to
  base::Time rotated_at;       // When rotation occurred
  std::string rotation_reason; // Why the rotation happened
  
  // Usage tracking
  int use_count = 0;           // Number of times key has been used
};

// Abstract interface for platform-specific key storage
class KeyStorage {
 public:
  virtual ~KeyStorage() = default;
  
  // Store an encrypted key with the given identifier
  // Returns true if the key was stored successfully
  virtual bool StoreKey(const KeyIdentifier& id,
                       const EncryptedKey& key) = 0;
  
  // Retrieve an encrypted key by its identifier
  // Returns nullopt if the key is not found
  virtual std::optional<EncryptedKey> RetrieveKey(
      const KeyIdentifier& id) = 0;
  
  // Delete a key by its identifier
  // Returns true if the key was deleted successfully
  virtual bool DeleteKey(const KeyIdentifier& id) = 0;
  
  // List all stored key identifiers
  virtual std::vector<KeyIdentifier> ListKeys() = 0;
  
  // Update key metadata (e.g., last_used_at, name)
  // Returns true if the metadata was updated successfully
  virtual bool UpdateKeyMetadata(const KeyIdentifier& id) = 0;
  
  // Check if a key with the given ID exists
  virtual bool HasKey(const std::string& key_id) = 0;
  
  // Get the default key identifier, if any
  virtual std::optional<KeyIdentifier> GetDefaultKey() = 0;
  
  // Set a key as the default
  virtual bool SetDefaultKey(const std::string& key_id) = 0;
  
  // Get detailed key information by ID
  virtual std::optional<KeyIdentifier> GetKeyInfo(const std::string& key_id) = 0;
  
  // Update detailed key information
  virtual bool UpdateKeyInfo(const std::string& key_id, const KeyIdentifier& info) = 0;
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_KEY_STORAGE_INTERFACE_H_