// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_FILE_FALLBACK_STORAGE_H_
#define CHROME_BROWSER_NOSTR_FILE_FALLBACK_STORAGE_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/nostr/key_storage_interface.h"

class Profile;

namespace nostr {

// Fallback key storage using encrypted files for Linux systems without Secret Service
class FileFallbackStorage : public KeyStorage {
 public:
  explicit FileFallbackStorage(Profile* profile);
  ~FileFallbackStorage() override;
  
  // Initialize the storage directory
  bool Initialize();
  
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
  // Storage structure on disk
  struct StorageData {
    std::map<std::string, EncryptedKey> keys;
    std::map<std::string, KeyIdentifier> metadata;
    std::string default_key_id;
  };
  
  // Get storage directory path
  base::FilePath GetStorageDirectory() const;
  
  // Get storage file path
  base::FilePath GetStorageFilePath() const;
  
  // Get lock file path
  base::FilePath GetLockFilePath() const;
  
  // Load data from disk
  bool LoadData();
  
  // Save data to disk
  bool SaveData();
  
  // Encrypt/decrypt storage data
  std::optional<std::vector<uint8_t>> EncryptStorageData(const StorageData& data);
  std::optional<StorageData> DecryptStorageData(const std::vector<uint8_t>& encrypted);
  
  // Get OS user key for encryption
  std::string GetOSUserKey() const;
  
  // Profile for storage location
  Profile* profile_;
  
  // In-memory cache of storage data
  StorageData data_;
  
  // Whether data has been loaded
  bool data_loaded_;
  
  // Lock file descriptor for exclusive access
  int lock_fd_;
  
  DISALLOW_COPY_AND_ASSIGN(FileFallbackStorage);
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_FILE_FALLBACK_STORAGE_H_