// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/secret_service_client.h"

#include <libsecret/secret.h>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace nostr {

namespace {

// Define the secret schema
const SecretSchema* GetSecretSchema() {
  static const SecretSchema schema = {
    SecretServiceClient::kSchemaName,
    SECRET_SCHEMA_NONE,
    {
      { SecretServiceClient::kAttrKeyId, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { SecretServiceClient::kAttrType, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING }
    }
  };
  return &schema;
}

}  // namespace

// Static member definitions
constexpr char SecretServiceClient::kSchemaName[];
constexpr char SecretServiceClient::kAttrKeyId[];
constexpr char SecretServiceClient::kAttrType[];
constexpr char SecretServiceClient::kCollectionName[];

SecretServiceClient::SecretServiceClient() 
    : is_available_(false),
      secret_schema_(nullptr) {
}

SecretServiceClient::~SecretServiceClient() = default;

bool SecretServiceClient::Initialize() {
  // Check if Secret Service is available by trying to get the service
  GError* error = nullptr;
  SecretService* service = secret_service_get_sync(SECRET_SERVICE_NONE, nullptr, &error);
  
  if (error) {
    LOG(WARNING) << "Secret Service not available: " << error->message;
    g_error_free(error);
    return false;
  }
  
  if (!service) {
    LOG(WARNING) << "Secret Service not available";
    return false;
  }
  
  g_object_unref(service);
  
  secret_schema_ = const_cast<void*>(static_cast<const void*>(GetSecretSchema()));
  is_available_ = true;
  LOG(INFO) << "Secret Service initialized successfully";
  return true;
}

bool SecretServiceClient::StoreKey(const KeyIdentifier& id,
                                  const EncryptedKey& key) {
  if (!is_available_) {
    return false;
  }
  
  // Store the encrypted key
  std::string key_data = SerializeEncryptedKey(key);
  std::string key_label = GetSecretLabel(id.id, "key");
  
  GError* error = nullptr;
  gboolean result = secret_password_store_sync(
      GetSecretSchema(),
      kCollectionName,
      key_label.c_str(),
      key_data.c_str(),
      nullptr,  // cancellable
      &error,
      kAttrKeyId, id.id.c_str(),
      kAttrType, "key",
      nullptr);
  
  if (error) {
    LOG(ERROR) << "Failed to store key: " << error->message;
    g_error_free(error);
    return false;
  }
  
  if (!result) {
    LOG(ERROR) << "Failed to store key";
    return false;
  }
  
  // Store the metadata
  std::string metadata_data = SerializeKeyMetadata(id);
  std::string metadata_label = GetSecretLabel(id.id, "metadata");
  
  result = secret_password_store_sync(
      GetSecretSchema(),
      kCollectionName,
      metadata_label.c_str(),
      metadata_data.c_str(),
      nullptr,  // cancellable
      &error,
      kAttrKeyId, id.id.c_str(),
      kAttrType, "metadata",
      nullptr);
  
  if (error) {
    LOG(ERROR) << "Failed to store metadata: " << error->message;
    g_error_free(error);
    // Try to clean up the key we just stored
    secret_password_clear_sync(GetSecretSchema(), nullptr, &error,
                              kAttrKeyId, id.id.c_str(),
                              kAttrType, "key",
                              nullptr);
    if (error) {
      g_error_free(error);
    }
    return false;
  }
  
  return result == TRUE;
}

std::optional<EncryptedKey> SecretServiceClient::RetrieveKey(
    const KeyIdentifier& id) {
  if (!is_available_) {
    return std::nullopt;
  }
  
  GError* error = nullptr;
  gchar* password = secret_password_lookup_sync(
      GetSecretSchema(),
      nullptr,  // cancellable
      &error,
      kAttrKeyId, id.id.c_str(),
      kAttrType, "key",
      nullptr);
  
  if (error) {
    LOG(ERROR) << "Failed to retrieve key: " << error->message;
    g_error_free(error);
    return std::nullopt;
  }
  
  if (!password) {
    return std::nullopt;
  }
  
  std::string key_data(password);
  secret_password_free(password);
  
  return DeserializeEncryptedKey(key_data);
}

bool SecretServiceClient::DeleteKey(const KeyIdentifier& id) {
  if (!is_available_) {
    return false;
  }
  
  // Delete the key
  GError* error = nullptr;
  gboolean result = secret_password_clear_sync(
      GetSecretSchema(),
      nullptr,  // cancellable
      &error,
      kAttrKeyId, id.id.c_str(),
      kAttrType, "key",
      nullptr);
  
  if (error) {
    LOG(ERROR) << "Failed to delete key: " << error->message;
    g_error_free(error);
    return false;
  }
  
  // Delete the metadata
  result = secret_password_clear_sync(
      GetSecretSchema(),
      nullptr,  // cancellable
      &error,
      kAttrKeyId, id.id.c_str(),
      kAttrType, "metadata",
      nullptr);
  
  if (error) {
    LOG(ERROR) << "Failed to delete metadata: " << error->message;
    g_error_free(error);
    // Key was deleted but metadata wasn't - not ideal but not fatal
  }
  
  // If this was the default key, clear the default
  auto default_key = GetDefaultKey();
  if (default_key && default_key->id == id.id) {
    // Clear default key
    secret_password_clear_sync(
        GetSecretSchema(),
        nullptr,  // cancellable
        &error,
        kAttrKeyId, "default",
        kAttrType, "default",
        nullptr);
    if (error) {
      g_error_free(error);
    }
  }
  
  return result == TRUE;
}

std::vector<KeyIdentifier> SecretServiceClient::ListKeys() {
  std::vector<KeyIdentifier> keys;
  
  if (!is_available_) {
    return keys;
  }
  
  GError* error = nullptr;
  GList* items = secret_password_search_sync(
      GetSecretSchema(),
      SECRET_SEARCH_ALL,
      nullptr,  // cancellable
      &error,
      kAttrType, "metadata",
      nullptr);
  
  if (error) {
    LOG(ERROR) << "Failed to list keys: " << error->message;
    g_error_free(error);
    return keys;
  }
  
  for (GList* l = items; l != nullptr; l = l->next) {
    auto* item = static_cast<SecretItem*>(l->data);
    
    // Get the attributes
    GHashTable* attributes = secret_item_get_attributes(item);
    if (!attributes) {
      continue;
    }
    
    auto* key_id = static_cast<const char*>(g_hash_table_lookup(attributes, kAttrKeyId));
    if (!key_id) {
      g_hash_table_unref(attributes);
      continue;
    }
    
    // Load the secret to get metadata
    SecretValue* value = secret_item_get_secret(item);
    if (!value) {
      g_hash_table_unref(attributes);
      continue;
    }
    
    const gchar* secret = secret_value_get_text(value);
    if (secret) {
      auto metadata = DeserializeKeyMetadata(std::string(secret));
      if (metadata) {
        keys.push_back(*metadata);
      }
    }
    
    secret_value_unref(value);
    g_hash_table_unref(attributes);
  }
  
  g_list_free_full(items, g_object_unref);
  
  return keys;
}

bool SecretServiceClient::UpdateKeyMetadata(const KeyIdentifier& id) {
  if (!is_available_) {
    return false;
  }
  
  // Retrieve the existing key to ensure it exists
  auto key = RetrieveKey(id);
  if (!key) {
    return false;
  }
  
  // Update the metadata
  std::string metadata_data = SerializeKeyMetadata(id);
  std::string metadata_label = GetSecretLabel(id.id, "metadata");
  
  GError* error = nullptr;
  gboolean result = secret_password_store_sync(
      GetSecretSchema(),
      kCollectionName,
      metadata_label.c_str(),
      metadata_data.c_str(),
      nullptr,  // cancellable
      &error,
      kAttrKeyId, id.id.c_str(),
      kAttrType, "metadata",
      nullptr);
  
  if (error) {
    LOG(ERROR) << "Failed to update metadata: " << error->message;
    g_error_free(error);
    return false;
  }
  
  return result == TRUE;
}

bool SecretServiceClient::HasKey(const std::string& key_id) {
  if (!is_available_) {
    return false;
  }
  
  GError* error = nullptr;
  gchar* password = secret_password_lookup_sync(
      GetSecretSchema(),
      nullptr,  // cancellable
      &error,
      kAttrKeyId, key_id.c_str(),
      kAttrType, "key",
      nullptr);
  
  if (error) {
    g_error_free(error);
    return false;
  }
  
  if (password) {
    secret_password_free(password);
    return true;
  }
  
  return false;
}

std::optional<KeyIdentifier> SecretServiceClient::GetDefaultKey() {
  if (!is_available_) {
    return std::nullopt;
  }
  
  GError* error = nullptr;
  gchar* default_id = secret_password_lookup_sync(
      GetSecretSchema(),
      nullptr,  // cancellable
      &error,
      kAttrKeyId, "default",
      kAttrType, "default",
      nullptr);
  
  if (error) {
    g_error_free(error);
    return std::nullopt;
  }
  
  if (!default_id) {
    return std::nullopt;
  }
  
  std::string key_id(default_id);
  secret_password_free(default_id);
  
  // Look up the metadata for this key
  gchar* metadata = secret_password_lookup_sync(
      GetSecretSchema(),
      nullptr,  // cancellable
      &error,
      kAttrKeyId, key_id.c_str(),
      kAttrType, "metadata",
      nullptr);
  
  if (error) {
    g_error_free(error);
    return std::nullopt;
  }
  
  if (!metadata) {
    return std::nullopt;
  }
  
  std::string metadata_data(metadata);
  secret_password_free(metadata);
  
  auto result = DeserializeKeyMetadata(metadata_data);
  if (result) {
    result->is_default = true;
  }
  
  return result;
}

bool SecretServiceClient::SetDefaultKey(const std::string& key_id) {
  if (!is_available_) {
    return false;
  }
  
  // Verify the key exists
  if (!HasKey(key_id)) {
    return false;
  }
  
  // Store the default key ID
  GError* error = nullptr;
  gboolean result = secret_password_store_sync(
      GetSecretSchema(),
      kCollectionName,
      "Tungsten Default Nostr Key",
      key_id.c_str(),
      nullptr,  // cancellable
      &error,
      kAttrKeyId, "default",
      kAttrType, "default",
      nullptr);
  
  if (error) {
    LOG(ERROR) << "Failed to set default key: " << error->message;
    g_error_free(error);
    return false;
  }
  
  return result == TRUE;
}

std::string SecretServiceClient::GetSecretLabel(const std::string& key_id, 
                                               const std::string& type) const {
  if (type == "key") {
    return "dryft Nostr Key: " + key_id;
  } else if (type == "metadata") {
    return "dryft Nostr Key Metadata: " + key_id;
  } else if (type == "default") {
    return "Tungsten Default Nostr Key";
  }
  return "dryft Nostr: " + key_id;
}

std::string SecretServiceClient::SerializeEncryptedKey(const EncryptedKey& key) const {
  base::Value::Dict dict;
  dict.Set("encrypted_data", base::HexEncode(key.encrypted_data));
  dict.Set("salt", base::HexEncode(key.salt));
  dict.Set("iv", base::HexEncode(key.iv));
  dict.Set("kdf_iterations", static_cast<int>(key.kdf_iterations));
  dict.Set("algorithm", key.algorithm);
  
  std::string json;
  base::JSONWriter::Write(dict, &json);
  return json;
}

std::optional<EncryptedKey> SecretServiceClient::DeserializeEncryptedKey(
    const std::string& data) const {
  auto value = base::JSONReader::Read(data);
  if (!value || !value->is_dict()) {
    return std::nullopt;
  }
  
  const auto& dict = value->GetDict();
  
  EncryptedKey key;
  
  auto* encrypted_hex = dict.FindString("encrypted_data");
  auto* salt_hex = dict.FindString("salt");
  auto* iv_hex = dict.FindString("iv");
  auto iterations = dict.FindInt("kdf_iterations");
  auto* algorithm = dict.FindString("algorithm");
  
  if (!encrypted_hex || !salt_hex || !iv_hex || !iterations || !algorithm) {
    return std::nullopt;
  }
  
  if (!base::HexStringToBytes(*encrypted_hex, &key.encrypted_data) ||
      !base::HexStringToBytes(*salt_hex, &key.salt) ||
      !base::HexStringToBytes(*iv_hex, &key.iv)) {
    return std::nullopt;
  }
  
  key.kdf_iterations = *iterations;
  key.algorithm = *algorithm;
  
  return key;
}

std::string SecretServiceClient::SerializeKeyMetadata(const KeyIdentifier& id) const {
  base::Value::Dict dict;
  dict.Set("id", id.id);
  dict.Set("name", id.name);
  dict.Set("npub", id.npub);
  dict.Set("created_at", id.created_at.ToJsTimeIgnoringNull());
  if (id.last_used.has_value()) {
    dict.Set("last_used", id.last_used->ToJsTimeIgnoringNull());
  }
  dict.Set("is_default", id.is_default);
  
  std::string json;
  base::JSONWriter::Write(dict, &json);
  return json;
}

std::optional<KeyIdentifier> SecretServiceClient::DeserializeKeyMetadata(
    const std::string& data) const {
  auto value = base::JSONReader::Read(data);
  if (!value || !value->is_dict()) {
    return std::nullopt;
  }
  
  const auto& dict = value->GetDict();
  
  KeyIdentifier id;
  
  auto* key_id = dict.FindString("id");
  auto* name = dict.FindString("name");
  auto* npub = dict.FindString("npub");
  auto created_at = dict.FindDouble("created_at");
  auto is_default = dict.FindBool("is_default");
  
  if (!key_id || !name || !npub || !created_at || !is_default) {
    return std::nullopt;
  }
  
  id.id = *key_id;
  id.name = *name;
  id.npub = *npub;
  id.created_at = base::Time::FromJsTime(*created_at);
  id.is_default = *is_default;
  
  auto last_used = dict.FindDouble("last_used");
  if (last_used) {
    id.last_used = base::Time::FromJsTime(*last_used);
  }
  
  return id;
}

}  // namespace nostr