// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/file_fallback_storage.h"

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/nostr/key_encryption.h"
#include "chrome/browser/profiles/profile.h"

namespace nostr {

namespace {

// File permissions for storage files (owner read/write only)
constexpr mode_t kStorageFileMode = S_IRUSR | S_IWUSR;

// Directory permissions for storage directory
constexpr mode_t kStorageDirMode = S_IRWXU;

}  // namespace

FileFallbackStorage::FileFallbackStorage(Profile* profile)
    : profile_(profile),
      data_loaded_(false),
      lock_fd_(-1) {
  DCHECK(profile_);
}

FileFallbackStorage::~FileFallbackStorage() {
  if (lock_fd_ != -1) {
    flock(lock_fd_, LOCK_UN);
    close(lock_fd_);
  }
}

bool FileFallbackStorage::Initialize() {
  // Create storage directory if it doesn't exist
  base::FilePath storage_dir = GetStorageDirectory();
  if (!base::DirectoryExists(storage_dir)) {
    if (!base::CreateDirectory(storage_dir)) {
      LOG(ERROR) << "Failed to create storage directory: " << storage_dir;
      return false;
    }
    
    // Set proper permissions
    if (chmod(storage_dir.value().c_str(), kStorageDirMode) != 0) {
      LOG(WARNING) << "Failed to set directory permissions";
    }
  }
  
  // Acquire file lock
  base::FilePath lock_file = GetLockFilePath();
  lock_fd_ = open(lock_file.value().c_str(), O_CREAT | O_RDWR, kStorageFileMode);
  if (lock_fd_ == -1) {
    LOG(ERROR) << "Failed to open lock file: " << lock_file;
    return false;
  }
  
  if (flock(lock_fd_, LOCK_EX | LOCK_NB) != 0) {
    LOG(ERROR) << "Failed to acquire file lock - another instance may be running";
    close(lock_fd_);
    lock_fd_ = -1;
    return false;
  }
  
  // Load existing data
  if (!LoadData()) {
    LOG(WARNING) << "Failed to load existing data, starting with empty storage";
    // Initialize empty data
    data_ = StorageData();
    data_loaded_ = true;
  }
  
  return true;
}

base::FilePath FileFallbackStorage::GetStorageDirectory() const {
  return profile_->GetPath().Append("nostr").Append("keys");
}

base::FilePath FileFallbackStorage::GetStorageFilePath() const {
  return GetStorageDirectory().Append("keys.dat");
}

base::FilePath FileFallbackStorage::GetLockFilePath() const {
  return GetStorageDirectory().Append("keys.lock");
}

bool FileFallbackStorage::LoadData() {
  base::FilePath storage_file = GetStorageFilePath();
  
  if (!base::PathExists(storage_file)) {
    // No existing data file
    data_ = StorageData();
    data_loaded_ = true;
    return true;
  }
  
  std::string encrypted_data;
  if (!base::ReadFileToString(storage_file, &encrypted_data)) {
    LOG(ERROR) << "Failed to read storage file";
    return false;
  }
  
  if (encrypted_data.empty()) {
    // Empty file
    data_ = StorageData();
    data_loaded_ = true;
    return true;
  }
  
  // Decrypt the data
  std::vector<uint8_t> encrypted_bytes(encrypted_data.begin(), encrypted_data.end());
  auto decrypted = DecryptStorageData(encrypted_bytes);
  if (!decrypted) {
    LOG(ERROR) << "Failed to decrypt storage data";
    return false;
  }
  
  data_ = *decrypted;
  data_loaded_ = true;
  return true;
}

bool FileFallbackStorage::SaveData() {
  if (!data_loaded_) {
    LOG(ERROR) << "Cannot save data - not loaded";
    return false;
  }
  
  // Encrypt the data
  auto encrypted_bytes = EncryptStorageData(data_);
  if (!encrypted_bytes) {
    LOG(ERROR) << "Failed to encrypt storage data";
    return false;
  }
  
  std::string encrypted_data(encrypted_bytes->begin(), encrypted_bytes->end());
  
  // Write to temporary file first
  base::FilePath storage_file = GetStorageFilePath();
  base::FilePath temp_file = storage_file.AddExtension(".tmp");
  
  if (!base::WriteFile(temp_file, encrypted_data)) {
    LOG(ERROR) << "Failed to write temporary storage file";
    return false;
  }
  
  // Set proper permissions
  if (chmod(temp_file.value().c_str(), kStorageFileMode) != 0) {
    LOG(WARNING) << "Failed to set file permissions";
  }
  
  // Atomic rename
  if (!base::ReplaceFile(temp_file, storage_file, nullptr)) {
    LOG(ERROR) << "Failed to replace storage file";
    base::DeleteFile(temp_file);
    return false;
  }
  
  return true;
}

std::optional<std::vector<uint8_t>> FileFallbackStorage::EncryptStorageData(
    const StorageData& data) {
  // Serialize to JSON
  base::Value::Dict root;
  
  // Serialize keys
  base::Value::Dict keys_dict;
  for (const auto& [key_id, encrypted_key] : data.keys) {
    base::Value::Dict key_dict;
    key_dict.Set("encrypted_data", base::HexEncode(encrypted_key.encrypted_data));
    key_dict.Set("salt", base::HexEncode(encrypted_key.salt));
    key_dict.Set("iv", base::HexEncode(encrypted_key.iv));
    key_dict.Set("kdf_iterations", static_cast<int>(encrypted_key.kdf_iterations));
    key_dict.Set("algorithm", encrypted_key.algorithm);
    keys_dict.Set(key_id, std::move(key_dict));
  }
  root.Set("keys", std::move(keys_dict));
  
  // Serialize metadata
  base::Value::Dict metadata_dict;
  for (const auto& [key_id, identifier] : data.metadata) {
    base::Value::Dict id_dict;
    id_dict.Set("id", identifier.id);
    id_dict.Set("name", identifier.name);
    id_dict.Set("npub", identifier.npub);
    id_dict.Set("created_at", identifier.created_at.ToJsTimeIgnoringNull());
    if (identifier.last_used.has_value()) {
      id_dict.Set("last_used", identifier.last_used->ToJsTimeIgnoringNull());
    }
    id_dict.Set("is_default", identifier.is_default);
    metadata_dict.Set(key_id, std::move(id_dict));
  }
  root.Set("metadata", std::move(metadata_dict));
  
  // Set default key
  if (!data.default_key_id.empty()) {
    root.Set("default_key_id", data.default_key_id);
  }
  
  std::string json;
  if (!base::JSONWriter::Write(root, &json)) {
    LOG(ERROR) << "Failed to serialize storage data";
    return std::nullopt;
  }
  
  // Encrypt using OS user key
  std::string os_key = GetOSUserKey();
  std::vector<uint8_t> json_bytes(json.begin(), json.end());
  
  return KeyEncryption::EncryptData(json_bytes, os_key);
}

std::optional<FileFallbackStorage::StorageData> FileFallbackStorage::DecryptStorageData(
    const std::vector<uint8_t>& encrypted) {
  // Decrypt using OS user key
  std::string os_key = GetOSUserKey();
  auto decrypted_bytes = KeyEncryption::DecryptData(encrypted, os_key);
  if (!decrypted_bytes) {
    LOG(ERROR) << "Failed to decrypt storage data";
    return std::nullopt;
  }
  
  std::string json(decrypted_bytes->begin(), decrypted_bytes->end());
  
  // Parse JSON
  auto value = base::JSONReader::Read(json);
  if (!value || !value->is_dict()) {
    LOG(ERROR) << "Failed to parse storage JSON";
    return std::nullopt;
  }
  
  const auto& root = value->GetDict();
  StorageData data;
  
  // Parse keys
  auto* keys_dict = root.FindDict("keys");
  if (keys_dict) {
    for (const auto [key_id, key_value] : *keys_dict) {
      if (!key_value.is_dict()) {
        continue;
      }
      
      const auto& key_dict = key_value.GetDict();
      EncryptedKey encrypted_key;
      
      auto* encrypted_hex = key_dict.FindString("encrypted_data");
      auto* salt_hex = key_dict.FindString("salt");
      auto* iv_hex = key_dict.FindString("iv");
      auto iterations = key_dict.FindInt("kdf_iterations");
      auto* algorithm = key_dict.FindString("algorithm");
      
      if (!encrypted_hex || !salt_hex || !iv_hex || !iterations || !algorithm) {
        LOG(WARNING) << "Invalid key data for key: " << key_id;
        continue;
      }
      
      if (!base::HexStringToBytes(*encrypted_hex, &encrypted_key.encrypted_data) ||
          !base::HexStringToBytes(*salt_hex, &encrypted_key.salt) ||
          !base::HexStringToBytes(*iv_hex, &encrypted_key.iv)) {
        LOG(WARNING) << "Invalid hex data for key: " << key_id;
        continue;
      }
      
      encrypted_key.kdf_iterations = *iterations;
      encrypted_key.algorithm = *algorithm;
      
      data.keys[key_id] = encrypted_key;
    }
  }
  
  // Parse metadata
  auto* metadata_dict = root.FindDict("metadata");
  if (metadata_dict) {
    for (const auto [key_id, metadata_value] : *metadata_dict) {
      if (!metadata_value.is_dict()) {
        continue;
      }
      
      const auto& id_dict = metadata_value.GetDict();
      KeyIdentifier identifier;
      
      auto* id = id_dict.FindString("id");
      auto* name = id_dict.FindString("name");
      auto* npub = id_dict.FindString("npub");
      auto created_at = id_dict.FindDouble("created_at");
      auto is_default = id_dict.FindBool("is_default");
      
      if (!id || !name || !npub || !created_at || !is_default) {
        LOG(WARNING) << "Invalid metadata for key: " << key_id;
        continue;
      }
      
      identifier.id = *id;
      identifier.name = *name;
      identifier.npub = *npub;
      identifier.created_at = base::Time::FromJsTime(*created_at);
      identifier.is_default = *is_default;
      
      auto last_used = id_dict.FindDouble("last_used");
      if (last_used) {
        identifier.last_used = base::Time::FromJsTime(*last_used);
      }
      
      data.metadata[key_id] = identifier;
    }
  }
  
  // Parse default key ID
  auto* default_key_id = root.FindString("default_key_id");
  if (default_key_id) {
    data.default_key_id = *default_key_id;
  }
  
  return data;
}

std::string FileFallbackStorage::GetOSUserKey() const {
  // Create a key based on the user's profile path and UID
  std::string profile_path = profile_->GetPath().value();
  std::string uid_str = std::to_string(getuid());
  
  // Combine profile path and UID for the key
  return "tungsten-nostr-" + uid_str + "-" + profile_path;
}

bool FileFallbackStorage::StoreKey(const KeyIdentifier& id,
                                  const EncryptedKey& key) {
  if (!data_loaded_) {
    LOG(ERROR) << "Storage not initialized";
    return false;
  }
  
  data_.keys[id.id] = key;
  data_.metadata[id.id] = id;
  
  return SaveData();
}

std::optional<EncryptedKey> FileFallbackStorage::RetrieveKey(
    const KeyIdentifier& id) {
  if (!data_loaded_) {
    LOG(ERROR) << "Storage not initialized";
    return std::nullopt;
  }
  
  auto it = data_.keys.find(id.id);
  if (it == data_.keys.end()) {
    return std::nullopt;
  }
  
  return it->second;
}

bool FileFallbackStorage::DeleteKey(const KeyIdentifier& id) {
  if (!data_loaded_) {
    LOG(ERROR) << "Storage not initialized";
    return false;
  }
  
  auto key_it = data_.keys.find(id.id);
  auto metadata_it = data_.metadata.find(id.id);
  
  if (key_it == data_.keys.end() && metadata_it == data_.metadata.end()) {
    return false;  // Key doesn't exist
  }
  
  if (key_it != data_.keys.end()) {
    data_.keys.erase(key_it);
  }
  
  if (metadata_it != data_.metadata.end()) {
    data_.metadata.erase(metadata_it);
  }
  
  // Clear default if this was the default key
  if (data_.default_key_id == id.id) {
    data_.default_key_id.clear();
  }
  
  return SaveData();
}

std::vector<KeyIdentifier> FileFallbackStorage::ListKeys() {
  std::vector<KeyIdentifier> keys;
  
  if (!data_loaded_) {
    LOG(ERROR) << "Storage not initialized";
    return keys;
  }
  
  for (const auto& [key_id, identifier] : data_.metadata) {
    keys.push_back(identifier);
  }
  
  return keys;
}

bool FileFallbackStorage::UpdateKeyMetadata(const KeyIdentifier& id) {
  if (!data_loaded_) {
    LOG(ERROR) << "Storage not initialized";
    return false;
  }
  
  // Check if key exists
  if (data_.keys.find(id.id) == data_.keys.end()) {
    return false;
  }
  
  data_.metadata[id.id] = id;
  return SaveData();
}

bool FileFallbackStorage::HasKey(const std::string& key_id) {
  if (!data_loaded_) {
    return false;
  }
  
  return data_.keys.find(key_id) != data_.keys.end();
}

std::optional<KeyIdentifier> FileFallbackStorage::GetDefaultKey() {
  if (!data_loaded_ || data_.default_key_id.empty()) {
    return std::nullopt;
  }
  
  auto it = data_.metadata.find(data_.default_key_id);
  if (it == data_.metadata.end()) {
    // Default key metadata missing, clear the default
    data_.default_key_id.clear();
    SaveData();
    return std::nullopt;
  }
  
  KeyIdentifier result = it->second;
  result.is_default = true;
  return result;
}

bool FileFallbackStorage::SetDefaultKey(const std::string& key_id) {
  if (!data_loaded_) {
    LOG(ERROR) << "Storage not initialized";
    return false;
  }
  
  // Verify the key exists
  if (data_.keys.find(key_id) == data_.keys.end()) {
    return false;
  }
  
  data_.default_key_id = key_id;
  return SaveData();
}

}  // namespace nostr