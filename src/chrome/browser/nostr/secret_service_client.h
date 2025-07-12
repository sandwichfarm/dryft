// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_SECRET_SERVICE_CLIENT_H_
#define CHROME_BROWSER_NOSTR_SECRET_SERVICE_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/nostr/key_storage_interface.h"

namespace nostr {

// Client for Linux Secret Service API (libsecret)
// This provides access to GNOME Keyring, KDE Wallet (via adapter), etc.
class SecretServiceClient : public KeyStorage {
 public:
  SecretServiceClient();
  ~SecretServiceClient() override;
  
  // Initialize the client and check if Secret Service is available
  bool Initialize();
  
  // Check if Secret Service is available
  bool IsAvailable() const { return is_available_; }
  
  // KeyStorage implementation
  bool StoreKey(const KeyIdentifier& id,
                const EncryptedKey& key) override;
  std::optional<EncryptedKey> RetrieveKey(
      const KeyIdentifier& id) override;
  bool DeleteKey(const KeyIdentifier& id) override;
  std::vector<KeyIdentifier> ListKeys() override;
  bool UpdateKeyMetadata(const KeyIdentifier& id) override;
  bool HasKey(const std::string& key_id) override;
  std::optional<KeyIdentifier> GetDefaultKey() override;
  bool SetDefaultKey(const std::string& key_id) override;
  
 private:
  // Schema name for our secrets
  static constexpr char kSchemaName[] = "org.tungsten.NostrKey";
  
  // Attribute names
  static constexpr char kAttrKeyId[] = "key_id";
  static constexpr char kAttrType[] = "type";  // "key", "metadata", or "default"
  
  // Collection name
  static constexpr char kCollectionName[] = "Tungsten Nostr Keys";
  
  // Get label for secret
  std::string GetSecretLabel(const std::string& key_id, const std::string& type) const;
  
  // Serialize/deserialize data
  std::string SerializeEncryptedKey(const EncryptedKey& key) const;
  std::optional<EncryptedKey> DeserializeEncryptedKey(const std::string& data) const;
  std::string SerializeKeyMetadata(const KeyIdentifier& id) const;
  std::optional<KeyIdentifier> DeserializeKeyMetadata(const std::string& data) const;
  
  // Whether Secret Service is available
  bool is_available_;
  
  // Opaque pointer to libsecret schema
  void* secret_schema_;
  
  DISALLOW_COPY_AND_ASSIGN(SecretServiceClient);
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_SECRET_SERVICE_CLIENT_H_