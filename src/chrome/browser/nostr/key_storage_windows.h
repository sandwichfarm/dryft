// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_KEY_STORAGE_WINDOWS_H_
#define CHROME_BROWSER_NOSTR_KEY_STORAGE_WINDOWS_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#include <wincred.h>
#endif

#include <map>
#include <memory>
#include <string>

#include "chrome/browser/nostr/key_storage_interface.h"

class Profile;

namespace nostr {

// Windows implementation of KeyStorage using Windows Credential Manager
class KeyStorageWindows : public KeyStorage {
 public:
  explicit KeyStorageWindows(Profile* profile);
  ~KeyStorageWindows() override;
  
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
  // Generate target name for credential
  std::wstring GetCredentialTargetName(const std::string& key_id) const;
  
  // Generate metadata target name
  std::wstring GetMetadataTargetName(const std::string& key_id) const;
  
  // Get the default key target name
  std::wstring GetDefaultKeyTargetName() const;
  
  // Serialize/deserialize encrypted key data
  std::vector<uint8_t> SerializeEncryptedKey(const EncryptedKey& key) const;
  std::optional<EncryptedKey> DeserializeEncryptedKey(
      const std::vector<uint8_t>& data) const;
  
  // Serialize/deserialize key metadata
  std::string SerializeKeyMetadata(const KeyIdentifier& id) const;
  std::optional<KeyIdentifier> DeserializeKeyMetadata(
      const std::string& data) const;
  
  // Read credential data
  bool ReadCredential(const std::wstring& target_name,
                     std::vector<uint8_t>* data);
  
  // Write credential data
  bool WriteCredential(const std::wstring& target_name,
                      const std::string& username,
                      const std::vector<uint8_t>& data,
                      const std::string& comment);
  
  // Delete a credential
  bool DeleteCredential(const std::wstring& target_name);
  
  // List all Tungsten Nostr credentials
  std::vector<std::wstring> EnumerateCredentials() const;
  
  // Profile for which we're storing keys
  Profile* profile_;
  
  // Prefix for all credential names
  static constexpr wchar_t kCredentialPrefix[] = L"Tungsten_Nostr_";
  static constexpr wchar_t kMetadataPrefix[] = L"Tungsten_Nostr_Meta_";
  static constexpr wchar_t kDefaultKeyName[] = L"Tungsten_Nostr_Default";
  
  DISALLOW_COPY_AND_ASSIGN(KeyStorageWindows);
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_KEY_STORAGE_WINDOWS_H_