// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nostr/nostr_migration_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/nostr/extension_migration_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"

NostrMigrationHandler::NostrMigrationHandler() = default;

NostrMigrationHandler::~NostrMigrationHandler() = default;

void NostrMigrationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "detectNostrExtensions",
      base::BindRepeating(&NostrMigrationHandler::HandleDetectExtensions,
                         base::Unretained(this)));
  
  web_ui()->RegisterMessageCallback(
      "readExtensionData",
      base::BindRepeating(&NostrMigrationHandler::HandleReadExtensionData,
                         base::Unretained(this)));
  
  web_ui()->RegisterMessageCallback(
      "performMigration",
      base::BindRepeating(&NostrMigrationHandler::HandlePerformMigration,
                         base::Unretained(this)));
  
  web_ui()->RegisterMessageCallback(
      "disableExtension",
      base::BindRepeating(&NostrMigrationHandler::HandleDisableExtension,
                         base::Unretained(this)));
}

void NostrMigrationHandler::HandleDetectExtensions(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args[0].GetString();
  
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* migration_service = 
      nostr::ExtensionMigrationServiceFactory::GetForProfile(profile);
  
  auto detected = migration_service->DetectInstalledExtensions();
  
  base::Value::List result;
  for (const auto& ext : detected) {
    result.Append(ExtensionToDict(ext));
  }
  
  ResolveJavascriptCallback(base::Value(callback_id), result);
}

void NostrMigrationHandler::HandleReadExtensionData(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const std::string& callback_id = args[0].GetString();
  const base::Value::Dict& extension_dict = args[1].GetDict();
  
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* migration_service = 
      nostr::ExtensionMigrationServiceFactory::GetForProfile(profile);
  
  nostr::DetectedExtension extension = DictToExtension(extension_dict);
  nostr::MigrationData data = migration_service->ReadExtensionData(extension);
  
  ResolveJavascriptCallback(base::Value(callback_id), 
                           MigrationDataToDict(data));
}

void NostrMigrationHandler::HandlePerformMigration(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const std::string& callback_id = args[0].GetString();
  const base::Value::Dict& migration_dict = args[1].GetDict();
  
  const base::Value::Dict* extension_dict = migration_dict.FindDict("extension");
  const base::Value::Dict* data_dict = migration_dict.FindDict("data");
  
  if (!extension_dict || !data_dict) {
    std::string error_message = "Invalid migration data: ";
    if (!extension_dict) {
      error_message += "missing 'extension' field";
    }
    if (!data_dict) {
      if (!extension_dict) error_message += ", ";
      error_message += "missing 'data' field";
    }
    RejectJavascriptCallback(base::Value(callback_id), 
                           base::Value(error_message));
    return;
  }
  
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* migration_service = 
      nostr::ExtensionMigrationServiceFactory::GetForProfile(profile);
  
  nostr::DetectedExtension extension = DictToExtension(*extension_dict);
  
  // Convert data dict back to MigrationData
  nostr::MigrationData data;
  data.success = data_dict->FindBool("success").value_or(false);
  
  // Extract keys
  const base::Value::List* keys_list = data_dict->FindList("keys");
  if (keys_list) {
    for (const auto& key_value : *keys_list) {
      const base::Value::Dict& key_dict = key_value.GetDict();
      nostr::MigrationData::KeyData key_data;
      const std::string* name = key_dict.FindString("name");
      const std::string* private_key = key_dict.FindString("privateKeyHex");
      const std::optional<bool> is_default = key_dict.FindBool("isDefault");
      
      if (name) key_data.name = *name;
      if (private_key) key_data.private_key_hex = *private_key;
      if (is_default) key_data.is_default = *is_default;
      
      data.keys.push_back(key_data);
    }
  }
  
  // Extract relay URLs
  const base::Value::List* relays_list = data_dict->FindList("relayUrls");
  if (relays_list) {
    for (const auto& relay : *relays_list) {
      if (relay.is_string()) {
        data.relay_urls.push_back(relay.GetString());
      }
    }
  }
  
  // Extract permissions
  const base::Value::List* permissions_list = data_dict->FindList("permissions");
  if (permissions_list) {
    for (const auto& perm_value : *permissions_list) {
      const base::Value::Dict& perm_dict = perm_value.GetDict();
      nostr::MigrationData::PermissionData perm_data;
      const std::string* origin = perm_dict.FindString("origin");
      const base::Value::List* methods = perm_dict.FindList("allowedMethods");
      
      if (origin) perm_data.origin = *origin;
      if (methods) {
        for (const auto& method : *methods) {
          if (method.is_string()) {
            perm_data.allowed_methods.push_back(method.GetString());
          }
        }
      }
      
      data.permissions.push_back(perm_data);
    }
  }
  
  // Perform migration
  migration_service->MigrateFromExtension(
      extension, data,
      base::BindRepeating(&NostrMigrationHandler::OnMigrationProgress,
                         weak_factory_.GetWeakPtr(), callback_id),
      base::BindOnce(&NostrMigrationHandler::OnMigrationComplete,
                    weak_factory_.GetWeakPtr(), callback_id));
}

void NostrMigrationHandler::HandleDisableExtension(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const std::string& callback_id = args[0].GetString();
  const base::Value::Dict& extension_dict = args[1].GetDict();
  
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* migration_service = 
      nostr::ExtensionMigrationServiceFactory::GetForProfile(profile);
  
  nostr::DetectedExtension extension = DictToExtension(extension_dict);
  migration_service->DisableExtension(extension);
  
  ResolveJavascriptCallback(base::Value(callback_id), base::Value(true));
}

void NostrMigrationHandler::OnMigrationProgress(
    const std::string& callback_id,
    int items_completed,
    int total_items,
    const std::string& current_item) {
  base::Value::Dict progress;
  progress.Set("type", "migrationProgress");
  progress.Set("completed", items_completed);
  progress.Set("total", total_items);
  progress.Set("current", current_item);
  
  FireWebUIListener("migration-progress", progress);
}

void NostrMigrationHandler::OnMigrationComplete(
    const std::string& callback_id,
    bool success,
    const std::string& message) {
  base::Value::Dict result;
  result.Set("success", success);
  result.Set("message", message);
  
  ResolveJavascriptCallback(base::Value(callback_id), result);
}

base::Value::Dict NostrMigrationHandler::ExtensionToDict(
    const nostr::DetectedExtension& extension) {
  base::Value::Dict dict;
  dict.Set("type", static_cast<int>(extension.type));
  dict.Set("id", extension.id);
  dict.Set("name", extension.name);
  dict.Set("version", extension.version);
  dict.Set("storagePath", extension.storage_path.value());
  dict.Set("isEnabled", extension.is_enabled);
  return dict;
}

base::Value::Dict NostrMigrationHandler::MigrationDataToDict(
    const nostr::MigrationData& data) {
  base::Value::Dict dict;
  dict.Set("success", data.success);
  dict.Set("errorMessage", data.error_message);
  
  // Convert keys
  base::Value::List keys_list;
  for (const auto& key : data.keys) {
    base::Value::Dict key_dict;
    key_dict.Set("name", key.name);
    key_dict.Set("privateKeyHex", key.private_key_hex);
    key_dict.Set("isDefault", key.is_default);
    keys_list.Append(std::move(key_dict));
  }
  dict.Set("keys", std::move(keys_list));
  
  // Convert relay URLs
  base::Value::List relays_list;
  for (const auto& relay : data.relay_urls) {
    relays_list.Append(relay);
  }
  dict.Set("relayUrls", std::move(relays_list));
  
  // Convert permissions
  base::Value::List permissions_list;
  for (const auto& perm : data.permissions) {
    base::Value::Dict perm_dict;
    perm_dict.Set("origin", perm.origin);
    
    base::Value::List methods_list;
    for (const auto& method : perm.allowed_methods) {
      methods_list.Append(method);
    }
    perm_dict.Set("allowedMethods", std::move(methods_list));
    
    permissions_list.Append(std::move(perm_dict));
  }
  dict.Set("permissions", std::move(permissions_list));
  
  return dict;
}

nostr::DetectedExtension NostrMigrationHandler::DictToExtension(
    const base::Value::Dict& dict) {
  nostr::DetectedExtension extension;
  
  std::optional<int> type = dict.FindInt("type");
  if (type) {
    extension.type = static_cast<nostr::DetectedExtension::Type>(*type);
  }
  
  const std::string* id = dict.FindString("id");
  if (id) extension.id = *id;
  
  const std::string* name = dict.FindString("name");
  if (name) extension.name = *name;
  
  const std::string* version = dict.FindString("version");
  if (version) extension.version = *version;
  
  const std::string* storage_path = dict.FindString("storagePath");
  if (storage_path) {
    extension.storage_path = base::FilePath(*storage_path);
  }
  
  std::optional<bool> is_enabled = dict.FindBool("isEnabled");
  if (is_enabled) extension.is_enabled = *is_enabled;
  
  return extension;
}