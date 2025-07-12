// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)

#include "chrome/browser/nostr/key_storage_mac.h"

#import <Foundation/Foundation.h>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/nostr/mac_keychain_manager.h"
#include "chrome/browser/profiles/profile.h"

namespace nostr {

KeyStorageMac::KeyStorageMac(Profile* profile) 
    : profile_(profile),
      keychain_manager_([[MacKeychainManager alloc] init]) {
  DCHECK(profile_);
}

KeyStorageMac::~KeyStorageMac() {
  [keychain_manager_ release];
}

bool KeyStorageMac::StoreKey(const KeyIdentifier& id,
                            const EncryptedKey& key) {
  if (id.id.empty()) {
    LOG(ERROR) << "Cannot store key with empty ID";
    return false;
  }
  
  @autoreleasepool {
    NSString* service = base::SysUTF8ToNSString(GetServiceName());
    NSString* account = base::SysUTF8ToNSString(GetAccountName(id.id));
    
    // Store the encrypted key data
    auto serialized_key = SerializeEncryptedKey(key);
    NSData* key_data = [NSData dataWithBytes:serialized_key.data()
                                      length:serialized_key.size()];
    
    NSError* error = nil;
    if (![keychain_manager_ storeData:key_data
                           forService:service
                              account:account
                                error:&error]) {
      LOG(ERROR) << "Failed to store key: " 
                 << base::SysNSStringToUTF8([error localizedDescription]);
      return false;
    }
    
    // Store the metadata separately
    NSString* meta_account = base::SysUTF8ToNSString(GetAccountName(id.id + "_meta"));
    auto meta_str = SerializeKeyMetadata(id);
    NSData* meta_data = [NSData dataWithBytes:meta_str.data()
                                       length:meta_str.length()];
    
    if (![keychain_manager_ storeData:meta_data
                           forService:service
                              account:meta_account
                                error:&error]) {
      // Clean up the key if metadata storage fails
      [keychain_manager_ deleteItemForService:service account:account error:nil];
      LOG(ERROR) << "Failed to store metadata: " 
                 << base::SysNSStringToUTF8([error localizedDescription]);
      return false;
    }
    
    // If this is the first key or marked as default, set it as default
    auto keys = ListKeys();
    if (keys.size() == 1 || id.is_default) {
      SetDefaultKey(id.id);
    }
    
    return true;
  }
}

std::optional<EncryptedKey> KeyStorageMac::RetrieveKey(
    const KeyIdentifier& id) {
  @autoreleasepool {
    NSString* service = base::SysUTF8ToNSString(GetServiceName());
    NSString* account = base::SysUTF8ToNSString(GetAccountName(id.id));
    
    NSError* error = nil;
    NSData* data = [keychain_manager_ retrieveDataForService:service
                                                     account:account
                                                       error:&error];
    
    if (!data) {
      if (error) {
        LOG(ERROR) << "Failed to retrieve key: " 
                   << base::SysNSStringToUTF8([error localizedDescription]);
      }
      return std::nullopt;
    }
    
    std::vector<uint8_t> key_data(static_cast<const uint8_t*>([data bytes]),
                                  static_cast<const uint8_t*>([data bytes]) + [data length]);
    
    // Update last used time
    UpdateKeyMetadata(id);
    
    return DeserializeEncryptedKey(key_data);
  }
}

bool KeyStorageMac::DeleteKey(const KeyIdentifier& id) {
  @autoreleasepool {
    NSString* service = base::SysUTF8ToNSString(GetServiceName());
    NSString* account = base::SysUTF8ToNSString(GetAccountName(id.id));
    NSString* meta_account = base::SysUTF8ToNSString(GetAccountName(id.id + "_meta"));
    
    NSError* error = nil;
    bool key_deleted = [keychain_manager_ deleteItemForService:service
                                                       account:account
                                                         error:&error];
    bool meta_deleted = [keychain_manager_ deleteItemForService:service
                                                        account:meta_account
                                                          error:&error];
    
    if (key_deleted && meta_deleted) {
      // If this was the default key, set another as default
      auto default_key = GetDefaultKey();
      if (default_key && default_key->id == id.id) {
        NSString* default_account = base::SysUTF8ToNSString(GetDefaultKeyAccountName());
        [keychain_manager_ deleteItemForService:service account:default_account error:nil];
        
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
}

std::vector<KeyIdentifier> KeyStorageMac::ListKeys() {
  std::vector<KeyIdentifier> result;
  
  @autoreleasepool {
    NSString* service = base::SysUTF8ToNSString(GetServiceName());
    NSError* error = nil;
    NSArray<NSString*>* accounts = [keychain_manager_ listAccountsForService:service
                                                                       error:&error];
    
    if (!accounts) {
      if (error) {
        LOG(ERROR) << "Failed to list keys: " 
                   << base::SysNSStringToUTF8([error localizedDescription]);
      }
      return result;
    }
    
    for (NSString* account in accounts) {
      std::string account_str = base::SysNSStringToUTF8(account);
      
      // Skip metadata and default key entries
      if (account_str.find("_meta") != std::string::npos ||
          account_str == kDefaultKeyAccount) {
        continue;
      }
      
      // Extract key ID from account name
      std::string key_id = account_str;
      
      // Load metadata for this key
      NSString* meta_account = base::SysUTF8ToNSString(GetAccountName(key_id + "_meta"));
      NSData* meta_data = [keychain_manager_ retrieveDataForService:service
                                                           account:meta_account
                                                             error:nil];
      
      if (meta_data) {
        std::string meta_str(static_cast<const char*>([meta_data bytes]),
                            [meta_data length]);
        auto key_info = DeserializeKeyMetadata(meta_str);
        if (key_info) {
          result.push_back(*key_info);
        }
      }
    }
    
    return result;
  }
}

bool KeyStorageMac::UpdateKeyMetadata(const KeyIdentifier& id) {
  @autoreleasepool {
    NSString* service = base::SysUTF8ToNSString(GetServiceName());
    NSString* meta_account = base::SysUTF8ToNSString(GetAccountName(id.id + "_meta"));
    
    // Read existing metadata
    NSError* error = nil;
    NSData* data = [keychain_manager_ retrieveDataForService:service
                                                     account:meta_account
                                                       error:&error];
    
    if (!data) {
      return false;
    }
    
    std::string meta_str(static_cast<const char*>([data bytes]),
                        [data length]);
    auto existing = DeserializeKeyMetadata(meta_str);
    if (!existing) {
      return false;
    }
    
    // Update with new metadata
    KeyIdentifier updated = id;
    updated.created_at = existing->created_at;  // Preserve creation time
    updated.last_used_at = base::Time::Now();   // Update last used
    
    // Save updated metadata
    auto updated_str = SerializeKeyMetadata(updated);
    NSData* updated_data = [NSData dataWithBytes:updated_str.data()
                                         length:updated_str.length()];
    
    return [keychain_manager_ updateData:updated_data
                              forService:service
                                 account:meta_account
                                   error:&error];
  }
}

bool KeyStorageMac::HasKey(const std::string& key_id) {
  @autoreleasepool {
    NSString* service = base::SysUTF8ToNSString(GetServiceName());
    NSString* account = base::SysUTF8ToNSString(GetAccountName(key_id));
    
    return [keychain_manager_ hasItemForService:service account:account];
  }
}

std::optional<KeyIdentifier> KeyStorageMac::GetDefaultKey() {
  @autoreleasepool {
    NSString* service = base::SysUTF8ToNSString(GetServiceName());
    NSString* account = base::SysUTF8ToNSString(GetDefaultKeyAccountName());
    
    NSError* error = nil;
    NSData* data = [keychain_manager_ retrieveDataForService:service
                                                     account:account
                                                       error:&error];
    
    if (!data) {
      return std::nullopt;
    }
    
    std::string default_id(static_cast<const char*>([data bytes]),
                          [data length]);
    
    // Load the metadata for this key
    auto keys = ListKeys();
    for (const auto& key : keys) {
      if (key.id == default_id) {
        return key;
      }
    }
    
    return std::nullopt;
  }
}

bool KeyStorageMac::SetDefaultKey(const std::string& key_id) {
  if (!HasKey(key_id)) {
    LOG(ERROR) << "Cannot set non-existent key as default: " << key_id;
    return false;
  }
  
  @autoreleasepool {
    NSString* service = base::SysUTF8ToNSString(GetServiceName());
    NSString* account = base::SysUTF8ToNSString(GetDefaultKeyAccountName());
    NSData* data = [NSData dataWithBytes:key_id.data() length:key_id.length()];
    
    NSError* error = nil;
    return [keychain_manager_ storeData:data
                             forService:service
                                account:account
                                  error:&error];
  }
}

std::string KeyStorageMac::GetServiceName() const {
  return kServiceName;
}

std::string KeyStorageMac::GetAccountName(const std::string& key_id) const {
  return key_id;
}

std::string KeyStorageMac::GetDefaultKeyAccountName() const {
  return kDefaultKeyAccount;
}

std::vector<uint8_t> KeyStorageMac::SerializeEncryptedKey(
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

std::optional<EncryptedKey> KeyStorageMac::DeserializeEncryptedKey(
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

std::string KeyStorageMac::SerializeKeyMetadata(
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

std::optional<KeyIdentifier> KeyStorageMac::DeserializeKeyMetadata(
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

}  // namespace nostr

#endif  // BUILDFLAG(IS_MAC)