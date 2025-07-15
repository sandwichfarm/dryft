// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nostr/nostr_settings_ui.h"

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/nostr/local_relay/local_relay_config.h"
#include "chrome/browser/nostr/nostr_permission_manager.h"
#include "chrome/browser/nostr/nostr_permission_manager_factory.h"
#include "chrome/browser/nostr/nostr_service.h"
#include "chrome/browser/nostr/nostr_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/tungsten_url_constants.h"
#include "chrome/grit/nostr_settings_resources.h"
#include "chrome/grit/nostr_settings_resources_map.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "url/gurl.h"

namespace tungsten {

namespace {

constexpr char kNostrSettingsHost[] = "settings-nostr";

// Helper functions for permission conversions
std::string NostrPermissionMethodToString(nostr::NIP07Permission::Method method) {
  switch (method) {
    case nostr::NIP07Permission::Method::GET_PUBLIC_KEY:
      return "getPublicKey";
    case nostr::NIP07Permission::Method::SIGN_EVENT:
      return "signEvent";
    case nostr::NIP07Permission::Method::GET_RELAYS:
      return "getRelays";
    case nostr::NIP07Permission::Method::NIP04_ENCRYPT:
      return "nip04.encrypt";
    case nostr::NIP07Permission::Method::NIP04_DECRYPT:
      return "nip04.decrypt";
    default:
      return "unknown";
  }
}

nostr::NIP07Permission::Method NostrPermissionMethodFromString(const std::string& str) {
  if (str == "getPublicKey")
    return nostr::NIP07Permission::Method::GET_PUBLIC_KEY;
  if (str == "signEvent")
    return nostr::NIP07Permission::Method::SIGN_EVENT;
  if (str == "getRelays")
    return nostr::NIP07Permission::Method::GET_RELAYS;
  if (str == "nip04.encrypt")
    return nostr::NIP07Permission::Method::NIP04_ENCRYPT;
  if (str == "nip04.decrypt")
    return nostr::NIP07Permission::Method::NIP04_DECRYPT;
  // Should not reach here in normal operation
  return nostr::NIP07Permission::Method::GET_PUBLIC_KEY;
}

std::string NostrPermissionPolicyToString(nostr::NIP07Permission::Policy policy) {
  switch (policy) {
    case nostr::NIP07Permission::Policy::ASK:
      return "ask";
    case nostr::NIP07Permission::Policy::ALLOW:
      return "allow";
    case nostr::NIP07Permission::Policy::DENY:
      return "deny";
    default:
      return "ask";
  }
}

nostr::NIP07Permission::Policy NostrPermissionPolicyFromString(const std::string& str) {
  if (str == "allow")
    return nostr::NIP07Permission::Policy::ALLOW;
  if (str == "deny")
    return nostr::NIP07Permission::Policy::DENY;
  return nostr::NIP07Permission::Policy::ASK;
}

void SetupWebUIDataSource(content::WebUIDataSource* source) {
  // Add localized strings
  source->AddString("nostrSettingsTitle", "Nostr Settings");
  source->AddString("accountsTitle", "Accounts");
  source->AddString("permissionsTitle", "Permissions");
  source->AddString("localRelayTitle", "Local Relay");
  source->AddString("blossomTitle", "Blossom Storage");
  source->AddString("securityTitle", "Security");
  
  // Add feature flags
  source->AddBoolean("isNostrEnabled", true);
  source->AddBoolean("isLocalRelaySupported", true);
  source->AddBoolean("isBlossomSupported", true);
  
  // Set CSP
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://settings-nostr 'self';");
      
  // Add resource files
  source->AddResourcePaths(
      base::make_span(kNostrSettingsResources, kNostrSettingsResourcesSize));
  source->SetDefaultResource(IDR_NOSTR_SETTINGS_INDEX_HTML);
}

}  // namespace

// NostrSettingsHandler implementation

NostrSettingsHandler::NostrSettingsHandler() = default;
NostrSettingsHandler::~NostrSettingsHandler() = default;

void NostrSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getNostrEnabled",
      base::BindRepeating(&NostrSettingsHandler::HandleGetNostrEnabled,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "setNostrEnabled",
      base::BindRepeating(&NostrSettingsHandler::HandleSetNostrEnabled,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getLocalRelayConfig",
      base::BindRepeating(&NostrSettingsHandler::HandleGetLocalRelayConfig,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "setLocalRelayConfig",
      base::BindRepeating(&NostrSettingsHandler::HandleSetLocalRelayConfig,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getAccounts",
      base::BindRepeating(&NostrSettingsHandler::HandleGetAccounts,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getPermissions",
      base::BindRepeating(&NostrSettingsHandler::HandleGetPermissions,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "setPermission",
      base::BindRepeating(&NostrSettingsHandler::HandleSetPermission,
                          weak_factory_.GetWeakPtr()));
  
  // Account management handlers
  web_ui()->RegisterMessageCallback(
      "generateKeys",
      base::BindRepeating(&NostrSettingsHandler::HandleGenerateKeys,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "createAccount",
      base::BindRepeating(&NostrSettingsHandler::HandleCreateAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "importAccount",
      base::BindRepeating(&NostrSettingsHandler::HandleImportAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "switchAccount",
      base::BindRepeating(&NostrSettingsHandler::HandleSwitchAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "deleteAccount",
      base::BindRepeating(&NostrSettingsHandler::HandleDeleteAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "exportAccount",
      base::BindRepeating(&NostrSettingsHandler::HandleExportAccount,
                          weak_factory_.GetWeakPtr()));
  
  // Enhanced permission handlers
  web_ui()->RegisterMessageCallback(
      "setPermissionFull",
      base::BindRepeating(&NostrSettingsHandler::HandleSetPermissionFull,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "resetPermission",
      base::BindRepeating(&NostrSettingsHandler::HandleResetPermission,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "deletePermission",
      base::BindRepeating(&NostrSettingsHandler::HandleDeletePermission,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "bulkPermissionAction",
      base::BindRepeating(&NostrSettingsHandler::HandleBulkPermissionAction,
                          weak_factory_.GetWeakPtr()));
}

void NostrSettingsHandler::HandleGetNostrEnabled(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();
  
  // TODO: Get actual Nostr enabled state from preferences
  bool enabled = true;
  
  ResolveJavascriptCallback(callback_id, base::Value(enabled));
}

void NostrSettingsHandler::HandleSetNostrEnabled(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  bool enabled = args[1].GetBool();
  
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();
  
  // TODO: Set Nostr enabled state in preferences
  
  ResolveJavascriptCallback(callback_id, base::Value(true));
}

void NostrSettingsHandler::HandleGetLocalRelayConfig(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();
  
  base::Value::Dict config;
  config.Set("enabled", prefs->GetBoolean(nostr::local_relay::kRelayEnabledPref));
  config.Set("port", prefs->GetInteger(nostr::local_relay::kRelayPortPref));
  config.Set("interface", prefs->GetString(nostr::local_relay::kRelayInterfacePref));
  config.Set("externalAccess", prefs->GetBoolean(nostr::local_relay::kRelayExternalAccessPref));
  config.Set("maxStorageGB", prefs->GetInteger(nostr::local_relay::kMaxStorageGBPref));
  config.Set("maxEvents", prefs->GetInteger(nostr::local_relay::kMaxEventsPref));
  config.Set("retentionDays", prefs->GetInteger(nostr::local_relay::kRetentionDaysPref));
  
  ResolveJavascriptCallback(callback_id, base::Value(std::move(config)));
}

void NostrSettingsHandler::HandleSetLocalRelayConfig(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const base::Value::Dict& config = args[1].GetDict();
  
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();
  
  // Update preferences
  if (auto enabled = config.FindBool("enabled")) {
    prefs->SetBoolean(nostr::local_relay::kRelayEnabledPref, *enabled);
  }
  if (auto port = config.FindInt("port")) {
    prefs->SetInteger(nostr::local_relay::kRelayPortPref, *port);
  }
  if (auto* interface = config.FindString("interface")) {
    prefs->SetString(nostr::local_relay::kRelayInterfacePref, *interface);
  }
  if (auto external = config.FindBool("externalAccess")) {
    prefs->SetBoolean(nostr::local_relay::kRelayExternalAccessPref, *external);
  }
  if (auto storage = config.FindInt("maxStorageGB")) {
    prefs->SetInteger(nostr::local_relay::kMaxStorageGBPref, *storage);
  }
  if (auto events = config.FindInt("maxEvents")) {
    prefs->SetInteger(nostr::local_relay::kMaxEventsPref, *events);
  }
  if (auto retention = config.FindInt("retentionDays")) {
    prefs->SetInteger(nostr::local_relay::kRetentionDaysPref, *retention);
  }
  
  ResolveJavascriptCallback(callback_id, base::Value(true));
}

void NostrSettingsHandler::HandleGetAccounts(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* nostr_service = NostrServiceFactory::GetForProfile(profile);
  
  if (!nostr_service) {
    ResolveJavascriptCallback(callback_id, base::Value(base::Value::List()));
    return;
  }
  
  // Get accounts from NostrService - using the synchronous API
  base::Value::List accounts = nostr_service->ListAccounts();
  
  // Get current account to mark it as default
  base::Value::Dict current = nostr_service->GetCurrentAccount();
  std::string* current_pubkey = current.FindString("pubkey");
  
  // Update the isDefault flag in the accounts list
  if (current_pubkey) {
    for (auto& account : accounts) {
      if (account.is_dict()) {
        base::Value::Dict& dict = account.GetDict();
        std::string* pubkey = dict.FindString("pubkey");
        if (pubkey && *pubkey == *current_pubkey) {
          dict.Set("isDefault", true);
        } else {
          dict.Set("isDefault", false);
        }
      }
    }
  }
  
  ResolveJavascriptCallback(callback_id, base::Value(std::move(accounts)));
}

void NostrSettingsHandler::HandleGetPermissions(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* permission_manager = NostrPermissionManagerFactory::GetForProfile(profile);
  
  if (!permission_manager) {
    ResolveJavascriptCallback(callback_id, base::Value(base::Value::Dict()));
    return;
  }
  
  // Get all permissions
  std::vector<nostr::NIP07Permission> permissions = permission_manager->GetAllPermissions();
  
  base::Value::Dict permissions_dict;
  for (const auto& permission : permissions) {
    base::Value::Dict origin_permissions;
    
    // Add default policy
    origin_permissions.Set("default", NostrPermissionPolicyToString(permission.default_policy));
    
    // Add method-specific policies
    base::Value::Dict methods;
    for (const auto& [method, policy] : permission.method_policies) {
      methods.Set(NostrPermissionMethodToString(method),
                  NostrPermissionPolicyToString(policy));
    }
    origin_permissions.Set("methods", std::move(methods));
    
    // Add kind permissions (placeholder for future implementation)
    base::Value::Dict kind_permissions;
    // TODO: Add actual kind permission data when available
    origin_permissions.Set("kindPermissions", std::move(kind_permissions));
    
    // Add rate limits (placeholder for future implementation)  
    base::Value::Dict rate_limits;
    rate_limits.Set("requestsPerMinute", 60);
    rate_limits.Set("signsPerHour", 20);
    origin_permissions.Set("rateLimits", std::move(rate_limits));
    
    // Add metadata
    origin_permissions.Set("lastUsed", base::TimeToValue(permission.last_used));
    if (!permission.granted_until.is_null()) {
      origin_permissions.Set("grantedUntil", base::TimeToValue(permission.granted_until));
    }
    
    permissions_dict.Set(permission.origin.Serialize(), std::move(origin_permissions));
  }
  
  ResolveJavascriptCallback(callback_id, base::Value(std::move(permissions_dict)));
}

void NostrSettingsHandler::HandleSetPermission(const base::Value::List& args) {
  CHECK_EQ(4U, args.size());
  const base::Value& callback_id = args[0];
  const std::string& origin_str = args[1].GetString();
  const std::string& method_str = args[2].GetString();
  const std::string& policy_str = args[3].GetString();
  
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* permission_manager = NostrPermissionManagerFactory::GetForProfile(profile);
  
  if (!permission_manager) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }
  
  // Parse values
  auto origin = url::Origin::Create(GURL(origin_str));
  auto method = NostrPermissionMethodFromString(method_str);
  auto policy = NostrPermissionPolicyFromString(policy_str);
  
  // Get existing permission or create new one
  auto existing = permission_manager->GetPermission(origin);
  nostr::NIP07Permission permission;
  if (existing.has_value()) {
    permission = existing.value();
  } else {
    permission.origin = origin;
  }
  
  // Update the method policy
  if (method_str == "default") {
    permission.default_policy = policy;
  } else {
    permission.method_policies[method] = policy;
  }
  
  // Grant the updated permission
  auto result = permission_manager->GrantPermission(origin, permission);
  
  ResolveJavascriptCallback(callback_id, 
                           base::Value(result == nostr::NostrPermissionManager::GrantResult::SUCCESS));
}

void NostrSettingsHandler::ResolveJavascriptCallback(
    const base::Value& callback_id,
    const base::Value& response) {
  AllowJavascript();
  CallJavascriptFunction("cr.webUIResponse", callback_id, response);
}

void NostrSettingsHandler::HandleGenerateKeys(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* nostr_service = NostrServiceFactory::GetForProfile(profile);
  
  if (!nostr_service) {
    ResolveJavascriptCallback(callback_id, base::Value());
    return;
  }
  
  // Generate new keypair
  std::string pubkey = nostr_service->GenerateNewKey();
  
  // Get the key details (this is a simplified version)
  base::Value::Dict result;
  result.Set("pubkey", pubkey);
  result.Set("npub", "npub1" + pubkey.substr(0, 20) + "...");  // Simplified
  result.Set("privkey", "generated_privkey");  // Should not expose actual privkey
  result.Set("nsec", "nsec1...");  // Simplified
  
  ResolveJavascriptCallback(callback_id, base::Value(std::move(result)));
}

void NostrSettingsHandler::HandleCreateAccount(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const base::Value::Dict& account_data = args[1].GetDict();
  
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* nostr_service = NostrServiceFactory::GetForProfile(profile);
  
  if (!nostr_service) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }
  
  // Extract account data
  const std::string* name = account_data.FindString("name");
  const std::string* about = account_data.FindString("about");
  const std::string* picture = account_data.FindString("picture");
  const std::string* nip05 = account_data.FindString("nip05");
  
  // Store profile metadata
  // TODO: Implement proper profile storage
  
  ResolveJavascriptCallback(callback_id, base::Value(true));
}

void NostrSettingsHandler::HandleImportAccount(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const base::Value::Dict& data = args[1].GetDict();
  
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* nostr_service = NostrServiceFactory::GetForProfile(profile);
  
  if (!nostr_service) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }
  
  const std::string* private_key = data.FindString("privateKey");
  if (!private_key) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }
  
  // Import the key
  std::string pubkey = nostr_service->ImportKey(*private_key);
  
  ResolveJavascriptCallback(callback_id, base::Value(!pubkey.empty()));
}

void NostrSettingsHandler::HandleSwitchAccount(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const base::Value::Dict& data = args[1].GetDict();
  
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* nostr_service = NostrServiceFactory::GetForProfile(profile);
  
  if (!nostr_service) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }
  
  const std::string* pubkey = data.FindString("pubkey");
  if (!pubkey) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }
  
  bool success = nostr_service->SetDefaultKey(*pubkey);
  
  ResolveJavascriptCallback(callback_id, base::Value(success));
}

void NostrSettingsHandler::HandleDeleteAccount(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const base::Value::Dict& data = args[1].GetDict();
  
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* nostr_service = NostrServiceFactory::GetForProfile(profile);
  
  if (!nostr_service) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }
  
  const std::string* pubkey = data.FindString("pubkey");
  if (!pubkey) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }
  
  bool success = nostr_service->DeleteAccount(*pubkey);
  
  ResolveJavascriptCallback(callback_id, base::Value(success));
}

void NostrSettingsHandler::HandleExportAccount(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const base::Value::Dict& data = args[1].GetDict();
  
  const std::string* pubkey = data.FindString("pubkey");
  if (!pubkey) {
    ResolveJavascriptCallback(callback_id, base::Value());
    return;
  }
  
  // Create export data (without exposing private key directly)
  base::Value::Dict export_data;
  export_data.Set("version", 1);
  export_data.Set("pubkey", *pubkey);
  export_data.Set("created", base::TimeToValue(base::Time::Now()));
  export_data.Set("encrypted", true);
  // Note: Actual implementation should encrypt the private key
  
  ResolveJavascriptCallback(callback_id, base::Value(std::move(export_data)));
}

void NostrSettingsHandler::HandleSetPermissionFull(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const base::Value::Dict& permission_data = args[1].GetDict();
  
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* permission_manager = NostrPermissionManagerFactory::GetForProfile(profile);
  
  if (!permission_manager) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }
  
  const std::string* origin_str = permission_data.FindString("origin");
  const base::Value::Dict* permission_dict = permission_data.FindDict("permission");
  
  if (!origin_str || !permission_dict) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }
  
  // Parse origin
  auto origin = url::Origin::Create(GURL(*origin_str));
  
  // Create permission object
  nostr::NIP07Permission permission;
  permission.origin = origin;
  
  // Set default policy
  const std::string* default_policy = permission_dict->FindString("default");
  if (default_policy) {
    permission.default_policy = NostrPermissionPolicyFromString(*default_policy);
  }
  
  // Set method policies
  const base::Value::Dict* methods = permission_dict->FindDict("methods");
  if (methods) {
    for (const auto [method_str, policy_value] : *methods) {
      if (policy_value.is_string()) {
        auto method = NostrPermissionMethodFromString(method_str);
        auto policy = NostrPermissionPolicyFromString(policy_value.GetString());
        permission.method_policies[method] = policy;
      }
    }
  }
  
  // Set expiration if provided
  const std::string* granted_until = permission_dict->FindString("grantedUntil");
  if (granted_until) {
    base::Time expiration;
    if (base::Time::FromString(granted_until->c_str(), &expiration)) {
      permission.granted_until = expiration;
    }
  }
  
  // Grant the permission
  auto result = permission_manager->GrantPermission(origin, permission);
  
  ResolveJavascriptCallback(callback_id, 
                           base::Value(result == nostr::NostrPermissionManager::GrantResult::SUCCESS));
}

void NostrSettingsHandler::HandleResetPermission(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const base::Value::Dict& data = args[1].GetDict();
  
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* permission_manager = NostrPermissionManagerFactory::GetForProfile(profile);
  
  if (!permission_manager) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }
  
  const std::string* origin_str = data.FindString("origin");
  if (!origin_str) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }
  
  auto origin = url::Origin::Create(GURL(*origin_str));
  
  // Reset to default permission policy (keep the permission entry but reset to ASK)
  nostr::NIP07Permission permission;
  permission.origin = origin;
  permission.default_policy = nostr::NIP07Permission::Policy::ASK;
  // Clear all method-specific policies to use default
  permission.method_policies.clear();
  
  auto result = permission_manager->GrantPermission(origin, permission);
  
  ResolveJavascriptCallback(callback_id, 
                           base::Value(result == nostr::NostrPermissionManager::GrantResult::SUCCESS));
}

void NostrSettingsHandler::HandleDeletePermission(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const base::Value::Dict& data = args[1].GetDict();
  
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* permission_manager = NostrPermissionManagerFactory::GetForProfile(profile);
  
  if (!permission_manager) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }
  
  const std::string* origin_str = data.FindString("origin");
  if (!origin_str) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }
  
  auto origin = url::Origin::Create(GURL(*origin_str));
  
  // Remove the permission
  bool success = permission_manager->RevokePermission(origin);
  
  ResolveJavascriptCallback(callback_id, base::Value(success));
}

void NostrSettingsHandler::HandleBulkPermissionAction(const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const base::Value::Dict& data = args[1].GetDict();
  
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* permission_manager = NostrPermissionManagerFactory::GetForProfile(profile);
  
  if (!permission_manager) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }
  
  const std::string* action = data.FindString("action");
  if (!action) {
    ResolveJavascriptCallback(callback_id, base::Value(false));
    return;
  }
  
  bool success = false;
  
  if (*action == "clear-all") {
    // Clear all permissions
    success = permission_manager->ClearAllPermissions();
  } else if (*action == "allow-all" || *action == "deny-all" || *action == "reset-all") {
    // Get all current permissions and update them
    auto permissions = permission_manager->GetAllPermissions();
    
    nostr::NIP07Permission::Policy new_policy;
    if (*action == "allow-all") {
      new_policy = nostr::NIP07Permission::Policy::ALLOW;
    } else if (*action == "deny-all") {
      new_policy = nostr::NIP07Permission::Policy::DENY;
    } else {
      new_policy = nostr::NIP07Permission::Policy::ASK;
    }
    
    success = true;
    for (const auto& permission : permissions) {
      nostr::NIP07Permission updated_permission = permission;
      updated_permission.default_policy = new_policy;
      
      auto result = permission_manager->GrantPermission(permission.origin, updated_permission);
      if (result != nostr::NostrPermissionManager::GrantResult::SUCCESS) {
        success = false;
      }
    }
  }
  
  ResolveJavascriptCallback(callback_id, base::Value(success));
}

// NostrSettingsUI implementation

NostrSettingsUI::NostrSettingsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  
  // Create and add WebUI data source
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, kNostrSettingsHost);
  SetupWebUIDataSource(source);
  
  // Add message handler
  web_ui->AddMessageHandler(std::make_unique<NostrSettingsHandler>());
}

NostrSettingsUI::~NostrSettingsUI() = default;

}  // namespace tungsten