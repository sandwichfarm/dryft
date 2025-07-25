// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_KEY_STORAGE_LINUX_H_
#define CHROME_BROWSER_NOSTR_KEY_STORAGE_LINUX_H_

#include "build/build_config.h"

#include <map>
#include <memory>
#include <string>

#include "chrome/browser/nostr/key_storage_interface.h"

class Profile;

namespace nostr {

class SecretServiceClient;
class FileFallbackStorage;

// Linux implementation of KeyStorage using Secret Service API with fallback
class KeyStorageLinux : public KeyStorage {
 public:
  explicit KeyStorageLinux(Profile* profile);
  ~KeyStorageLinux() override;
  
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
  
  // Check if Secret Service is available
  bool IsSecretServiceAvailable() const;
  
 private:
  // Initialize storage backend
  void InitializeStorage();
  
  // Get the active storage backend
  KeyStorage* GetActiveStorage();
  
  // Detect desktop environment
  enum class DesktopEnvironment {
    kUnknown,
    kGnome,
    kKde,
    kXfce,
    kOther
  };
  DesktopEnvironment DetectDesktopEnvironment() const;
  
  // Profile for which we're storing keys
  Profile* profile_;
  
  // Secret Service client (primary storage)
  std::unique_ptr<SecretServiceClient> secret_service_;
  
  // File-based fallback storage
  std::unique_ptr<FileFallbackStorage> file_fallback_;
  
  // Whether to use Secret Service or fallback
  bool use_secret_service_;
  
  // Desktop environment
  DesktopEnvironment desktop_environment_;
  
  DISALLOW_COPY_AND_ASSIGN(KeyStorageLinux);
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_KEY_STORAGE_LINUX_H_