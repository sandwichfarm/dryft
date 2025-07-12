// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/key_storage_in_memory.h"

#include "base/logging.h"
#include "base/time/time.h"

namespace nostr {

KeyStorageInMemory::KeyStorageInMemory() = default;
KeyStorageInMemory::~KeyStorageInMemory() = default;

bool KeyStorageInMemory::StoreKey(const KeyIdentifier& id,
                                  const EncryptedKey& key) {
  if (id.id.empty()) {
    LOG(ERROR) << "Cannot store key with empty ID";
    return false;
  }
  
  keys_[id.id] = key;
  metadata_[id.id] = id;
  
  // If this is the first key, make it default
  if (keys_.size() == 1) {
    default_key_id_ = id.id;
  }
  
  return true;
}

std::optional<EncryptedKey> KeyStorageInMemory::RetrieveKey(
    const KeyIdentifier& id) {
  auto it = keys_.find(id.id);
  if (it == keys_.end()) {
    return std::nullopt;
  }
  
  // Update last used time
  auto meta_it = metadata_.find(id.id);
  if (meta_it != metadata_.end()) {
    meta_it->second.last_used_at = base::Time::Now();
  }
  
  return it->second;
}

bool KeyStorageInMemory::DeleteKey(const KeyIdentifier& id) {
  auto it = keys_.find(id.id);
  if (it == keys_.end()) {
    return false;
  }
  
  keys_.erase(it);
  metadata_.erase(id.id);
  
  // If this was the default key, clear it
  if (default_key_id_ == id.id) {
    default_key_id_.clear();
    
    // Set the first remaining key as default
    if (!keys_.empty()) {
      default_key_id_ = keys_.begin()->first;
    }
  }
  
  return true;
}

std::vector<KeyIdentifier> KeyStorageInMemory::ListKeys() {
  std::vector<KeyIdentifier> result;
  result.reserve(metadata_.size());
  
  for (const auto& [id, metadata] : metadata_) {
    result.push_back(metadata);
  }
  
  return result;
}

bool KeyStorageInMemory::UpdateKeyMetadata(const KeyIdentifier& id) {
  auto it = metadata_.find(id.id);
  if (it == metadata_.end()) {
    return false;
  }
  
  // Update the metadata
  it->second = id;
  return true;
}

bool KeyStorageInMemory::HasKey(const std::string& key_id) {
  return keys_.find(key_id) != keys_.end();
}

std::optional<KeyIdentifier> KeyStorageInMemory::GetDefaultKey() {
  if (default_key_id_.empty()) {
    return std::nullopt;
  }
  
  auto it = metadata_.find(default_key_id_);
  if (it == metadata_.end()) {
    return std::nullopt;
  }
  
  return it->second;
}

bool KeyStorageInMemory::SetDefaultKey(const std::string& key_id) {
  if (!HasKey(key_id)) {
    LOG(ERROR) << "Cannot set non-existent key as default: " << key_id;
    return false;
  }
  
  // Clear previous default flag
  if (!default_key_id_.empty()) {
    auto old_it = metadata_.find(default_key_id_);
    if (old_it != metadata_.end()) {
      old_it->second.is_default = false;
    }
  }
  
  // Set new default
  default_key_id_ = key_id;
  auto new_it = metadata_.find(key_id);
  if (new_it != metadata_.end()) {
    new_it->second.is_default = true;
  }
  
  return true;
}

}  // namespace nostr