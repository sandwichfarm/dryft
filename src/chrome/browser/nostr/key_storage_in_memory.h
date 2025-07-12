// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_KEY_STORAGE_IN_MEMORY_H_
#define CHROME_BROWSER_NOSTR_KEY_STORAGE_IN_MEMORY_H_

#include <map>
#include <string>

#include "chrome/browser/nostr/key_storage_interface.h"

namespace nostr {

// In-memory implementation of KeyStorage for testing
class KeyStorageInMemory : public KeyStorage {
 public:
  KeyStorageInMemory();
  ~KeyStorageInMemory() override;
  
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
  // Storage for keys and metadata
  std::map<std::string, EncryptedKey> keys_;
  std::map<std::string, KeyIdentifier> metadata_;
  std::string default_key_id_;
  
  DISALLOW_COPY_AND_ASSIGN(KeyStorageInMemory);
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_KEY_STORAGE_IN_MEMORY_H_