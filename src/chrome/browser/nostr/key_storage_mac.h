// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_KEY_STORAGE_MAC_H_
#define CHROME_BROWSER_NOSTR_KEY_STORAGE_MAC_H_

#include "build/build_config.h"

#include <map>
#include <memory>
#include <string>

#include "chrome/browser/nostr/key_storage_interface.h"

#if BUILDFLAG(IS_MAC)
#ifdef __OBJC__
@class MacKeychainManager;
#else
class MacKeychainManager;
#endif
#endif

class Profile;

namespace nostr {

// macOS implementation of KeyStorage using macOS Keychain Services
class KeyStorageMac : public KeyStorage {
 public:
  explicit KeyStorageMac(Profile* profile);
  ~KeyStorageMac() override;
  
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
  // Generate service name for keychain item
  std::string GetServiceName() const;
  
  // Generate account name for key
  std::string GetAccountName(const std::string& key_id) const;
  
  // Get the default key account name
  std::string GetDefaultKeyAccountName() const;
  
  // Serialize/deserialize encrypted key data
  std::vector<uint8_t> SerializeEncryptedKey(const EncryptedKey& key) const;
  std::optional<EncryptedKey> DeserializeEncryptedKey(
      const std::vector<uint8_t>& data) const;
  
  // Serialize/deserialize key metadata
  std::string SerializeKeyMetadata(const KeyIdentifier& id) const;
  std::optional<KeyIdentifier> DeserializeKeyMetadata(
      const std::string& data) const;
  
  // Profile for which we're storing keys
  Profile* profile_;
  
  // Objective-C bridge to keychain manager
#if BUILDFLAG(IS_MAC)
  MacKeychainManager* keychain_manager_;
#endif
  
  // Service name for all keychain items
  static constexpr char kServiceName[] = "dryft browser - Nostr Keys";
  static constexpr char kDefaultKeyAccount[] = "_default_key";
  
  DISALLOW_COPY_AND_ASSIGN(KeyStorageMac);
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_KEY_STORAGE_MAC_H_