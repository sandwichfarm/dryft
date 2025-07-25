// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/extension_migration_service.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/nostr/nostr_service_factory.h"
#include "chrome/browser/nostr/nostr_permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace nostr {

namespace {

// Known extension IDs
constexpr char kAlbyExtensionId[] = "iokeahhehimjnekafflcihljlcjccdbe";
constexpr char kNos2xExtensionId[] = "kpgefcfmnafjgpblomihpgmejjdanjjp";
constexpr char kNostrConnectExtensionId[] = "mlbcnpnifbkckebjgmjpdhcedfhjoclg";
constexpr char kFlamingoExtensionId[] = "lbebcbdogjmcjendmnhjpkegagocpjmn";

// Storage keys used by extensions
constexpr char kAlbyKeysKey[] = "nostr_keys";
constexpr char kAlbyRelaysKey[] = "relays";
constexpr char kAlbyPermissionsKey[] = "permissions";

constexpr char kNos2xPrivateKeyKey[] = "privateKey";
constexpr char kNos2xRelaysKey[] = "relays";

}  // namespace

ExtensionMigrationService::ExtensionMigrationService(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

ExtensionMigrationService::~ExtensionMigrationService() = default;

std::vector<DetectedExtension> ExtensionMigrationService::DetectInstalledExtensions() {
  std::vector<DetectedExtension> detected;
  
  auto* extension_registry = extensions::ExtensionRegistry::Get(profile_);
  if (!extension_registry) {
    LOG(ERROR) << "Failed to get extension registry";
    return detected;
  }
  
  // Check for known Nostr extensions
  const struct {
    const char* id;
    DetectedExtension::Type type;
    const char* name;
  } known_extensions[] = {
    {kAlbyExtensionId, DetectedExtension::ALBY, "Alby"},
    {kNos2xExtensionId, DetectedExtension::NOS2X, "nos2x"},
    {kNostrConnectExtensionId, DetectedExtension::NOSTR_CONNECT, "Nostr Connect"},
    {kFlamingoExtensionId, DetectedExtension::FLAMINGO, "Flamingo"},
  };
  
  for (const auto& known : known_extensions) {
    const extensions::Extension* extension = 
        extension_registry->GetInstalledExtension(known.id);
    
    if (extension) {
      DetectedExtension detected_ext;
      detected_ext.type = known.type;
      detected_ext.id = extension->id();
      detected_ext.name = known.name;
      detected_ext.version = extension->VersionString();
      detected_ext.is_enabled = extension_registry->enabled_extensions().Contains(extension->id());
      
      // Get storage path
      base::FilePath profile_path = profile_->GetPath();
      detected_ext.storage_path = profile_path
          .Append(FILE_PATH_LITERAL("Local Extension Settings"))
          .Append(extension->id());
      
      detected.push_back(detected_ext);
      
      LOG(INFO) << "Detected Nostr extension: " << detected_ext.name 
                << " v" << detected_ext.version
                << " (enabled: " << detected_ext.is_enabled << ")";
    }
  }
  
  return detected;
}

MigrationData ExtensionMigrationService::ReadExtensionData(
    const DetectedExtension& extension) {
  switch (extension.type) {
    case DetectedExtension::ALBY:
      return ReadAlbyData(extension.storage_path);
    case DetectedExtension::NOS2X:
      return ReadNos2xData(extension.storage_path);
    case DetectedExtension::NOSTR_CONNECT:
      return ReadNostrConnectData(extension.storage_path);
    case DetectedExtension::FLAMINGO:
      return ReadFlamingoData(extension.storage_path);
    default:
      MigrationData data;
      data.success = false;
      data.error_message = "Unknown extension type";
      return data;
  }
}

void ExtensionMigrationService::MigrateFromExtension(
    const DetectedExtension& extension,
    const MigrationData& data,
    MigrationProgressCallback progress_callback,
    MigrationResultCallback result_callback) {
  if (!data.success) {
    std::move(result_callback).Run(false, data.error_message);
    return;
  }
  
  int total_items = data.keys.size() + 
                   (data.relay_urls.empty() ? 0 : 1) + 
                   (data.permissions.empty() ? 0 : 1);
  int completed = 0;
  
  // Import keys
  if (!data.keys.empty()) {
    progress_callback.Run(completed, total_items, "Importing keys...");
    ImportKeys(data.keys, progress_callback);
    completed += data.keys.size();
  }
  
  // Import relays
  if (!data.relay_urls.empty()) {
    progress_callback.Run(completed, total_items, "Importing relay list...");
    ImportRelays(data.relay_urls);
    completed++;
  }
  
  // Import permissions
  if (!data.permissions.empty()) {
    progress_callback.Run(completed, total_items, "Importing permissions...");
    ImportPermissions(data.permissions);
    completed++;
  }
  
  progress_callback.Run(completed, total_items, "Migration complete");
  std::move(result_callback).Run(true, "Successfully imported data from " + extension.name);
}

bool ExtensionMigrationService::ShouldDisableExtension(
    const DetectedExtension& extension) const {
  // Only suggest disabling if extension is currently enabled
  return extension.is_enabled;
}

void ExtensionMigrationService::DisableExtension(const DetectedExtension& extension) {
  auto* extension_service = extensions::ExtensionSystem::Get(profile_)
      ->extension_service();
  
  if (extension_service) {
    extension_service->DisableExtension(
        extension.id, extensions::disable_reason::DISABLE_USER_ACTION);
    LOG(INFO) << "Disabled extension: " << extension.name;
  }
}

void ExtensionMigrationService::Shutdown() {
  weak_factory_.InvalidateWeakPtrs();
}

MigrationData ExtensionMigrationService::ReadAlbyData(
    const base::FilePath& storage_path) {
  MigrationData data;
  
  // Alby stores data in LevelDB
  auto keys_value = ReadExtensionLevelDB(storage_path, kAlbyKeysKey);
  if (!keys_value) {
    data.success = false;
    data.error_message = "Failed to read Alby keys";
    return data;
  }
  
  // Parse keys JSON
  if (keys_value->is_dict()) {
    const base::Value::Dict& keys_dict = keys_value->GetDict();
    
    // Alby typically stores keys in a format like:
    // { "privateKey": "hex", "publicKey": "hex", "name": "Account Name" }
    const std::string* private_key = keys_dict.FindString("privateKey");
    const std::string* name = keys_dict.FindString("name");
    
    if (private_key && !private_key->empty()) {
      MigrationData::KeyData key_data;
      key_data.private_key_hex = *private_key;
      key_data.name = name ? *name : "Alby Account";
      key_data.is_default = true;
      data.keys.push_back(key_data);
    }
  }
  
  // Read relays
  auto relays_value = ReadExtensionLevelDB(storage_path, kAlbyRelaysKey);
  if (relays_value && relays_value->is_list()) {
    for (const auto& relay : relays_value->GetList()) {
      if (relay.is_string()) {
        data.relay_urls.push_back(relay.GetString());
      }
    }
  }
  
  // Read permissions
  auto permissions_value = ReadExtensionLevelDB(storage_path, kAlbyPermissionsKey);
  if (permissions_value && permissions_value->is_dict()) {
    // Alby stores permissions as { "origin": ["method1", "method2"] }
    for (const auto [origin, methods] : permissions_value->GetDict()) {
      if (methods.is_list()) {
        MigrationData::PermissionData perm;
        perm.origin = origin;
        for (const auto& method : methods.GetList()) {
          if (method.is_string()) {
            perm.allowed_methods.push_back(method.GetString());
          }
        }
        if (!perm.allowed_methods.empty()) {
          data.permissions.push_back(perm);
        }
      }
    }
  }
  
  data.success = !data.keys.empty();
  if (!data.success) {
    data.error_message = "No keys found in Alby extension";
  }
  
  return data;
}

MigrationData ExtensionMigrationService::ReadNos2xData(
    const base::FilePath& storage_path) {
  MigrationData data;
  
  // nos2x stores data differently
  auto private_key_value = ReadExtensionLevelDB(storage_path, kNos2xPrivateKeyKey);
  if (private_key_value && private_key_value->is_string()) {
    MigrationData::KeyData key_data;
    key_data.private_key_hex = private_key_value->GetString();
    key_data.name = "nos2x Account";
    key_data.is_default = true;
    data.keys.push_back(key_data);
  }
  
  // Read relays
  auto relays_value = ReadExtensionLevelDB(storage_path, kNos2xRelaysKey);
  if (relays_value && relays_value->is_list()) {
    for (const auto& relay : relays_value->GetList()) {
      if (relay.is_string()) {
        data.relay_urls.push_back(relay.GetString());
      }
    }
  }
  
  data.success = !data.keys.empty();
  if (!data.success) {
    data.error_message = "No keys found in nos2x extension";
  }
  
  return data;
}

MigrationData ExtensionMigrationService::ReadNostrConnectData(
    const base::FilePath& storage_path) {
  MigrationData data;
  // TODO: Implement Nostr Connect data reading
  data.success = false;
  data.error_message = "Nostr Connect migration not yet implemented";
  return data;
}

MigrationData ExtensionMigrationService::ReadFlamingoData(
    const base::FilePath& storage_path) {
  MigrationData data;
  // TODO: Implement Flamingo data reading
  data.success = false;
  data.error_message = "Flamingo migration not yet implemented";
  return data;
}

void ExtensionMigrationService::ImportKeys(
    const std::vector<MigrationData::KeyData>& keys,
    MigrationProgressCallback progress_callback) {
  auto* nostr_service = NostrServiceFactory::GetForProfile(profile_);
  if (!nostr_service) {
    LOG(ERROR) << "Failed to get NostrService for key import";
    return;
  }
  
  for (size_t i = 0; i < keys.size(); ++i) {
    const auto& key = keys[i];
    progress_callback.Run(i, keys.size(), "Importing key: " + key.name);
    
    std::string public_key = nostr_service->ImportKey(key.private_key_hex, key.name);
    if (!public_key.empty() && key.is_default) {
      nostr_service->SetDefaultKey(public_key);
    }
  }
}

void ExtensionMigrationService::ImportRelays(
    const std::vector<std::string>& relay_urls) {
  // TODO: Store relay URLs in preferences
  LOG(INFO) << "Importing " << relay_urls.size() << " relay URLs";
  
  // For now, just log them
  for (const auto& url : relay_urls) {
    LOG(INFO) << "  Relay: " << url;
  }
}

void ExtensionMigrationService::ImportPermissions(
    const std::vector<MigrationData::PermissionData>& permissions) {
  auto* permission_manager = NostrPermissionManagerFactory::GetForProfile(profile_);
  if (!permission_manager) {
    LOG(ERROR) << "Failed to get permission manager";
    return;
  }
  
  for (const auto& perm : permissions) {
    url::Origin origin = url::Origin::Create(GURL(perm.origin));
    for (const auto& method : perm.allowed_methods) {
      // Grant permission for each method
      permission_manager->GrantPermission(origin, method, true);
    }
  }
}

std::unique_ptr<base::Value> ExtensionMigrationService::ReadExtensionLevelDB(
    const base::FilePath& storage_path,
    const std::string& key) {
  leveldb::DB* db = nullptr;
  leveldb::Options options;
  options.create_if_missing = false;
  
  leveldb::Status status = leveldb::DB::Open(options, storage_path.value(), &db);
  if (!status.ok() || !db) {
    LOG(ERROR) << "Failed to open LevelDB at " << storage_path.value()
               << ": " << status.ToString();
    return nullptr;
  }
  
  std::string value;
  status = db->Get(leveldb::ReadOptions(), key, &value);
  
  std::unique_ptr<leveldb::DB> db_cleanup(db);
  
  if (!status.ok()) {
    if (!status.IsNotFound()) {
      LOG(ERROR) << "Failed to read key '" << key << "': " << status.ToString();
    }
    return nullptr;
  }
  
  // Parse JSON value
  auto result = base::JSONReader::ReadAndReturnValueWithError(value);
  if (!result.has_value()) {
    LOG(ERROR) << "Failed to parse JSON for key '" << key 
               << "': " << result.error().message;
    return nullptr;
  }
  
  return std::make_unique<base::Value>(std::move(*result));
}

}  // namespace nostr