// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)

#include "chrome/browser/nostr/key_storage_windows.h"

#include <shlwapi.h>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"

namespace nostr {

namespace {

// Maximum credential blob size (512KB should be more than enough)
constexpr DWORD kMaxCredentialBlobSize = 512 * 1024;

}  // namespace

KeyStorageWindows::KeyStorageWindows(Profile* profile) : profile_(profile) {
  DCHECK(profile_);
}

KeyStorageWindows::~KeyStorageWindows() = default;

bool KeyStorageWindows::StoreKey(const KeyIdentifier& id,
                                const EncryptedKey& key) {
  if (id.id.empty()) {
    LOG(ERROR) << "Cannot store key with empty ID";
    return false;
  }
  
  // Store the encrypted key data
  auto target_name = GetCredentialTargetName(id.id);
  auto serialized_key = SerializeEncryptedKey(key);
  
  if (!WriteCredential(target_name, 
                      base::UTF8ToWide(id.public_key),
                      serialized_key,
                      "Tungsten Nostr Key")) {
    LOG(ERROR) << "Failed to store encrypted key";
    return false;
  }
  
  // Store the metadata
  auto meta_target = GetMetadataTargetName(id.id);
  auto meta_data = SerializeKeyMetadata(id);
  std::vector<uint8_t> meta_bytes(meta_data.begin(), meta_data.end());
  
  if (!WriteCredential(meta_target,
                      base::UTF8ToWide(id.name),
                      meta_bytes,
                      "Tungsten Nostr Key Metadata")) {
    // Clean up the key if metadata storage fails
    DeleteCredential(target_name);
    LOG(ERROR) << "Failed to store key metadata";
    return false;
  }
  
  // If this is the first key or marked as default, set it as default
  auto keys = ListKeys();
  if (keys.size() == 1 || id.is_default) {
    SetDefaultKey(id.id);
  }
  
  return true;
}

std::optional<EncryptedKey> KeyStorageWindows::RetrieveKey(
    const KeyIdentifier& id) {
  auto target_name = GetCredentialTargetName(id.id);
  std::vector<uint8_t> data;
  
  if (!ReadCredential(target_name, &data)) {
    return std::nullopt;
  }
  
  // Update last used time in metadata
  UpdateKeyMetadata(id);
  
  return DeserializeEncryptedKey(data);
}

bool KeyStorageWindows::DeleteKey(const KeyIdentifier& id) {
  auto target_name = GetCredentialTargetName(id.id);
  auto meta_target = GetMetadataTargetName(id.id);
  
  bool key_deleted = DeleteCredential(target_name);
  bool meta_deleted = DeleteCredential(meta_target);
  
  if (key_deleted && meta_deleted) {
    // If this was the default key, clear it
    auto default_key = GetDefaultKey();
    if (default_key && default_key->id == id.id) {
      DeleteCredential(GetDefaultKeyTargetName());
      
      // Set another key as default if available
      auto remaining_keys = ListKeys();
      if (!remaining_keys.empty()) {
        SetDefaultKey(remaining_keys[0].id);
      }
    }
    return true;
  }
  
  return false;
}

std::vector<KeyIdentifier> KeyStorageWindows::ListKeys() {
  std::vector<KeyIdentifier> result;
  auto credentials = EnumerateCredentials();
  
  for (const auto& cred_name : credentials) {
    // Skip metadata and default key entries
    if (cred_name.find(kMetadataPrefix) != std::wstring::npos ||
        cred_name == kDefaultKeyName) {
      continue;
    }
    
    // Extract key ID from credential name
    std::wstring prefix = kCredentialPrefix;
    if (cred_name.find(prefix) == 0) {
      std::string key_id = base::WideToUTF8(cred_name.substr(prefix.length()));
      
      // Load metadata for this key
      auto meta_target = GetMetadataTargetName(key_id);
      std::vector<uint8_t> meta_data;
      
      if (ReadCredential(meta_target, &meta_data)) {
        std::string meta_str(meta_data.begin(), meta_data.end());
        auto key_info = DeserializeKeyMetadata(meta_str);
        if (key_info) {
          result.push_back(*key_info);
        }
      }
    }
  }
  
  return result;
}

bool KeyStorageWindows::UpdateKeyMetadata(const KeyIdentifier& id) {
  // Read existing metadata
  auto meta_target = GetMetadataTargetName(id.id);
  std::vector<uint8_t> data;
  
  if (!ReadCredential(meta_target, &data)) {
    return false;
  }
  
  std::string meta_str(data.begin(), data.end());
  auto existing = DeserializeKeyMetadata(meta_str);
  if (!existing) {
    return false;
  }
  
  // Update with new metadata, preserving some fields
  KeyIdentifier updated = id;
  updated.created_at = existing->created_at;  // Preserve creation time
  updated.last_used_at = base::Time::Now();   // Update last used
  
  // Save updated metadata
  auto updated_data = SerializeKeyMetadata(updated);
  std::vector<uint8_t> updated_bytes(updated_data.begin(), updated_data.end());
  
  return WriteCredential(meta_target,
                        base::UTF8ToWide(updated.name),
                        updated_bytes,
                        "Tungsten Nostr Key Metadata");
}

bool KeyStorageWindows::HasKey(const std::string& key_id) {
  auto target_name = GetCredentialTargetName(key_id);
  std::vector<uint8_t> data;
  return ReadCredential(target_name, &data);
}

std::optional<KeyIdentifier> KeyStorageWindows::GetDefaultKey() {
  std::vector<uint8_t> data;
  if (!ReadCredential(GetDefaultKeyTargetName(), &data)) {
    return std::nullopt;
  }
  
  std::string default_id(data.begin(), data.end());
  
  // Load the metadata for this key
  auto keys = ListKeys();
  for (const auto& key : keys) {
    if (key.id == default_id) {
      return key;
    }
  }
  
  return std::nullopt;
}

bool KeyStorageWindows::SetDefaultKey(const std::string& key_id) {
  if (!HasKey(key_id)) {
    LOG(ERROR) << "Cannot set non-existent key as default: " << key_id;
    return false;
  }
  
  std::vector<uint8_t> id_bytes(key_id.begin(), key_id.end());
  return WriteCredential(GetDefaultKeyTargetName(),
                        L"default",
                        id_bytes,
                        "Tungsten Nostr Default Key");
}

std::wstring KeyStorageWindows::GetCredentialTargetName(
    const std::string& key_id) const {
  return kCredentialPrefix + base::UTF8ToWide(key_id);
}

std::wstring KeyStorageWindows::GetMetadataTargetName(
    const std::string& key_id) const {
  return kMetadataPrefix + base::UTF8ToWide(key_id);
}

std::wstring KeyStorageWindows::GetDefaultKeyTargetName() const {
  return kDefaultKeyName;
}

std::vector<uint8_t> KeyStorageWindows::SerializeEncryptedKey(
    const EncryptedKey& key) const {
  base::Value::Dict dict;
  
  // Convert vectors to base64 for JSON storage
  dict.Set("encrypted_data", base::Base64Encode(key.encrypted_data));
  dict.Set("salt", base::Base64Encode(key.salt));
  dict.Set("iv", base::Base64Encode(key.iv));
  dict.Set("auth_tag", base::Base64Encode(key.auth_tag));
  dict.Set("kdf_algorithm", key.kdf_algorithm);
  dict.Set("kdf_iterations", static_cast<int>(key.kdf_iterations));
  dict.Set("encryption_algorithm", key.encryption_algorithm);
  
  std::string json;
  base::JSONWriter::Write(dict, &json);
  
  return std::vector<uint8_t>(json.begin(), json.end());
}

std::optional<EncryptedKey> KeyStorageWindows::DeserializeEncryptedKey(
    const std::vector<uint8_t>& data) const {
  std::string json(data.begin(), data.end());
  auto value = base::JSONReader::Read(json);
  
  if (!value || !value->is_dict()) {
    return std::nullopt;
  }
  
  const base::Value::Dict& dict = value->GetDict();
  EncryptedKey key;
  
  // Decode base64 fields
  auto* encrypted_data = dict.FindString("encrypted_data");
  auto* salt = dict.FindString("salt");
  auto* iv = dict.FindString("iv");
  auto* auth_tag = dict.FindString("auth_tag");
  
  if (!encrypted_data || !salt || !iv || !auth_tag) {
    return std::nullopt;
  }
  
  if (!base::Base64Decode(*encrypted_data, &key.encrypted_data) ||
      !base::Base64Decode(*salt, &key.salt) ||
      !base::Base64Decode(*iv, &key.iv) ||
      !base::Base64Decode(*auth_tag, &key.auth_tag)) {
    return std::nullopt;
  }
  
  // Read other fields
  auto* kdf_algorithm = dict.FindString("kdf_algorithm");
  auto kdf_iterations = dict.FindInt("kdf_iterations");
  auto* encryption_algorithm = dict.FindString("encryption_algorithm");
  
  if (!kdf_algorithm || !kdf_iterations || !encryption_algorithm) {
    return std::nullopt;
  }
  
  key.kdf_algorithm = *kdf_algorithm;
  key.kdf_iterations = *kdf_iterations;
  key.encryption_algorithm = *encryption_algorithm;
  
  return key;
}

std::string KeyStorageWindows::SerializeKeyMetadata(
    const KeyIdentifier& id) const {
  base::Value::Dict dict;
  
  dict.Set("id", id.id);
  dict.Set("name", id.name);
  dict.Set("public_key", id.public_key);
  dict.Set("created_at", id.created_at.ToJsTime());
  dict.Set("last_used_at", id.last_used_at.ToJsTime());
  dict.Set("is_default", id.is_default);
  
  base::Value::List relay_list;
  for (const auto& relay : id.relay_urls) {
    relay_list.Append(relay);
  }
  dict.Set("relay_urls", std::move(relay_list));
  
  std::string json;
  base::JSONWriter::Write(dict, &json);
  return json;
}

std::optional<KeyIdentifier> KeyStorageWindows::DeserializeKeyMetadata(
    const std::string& data) const {
  auto value = base::JSONReader::Read(data);
  
  if (!value || !value->is_dict()) {
    return std::nullopt;
  }
  
  const base::Value::Dict& dict = value->GetDict();
  KeyIdentifier id;
  
  // Read required fields
  auto* id_str = dict.FindString("id");
  auto* name = dict.FindString("name");
  auto* public_key = dict.FindString("public_key");
  
  if (!id_str || !name || !public_key) {
    return std::nullopt;
  }
  
  id.id = *id_str;
  id.name = *name;
  id.public_key = *public_key;
  
  // Read timestamps
  auto created_at = dict.FindDouble("created_at");
  auto last_used_at = dict.FindDouble("last_used_at");
  
  if (created_at) {
    id.created_at = base::Time::FromJsTime(*created_at);
  }
  if (last_used_at) {
    id.last_used_at = base::Time::FromJsTime(*last_used_at);
  }
  
  // Read optional fields
  auto is_default = dict.FindBool("is_default");
  if (is_default) {
    id.is_default = *is_default;
  }
  
  // Read relay URLs
  const base::Value::List* relay_list = dict.FindList("relay_urls");
  if (relay_list) {
    for (const auto& relay : *relay_list) {
      if (relay.is_string()) {
        id.relay_urls.push_back(relay.GetString());
      }
    }
  }
  
  return id;
}

bool KeyStorageWindows::ReadCredential(const std::wstring& target_name,
                                      std::vector<uint8_t>* data) {
  PCREDENTIALW credential = nullptr;
  
  if (!CredReadW(target_name.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
    DWORD error = GetLastError();
    if (error != ERROR_NOT_FOUND) {
      LOG(ERROR) << "Failed to read credential: " << error;
    }
    return false;
  }
  
  if (credential->CredentialBlobSize > 0 && credential->CredentialBlob) {
    data->assign(credential->CredentialBlob,
                 credential->CredentialBlob + credential->CredentialBlobSize);
  }
  
  CredFree(credential);
  return true;
}

bool KeyStorageWindows::WriteCredential(const std::wstring& target_name,
                                       const std::string& username,
                                       const std::vector<uint8_t>& data,
                                       const std::string& comment) {
  if (data.size() > kMaxCredentialBlobSize) {
    LOG(ERROR) << "Credential data too large: " << data.size();
    return false;
  }
  
  CREDENTIALW credential = {0};
  credential.Type = CRED_TYPE_GENERIC;
  credential.TargetName = const_cast<LPWSTR>(target_name.c_str());
  credential.UserName = const_cast<LPWSTR>(username.c_str());
  credential.Comment = const_cast<LPWSTR>(base::UTF8ToWide(comment).c_str());
  credential.CredentialBlobSize = static_cast<DWORD>(data.size());
  credential.CredentialBlob = const_cast<LPBYTE>(data.data());
  credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
  
  if (!CredWriteW(&credential, 0)) {
    DWORD error = GetLastError();
    LOG(ERROR) << "Failed to write credential: " << error;
    return false;
  }
  
  return true;
}

bool KeyStorageWindows::DeleteCredential(const std::wstring& target_name) {
  if (!CredDeleteW(target_name.c_str(), CRED_TYPE_GENERIC, 0)) {
    DWORD error = GetLastError();
    if (error != ERROR_NOT_FOUND) {
      LOG(ERROR) << "Failed to delete credential: " << error;
      return false;
    }
  }
  return true;
}

std::vector<std::wstring> KeyStorageWindows::EnumerateCredentials() const {
  std::vector<std::wstring> result;
  PCREDENTIALW* credentials = nullptr;
  DWORD count = 0;
  
  // Enumerate all generic credentials
  if (!CredEnumerateW(L"Tungsten_Nostr_*", 0, &count, &credentials)) {
    DWORD error = GetLastError();
    if (error != ERROR_NOT_FOUND) {
      LOG(ERROR) << "Failed to enumerate credentials: " << error;
    }
    return result;
  }
  
  for (DWORD i = 0; i < count; ++i) {
    if (credentials[i] && credentials[i]->TargetName) {
      result.push_back(credentials[i]->TargetName);
    }
  }
  
  CredFree(credentials);
  return result;
}

}  // namespace nostr

#endif  // BUILDFLAG(IS_WIN)