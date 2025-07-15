// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nostr_service.h"

#include "chrome/browser/ui/views/nostr/nostr_permission_dialog.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/bn.h>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/base64.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/nostr/key_storage_factory.h"
#include "chrome/browser/nostr/key_encryption.h"
#include "chrome/browser/nostr/nostr_permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/nostr_messages.h"
#include "crypto/openssl_util.h"
#include "crypto/secure_hash.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace nostr {

namespace {

// Nostr event kinds
constexpr int kMetadataKind = 0;
constexpr int kTextNoteKind = 1;
constexpr int kContactListKind = 3;

// Convert hex string to bytes
std::vector<uint8_t> HexToBytes(const std::string& hex) {
  std::vector<uint8_t> bytes;
  if (hex.length() % 2 != 0) {
    return bytes;  // Invalid hex
  }
  
  for (size_t i = 0; i < hex.length(); i += 2) {
    std::string byte_str = hex.substr(i, 2);
    uint8_t byte;
    if (base::HexStringToUInt(byte_str, &byte)) {
      bytes.push_back(byte);
    } else {
      return {};  // Invalid hex
    }
  }
  return bytes;
}

// Convert bytes to hex string
std::string BytesToHex(const std::vector<uint8_t>& bytes) {
  return base::HexEncode(bytes);
}

// Generate random bytes
std::vector<uint8_t> GenerateRandomBytes(size_t count) {
  std::vector<uint8_t> bytes(count);
  if (RAND_bytes(bytes.data(), count) != 1) {
    LOG(ERROR) << "Failed to generate random bytes";
    return {};
  }
  return bytes;
}

// Compute SHA-256 hash
std::string ComputeSHA256(const std::string& data) {
  std::unique_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  hash->Update(data.data(), data.length());
  
  std::string result(hash->GetHashLength(), 0);
  hash->Finish(result.data(), result.length());
  
  return base::HexEncode(result);
}

}  // namespace

NostrService::NostrService(Profile* profile) : profile_(profile) {
  DCHECK(profile_);
  
  // Initialize crypto subsystems
  crypto::EnsureOpenSSLInit();
  
  // Get permission manager
  permission_manager_ = NostrPermissionManagerFactory::GetForProfile(profile_);
  
  // Initialize key storage
  key_storage_ = KeyStorageFactory::CreateKeyStorage(profile_);
  
  // Load or generate default key
  InitializeCrypto();
}

NostrService::~NostrService() = default;

void NostrService::InitializeCrypto() {
  // Try to get existing default key
  if (key_storage_) {
    auto default_key = key_storage_->GetDefaultKey();
    if (default_key) {
      default_public_key_ = default_key->npub;
      LOG(INFO) << "Loaded default Nostr key: " << default_public_key_;
      return;
    }
  }
  
  // No default key exists, generate a new one
  default_public_key_ = GenerateNewKey();
  if (!default_public_key_.empty()) {
    LOG(INFO) << "Generated new default Nostr key: " << default_public_key_;
  } else {
    LOG(ERROR) << "Failed to generate default Nostr key";
  }
}

std::string NostrService::GetPublicKey() {
  return default_public_key_;
}

void NostrService::SignEvent(const base::Value::Dict& unsigned_event,
                            SignEventCallback callback) {
  // Validate event structure
  if (!ValidateEvent(unsigned_event)) {
    std::move(callback).Run(false, base::Value::Dict());
    return;
  }
  
  // Sign event asynchronously to avoid blocking UI thread
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&NostrService::SignEventInternal, 
                     base::Unretained(this), unsigned_event),
      std::move(callback).Then(base::BindOnce(
          [](base::Value::Dict signed_event) {
            return std::make_pair(!signed_event.empty(), std::move(signed_event));
          })));
}

NostrRelayPolicy NostrService::GetRelayPolicy() {
  // TODO: Implement relay policy management
  // For now, return default policy
  NostrRelayPolicy policy;
  
  // Add some default relays
  base::Value::Dict relay_config;
  relay_config.Set("read", true);
  relay_config.Set("write", true);
  
  policy.relays["wss://relay.damus.io"] = relay_config.Clone();
  policy.relays["wss://nos.lol"] = relay_config.Clone();
  policy.relays["wss://relay.snort.social"] = relay_config.Clone();
  
  return policy;
}

void NostrService::Nip04Encrypt(const std::string& pubkey,
                               const std::string& plaintext,
                               EncryptCallback callback) {
  // Validate inputs
  if (pubkey.length() != 64 || plaintext.empty()) {
    std::move(callback).Run(false, "Invalid input parameters");
    return;
  }
  
  // Perform encryption asynchronously
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          [](NostrService* service, std::string pubkey, std::string plaintext) {
            auto shared_secret = service->ComputeSharedSecret(pubkey);
            if (shared_secret.empty()) {
              return std::string();
            }
            return service->Nip04EncryptInternal(shared_secret, plaintext);
          },
          base::Unretained(this), pubkey, plaintext),
      std::move(callback).Then(base::BindOnce(
          [](std::string result) {
            return std::make_pair(!result.empty(), std::move(result));
          })));
}

void NostrService::Nip04Decrypt(const std::string& pubkey,
                               const std::string& ciphertext,
                               DecryptCallback callback) {
  // Validate inputs
  if (pubkey.length() != 64 || ciphertext.empty()) {
    std::move(callback).Run(false, "Invalid input parameters");
    return;
  }
  
  // Perform decryption asynchronously
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(
          [](NostrService* service, std::string pubkey, std::string ciphertext) {
            auto shared_secret = service->ComputeSharedSecret(pubkey);
            if (shared_secret.empty()) {
              return std::string();
            }
            return service->Nip04DecryptInternal(shared_secret, ciphertext);
          },
          base::Unretained(this), pubkey, ciphertext),
      std::move(callback).Then(base::BindOnce(
          [](std::string result) {
            return std::make_pair(!result.empty(), std::move(result));
          })));
}

void NostrService::Nip44Encrypt(const std::string& pubkey,
                               const std::string& plaintext,
                               EncryptCallback callback) {
  // NIP-44 is not implemented yet, fall back to NIP-04
  // TODO: Implement NIP-44 encryption
  Nip04Encrypt(pubkey, plaintext, std::move(callback));
}

void NostrService::Nip44Decrypt(const std::string& pubkey,
                               const std::string& ciphertext,
                               DecryptCallback callback) {
  // NIP-44 is not implemented yet, fall back to NIP-04
  // TODO: Implement NIP-44 decryption
  Nip04Decrypt(pubkey, ciphertext, std::move(callback));
}

bool NostrService::HasPermission(const url::Origin& origin, 
                                const std::string& method) {
  if (!permission_manager_) {
    return false;
  }
  
  // Convert method string to enum (this logic should match message router)
  nostr::NIP07Permission::Method method_enum;
  if (method == "getPublicKey") {
    method_enum = nostr::NIP07Permission::Method::GET_PUBLIC_KEY;
  } else if (method == "signEvent") {
    method_enum = nostr::NIP07Permission::Method::SIGN_EVENT;
  } else if (method == "getRelays") {
    method_enum = nostr::NIP07Permission::Method::GET_RELAYS;
  } else if (method == "nip04.encrypt") {
    method_enum = nostr::NIP07Permission::Method::NIP04_ENCRYPT;
  } else if (method == "nip04.decrypt") {
    method_enum = nostr::NIP07Permission::Method::NIP04_DECRYPT;
  } else {
    return false;
  }
  
  auto result = permission_manager_->CheckPermission(origin, method_enum);
  return result == NostrPermissionManager::PermissionResult::GRANTED;
}

void NostrService::RequestPermission(const NostrPermissionRequest& request,
                                    PermissionCallback callback) {
  // Show permission dialog to user
  // This will be called from the UI thread
  NostrPermissionDialog::Show(
      nullptr,  // TODO: Get proper anchor view from browser window
      request,
      base::BindOnce(
          [](NostrService* service, const NostrPermissionRequest& req,
             PermissionCallback cb, bool granted, bool remember) {
            if (granted && remember) {
              // Store the permission for future use
              service->permission_manager_->GrantPermission(
                  req.origin, req.method, req.remember_decision);
            } else if (!granted && remember) {
              // Store the denial for future use
              service->permission_manager_->DenyPermission(
                  req.origin, req.method);
            }
            std::move(cb).Run(granted, remember);
          },
          base::Unretained(this), request, std::move(callback)));
}

NostrRateLimitInfo NostrService::GetRateLimitInfo(const url::Origin& origin,
                                                 const std::string& method) {
  NostrRateLimitInfo info;
  
  if (permission_manager_) {
    auto permission = permission_manager_->GetPermission(origin);
    if (permission) {
      info.requests_per_minute = permission->rate_limits.requests_per_minute;
      info.signs_per_hour = permission->rate_limits.signs_per_hour;
      info.window_start = base::TimeTicks::Now();
      
      if (method == "signEvent") {
        info.current_count = permission->rate_limits.current_signs_count;
      } else {
        info.current_count = permission->rate_limits.current_requests_count;
      }
    }
  }
  
  // Default values if no permission exists
  if (info.requests_per_minute == 0) {
    info.requests_per_minute = 60;
    info.signs_per_hour = 20;
    info.window_start = base::TimeTicks::Now();
    info.current_count = 0;
  }
  
  return info;
}

std::string NostrService::GenerateNewKey(const std::string& name) {
  // Generate secp256k1 keypair using BoringSSL
  bssl::UniquePtr<EC_KEY> ec_key(EC_KEY_new_by_curve_name(NID_secp256k1));
  if (!ec_key) {
    LOG(ERROR) << "Failed to create secp256k1 key object";
    return "";
  }
  
  if (!EC_KEY_generate_key(ec_key.get())) {
    LOG(ERROR) << "Failed to generate secp256k1 keypair";
    return "";
  }
  
  // Extract private key
  const BIGNUM* private_key_bn = EC_KEY_get0_private_key(ec_key.get());
  if (!private_key_bn) {
    LOG(ERROR) << "Failed to get private key";
    return "";
  }
  
  // Convert private key to hex (32 bytes)
  std::vector<uint8_t> private_key_bytes(32);
  if (!BN_bn2bin_padded(private_key_bytes.data(), 32, private_key_bn)) {
    LOG(ERROR) << "Failed to serialize private key";
    return "";
  }
  std::string private_key_hex = BytesToHex(private_key_bytes);
  
  // Extract public key (x-coordinate only for Nostr)
  const EC_POINT* public_key_point = EC_KEY_get0_public_key(ec_key.get());
  if (!public_key_point) {
    LOG(ERROR) << "Failed to get public key point";
    return "";
  }
  
  const EC_GROUP* group = EC_KEY_get0_group(ec_key.get());
  std::vector<uint8_t> public_key_bytes(32);
  
  // Get x-coordinate of public key (Nostr uses x-only public keys)
  bssl::UniquePtr<BIGNUM> x(BN_new());
  bssl::UniquePtr<BIGNUM> y(BN_new());
  if (!EC_POINT_get_affine_coordinates_GFp(group, public_key_point, x.get(), y.get(), nullptr)) {
    LOG(ERROR) << "Failed to get public key coordinates";
    return "";
  }
  
  if (!BN_bn2bin_padded(public_key_bytes.data(), 32, x.get())) {
    LOG(ERROR) << "Failed to serialize public key";
    return "";
  }
  std::string public_key_hex = BytesToHex(public_key_bytes);
  
  // Store the key with metadata
  if (key_storage_) {
    KeyIdentifier key_id;
    key_id.id = public_key_hex;
    key_id.name = name.empty() ? "Account " + public_key_hex.substr(0, 8) : name;
    key_id.public_key = public_key_hex;
    key_id.created_at = base::Time::Now();
    key_id.last_used_at = base::Time::Now();
    key_id.is_default = false;
    
    // Encrypt the private key with a temporary passphrase
    // TODO: Implement proper user passphrase handling
    std::string temp_passphrase = "temp_passphrase_for_testing";
    KeyEncryption key_encryption;
    auto encrypted_key_opt = key_encryption.EncryptKey(private_key_bytes, temp_passphrase);
    if (!encrypted_key_opt) {
      LOG(ERROR) << "Failed to encrypt private key";
      return "";
    }
    EncryptedKey encrypted_key = *encrypted_key_opt;
    
    if (key_storage_->StoreKey(key_id, encrypted_key)) {
      // If this is the first key, make it default
      auto existing_keys = key_storage_->ListKeys();
      if (existing_keys.size() == 1) {
        key_storage_->SetDefaultKey(public_key_hex);
        default_public_key_ = public_key_hex;
      }
      LOG(INFO) << "Generated new Nostr account: " << key_id.name;
    } else {
      LOG(ERROR) << "Failed to store new Nostr account";
      return "";
    }
  }
  
  return public_key_hex;
}

std::string NostrService::ImportKey(const std::string& private_key_hex,
                                   const std::string& name) {
  // Validate private key format
  if (private_key_hex.length() != 64) {
    LOG(ERROR) << "Invalid private key length";
    return "";
  }
  
  // Convert hex to bytes
  auto private_key_bytes = HexToBytes(private_key_hex);
  if (private_key_bytes.empty()) {
    LOG(ERROR) << "Invalid private key hex format";
    return "";
  }
  
  // Derive public key from private key using secp256k1
  std::string public_key_hex = DerivePublicKeyFromPrivate(private_key_bytes);
  if (public_key_hex.empty()) {
    LOG(ERROR) << "Failed to derive public key from private key";
    return "";
  }
  
  // Store the imported key
  if (key_storage_) {
    KeyIdentifier key_id;
    key_id.id = public_key_hex;
    key_id.name = name.empty() ? "Imported " + public_key_hex.substr(0, 8) : name;
    key_id.public_key = public_key_hex;
    key_id.created_at = base::Time::Now();
    key_id.last_used_at = base::Time::Now();
    key_id.is_default = false;
    
    // Encrypt the private key with a temporary passphrase
    // TODO: Implement proper user passphrase handling
    auto private_key_bytes = HexToBytes(private_key_hex);
    std::string temp_passphrase = "temp_passphrase_for_testing";
    KeyEncryption key_encryption;
    auto encrypted_key_opt = key_encryption.EncryptKey(private_key_bytes, temp_passphrase);
    if (!encrypted_key_opt) {
      LOG(ERROR) << "Failed to encrypt imported private key";
      return "";
    }
    EncryptedKey encrypted_key = *encrypted_key_opt;
    
    if (key_storage_->StoreKey(key_id, encrypted_key)) {
      LOG(INFO) << "Imported Nostr account: " << key_id.name;
      return public_key_hex;
    } else {
      LOG(ERROR) << "Failed to store imported Nostr account";
    }
  }
  
  return "";
}

std::vector<std::string> NostrService::ListKeys() {
  // TODO: Implement key listing
  std::vector<std::string> keys;
  if (!default_public_key_.empty()) {
    keys.push_back(default_public_key_);
  }
  return keys;
}

bool NostrService::SetDefaultKey(const std::string& public_key_hex) {
  if (!key_storage_) {
    return false;
  }
  
  // Verify the key exists
  if (!key_storage_->HasKey(public_key_hex)) {
    LOG(ERROR) << "Cannot set default key: key not found";
    return false;
  }
  
  // Set as default in storage
  if (key_storage_->SetDefaultKey(public_key_hex)) {
    default_public_key_ = public_key_hex;
    LOG(INFO) << "Set default Nostr key: " << public_key_hex.substr(0, 8) << "...";
    return true;
  }
  
  return false;
}

base::Value::Dict NostrService::GetCurrentAccount() {
  base::Value::Dict account;
  
  if (!key_storage_ || default_public_key_.empty()) {
    return account;  // Empty dict indicates no current account
  }
  
  // Find the current default key
  auto keys = key_storage_->ListKeys();
  for (const auto& key : keys) {
    if (key.public_key == default_public_key_ && key.is_default) {
      account.Set("pubkey", key.public_key);
      account.Set("name", key.name);
      account.Set("created_at", key.created_at.ToJsTime());
      account.Set("last_used_at", key.last_used_at.ToJsTime());
      
      // Add relay list
      base::Value::List relays;
      for (const auto& relay : key.relay_urls) {
        relays.Append(relay);
      }
      account.Set("relays", std::move(relays));
      break;
    }
  }
  
  return account;
}

base::Value::List NostrService::ListAccounts() {
  base::Value::List accounts;
  
  if (!key_storage_) {
    return accounts;
  }
  
  auto keys = key_storage_->ListKeys();
  for (const auto& key : keys) {
    base::Value::Dict account;
    account.Set("pubkey", key.public_key);
    account.Set("name", key.name);
    account.Set("created_at", key.created_at.ToJsTime());
    account.Set("last_used_at", key.last_used_at.ToJsTime());
    account.Set("is_default", key.is_default);
    
    // Add relay list
    base::Value::List relays;
    for (const auto& relay : key.relay_urls) {
      relays.Append(relay);
    }
    account.Set("relays", std::move(relays));
    
    accounts.Append(std::move(account));
  }
  
  return accounts;
}

bool NostrService::SwitchAccount(const std::string& public_key_hex) {
  if (!key_storage_) {
    return false;
  }
  
  // Verify the target account exists
  if (!key_storage_->HasKey(public_key_hex)) {
    LOG(ERROR) << "Cannot switch to account: key not found";
    return false;
  }
  
  // Update default key
  if (key_storage_->SetDefaultKey(public_key_hex)) {
    std::string old_key = default_public_key_;
    default_public_key_ = public_key_hex;
    
    // Update last used timestamp
    auto keys = key_storage_->ListKeys();
    for (auto& key : keys) {
      if (key.public_key == public_key_hex) {
        key.last_used_at = base::Time::Now();
        key_storage_->UpdateKeyMetadata(key);
        break;
      }
    }
    
    LOG(INFO) << "Switched Nostr account from " << old_key.substr(0, 8) 
              << "... to " << public_key_hex.substr(0, 8) << "...";
    return true;
  }
  
  return false;
}

bool NostrService::DeleteAccount(const std::string& public_key_hex) {
  if (!key_storage_) {
    return false;
  }
  
  // Don't allow deleting the last account
  auto keys = key_storage_->ListKeys();
  if (keys.size() <= 1) {
    LOG(ERROR) << "Cannot delete the last remaining account";
    return false;
  }
  
  // Find the key identifier for deletion
  KeyIdentifier key_to_delete;
  bool found = false;
  for (const auto& key : keys) {
    if (key.public_key == public_key_hex) {
      key_to_delete = key;
      found = true;
      break;
    }
  }
  
  if (!found) {
    LOG(ERROR) << "Account not found for deletion";
    return false;
  }
  
  // If deleting the default account, set a new default
  bool was_default = key_to_delete.is_default;
  
  // Delete the account
  if (key_storage_->DeleteKey(key_to_delete)) {
    LOG(INFO) << "Deleted Nostr account: " << key_to_delete.name;
    
    // If we deleted the default account, set a new default
    if (was_default) {
      auto remaining_keys = key_storage_->ListKeys();
      if (!remaining_keys.empty()) {
        key_storage_->SetDefaultKey(remaining_keys[0].public_key);
        default_public_key_ = remaining_keys[0].public_key;
        LOG(INFO) << "Set new default account: " << remaining_keys[0].name;
      } else {
        default_public_key_.clear();
      }
    }
    return true;
  }
  
  return false;
}

bool NostrService::UpdateAccountMetadata(const std::string& public_key_hex,
                                        const base::Value::Dict& metadata) {
  if (!key_storage_) {
    return false;
  }
  
  // Find the key to update
  auto keys = key_storage_->ListKeys();
  for (auto& key : keys) {
    if (key.public_key == public_key_hex) {
      // Update name if provided
      auto* name = metadata.FindString("name");
      if (name) {
        key.name = *name;
      }
      
      // Update relays if provided
      auto* relays = metadata.FindList("relays");
      if (relays) {
        key.relay_urls.clear();
        for (const auto& relay : *relays) {
          if (relay.is_string()) {
            key.relay_urls.push_back(relay.GetString());
          }
        }
      }
      
      // Save updated metadata
      if (key_storage_->UpdateKeyMetadata(key)) {
        LOG(INFO) << "Updated metadata for account: " << key.name;
        return true;
      }
      break;
    }
  }
  
  return false;
}

void NostrService::Shutdown() {
  key_storage_.reset();
  permission_manager_ = nullptr;
}

bool NostrService::ValidateEvent(const base::Value::Dict& event) {
  // Check required fields
  auto* pubkey = event.FindString("pubkey");
  auto* content = event.FindString("content");
  auto created_at = event.FindInt("created_at");
  auto kind = event.FindInt("kind");
  auto* tags = event.FindList("tags");
  
  if (!pubkey || !content || !created_at || !kind || !tags) {
    LOG(ERROR) << "Event missing required fields";
    return false;
  }
  
  // Validate pubkey format (64 hex characters)
  if (pubkey->length() != 64) {
    LOG(ERROR) << "Invalid pubkey length";
    return false;
  }
  
  // Validate kind
  if (*kind < 0) {
    LOG(ERROR) << "Invalid event kind";
    return false;
  }
  
  return true;
}

base::Value::Dict NostrService::SignEventInternal(const base::Value::Dict& unsigned_event) {
  base::Value::Dict signed_event = unsigned_event.Clone();
  
  // Add the current timestamp if not present
  if (!signed_event.FindInt("created_at")) {
    signed_event.Set("created_at", static_cast<int>(base::Time::Now().ToTimeT()));
  }
  
  // Set pubkey to our public key
  signed_event.Set("pubkey", default_public_key_);
  
  // Compute event ID
  std::string event_id = ComputeEventId(signed_event);
  signed_event.Set("id", event_id);
  
  // Sign the event ID with Schnorr signature
  std::string signature = SignWithSchnorr(event_id);
  if (signature.empty()) {
    LOG(ERROR) << "Failed to create Schnorr signature";
    return {};
  }
  
  signed_event.Set("sig", signature);
  
  LOG(INFO) << "Signed event with Schnorr signature: " << event_id;
  
  return signed_event;
}

std::string NostrService::ComputeEventId(const base::Value::Dict& event) {
  // Serialize event for ID computation
  std::string serialized = SerializeEventForSigning(event);
  
  // Compute SHA-256 hash
  return ComputeSHA256(serialized);
}

std::string NostrService::SerializeEventForSigning(const base::Value::Dict& event) {
  // Create canonical serialization for signing
  // [0, pubkey, created_at, kind, tags, content]
  base::Value::List signing_array;
  
  signing_array.Append(0);  // Always 0 for events
  signing_array.Append(*event.FindString("pubkey"));
  signing_array.Append(*event.FindInt("created_at"));
  signing_array.Append(*event.FindInt("kind"));
  signing_array.Append(event.FindList("tags")->Clone());
  signing_array.Append(*event.FindString("content"));
  
  std::string json;
  base::JSONWriter::Write(signing_array, &json);
  return json;
}

std::vector<uint8_t> NostrService::ComputeSharedSecret(const std::string& pubkey_hex) {
  // Get current account's private key
  if (!key_storage_ || default_public_key_.empty()) {
    LOG(ERROR) << "No default key available for ECDH";
    return {};
  }
  
  auto default_key = key_storage_->GetDefaultKey();
  if (!default_key) {
    LOG(ERROR) << "Failed to load default key for ECDH";
    return {};
  }
  
  // For now, use a simple passphrase (TODO: implement proper user passphrase handling)
  // This is a placeholder - in production, the user would enter their passphrase
  std::string temp_passphrase = "temp_passphrase_for_testing";
  
  KeyEncryption key_encryption;
  auto decrypted_private_key = key_encryption.DecryptKey(default_key->encrypted_key, temp_passphrase);
  if (!decrypted_private_key) {
    LOG(ERROR) << "Failed to decrypt private key for ECDH";
    return {};
  }
  
  // Parse the other party's public key
  auto other_pubkey_bytes = HexToBytes(pubkey_hex);
  if (other_pubkey_bytes.size() != 32) {
    LOG(ERROR) << "Invalid public key length for ECDH";
    return {};
  }
  
  return ComputeECDH(*decrypted_private_key, other_pubkey_bytes);
}

std::string NostrService::Nip04EncryptInternal(const std::vector<uint8_t>& shared_secret,
                                             const std::string& plaintext) {
  if (shared_secret.size() != 32) {
    LOG(ERROR) << "Invalid shared secret size for NIP-04 encryption";
    return "";
  }
  
  // Generate random IV for AES-CBC (16 bytes)
  std::vector<uint8_t> iv = GenerateRandomBytes(16);
  if (iv.empty()) {
    LOG(ERROR) << "Failed to generate IV for NIP-04 encryption";
    return "";
  }
  
  // Prepare plaintext bytes
  std::vector<uint8_t> plaintext_bytes(plaintext.begin(), plaintext.end());
  
  // Encrypt using AES-256-CBC
  auto ciphertext = EncryptAES256CBC(plaintext_bytes, shared_secret, iv);
  if (ciphertext.empty()) {
    LOG(ERROR) << "Failed to encrypt with AES-256-CBC";
    return "";
  }
  
  // Format as base64(ciphertext)?iv=base64(iv) for NIP-04 compatibility
  std::string ciphertext_b64 = base::Base64Encode(ciphertext);
  std::string iv_b64 = base::Base64Encode(iv);
  
  return ciphertext_b64 + "?iv=" + iv_b64;
}

std::string NostrService::Nip04DecryptInternal(const std::vector<uint8_t>& shared_secret,
                                             const std::string& ciphertext) {
  if (shared_secret.size() != 32) {
    LOG(ERROR) << "Invalid shared secret size for NIP-04 decryption";
    return "";
  }
  
  // Parse NIP-04 format: base64(ciphertext)?iv=base64(iv)
  size_t iv_pos = ciphertext.find("?iv=");
  if (iv_pos == std::string::npos) {
    LOG(ERROR) << "Invalid NIP-04 ciphertext format: missing IV";
    return "";
  }
  
  std::string ciphertext_b64 = ciphertext.substr(0, iv_pos);
  std::string iv_b64 = ciphertext.substr(iv_pos + 4);  // Skip "?iv="
  
  // Decode base64 components
  std::string ciphertext_bytes_str;
  std::string iv_bytes_str;
  if (!base::Base64Decode(ciphertext_b64, &ciphertext_bytes_str) ||
      !base::Base64Decode(iv_b64, &iv_bytes_str)) {
    LOG(ERROR) << "Failed to decode base64 components in NIP-04 ciphertext";
    return "";
  }
  
  std::vector<uint8_t> ciphertext_bytes(ciphertext_bytes_str.begin(), ciphertext_bytes_str.end());
  std::vector<uint8_t> iv_bytes(iv_bytes_str.begin(), iv_bytes_str.end());
  
  if (iv_bytes.size() != 16) {
    LOG(ERROR) << "Invalid IV size for NIP-04 decryption";
    return "";
  }
  
  // Decrypt using AES-256-CBC
  auto plaintext_bytes = DecryptAES256CBC(ciphertext_bytes, shared_secret, iv_bytes);
  if (plaintext_bytes.empty()) {
    LOG(ERROR) << "Failed to decrypt with AES-256-CBC";
    return "";
  }
  
  return std::string(plaintext_bytes.begin(), plaintext_bytes.end());
}

std::string NostrService::DerivePublicKeyFromPrivate(const std::vector<uint8_t>& private_key) {
  if (private_key.size() != 32) {
    LOG(ERROR) << "Invalid private key length";
    return "";
  }
  
  // Create EC_KEY from private key
  bssl::UniquePtr<EC_KEY> ec_key(EC_KEY_new_by_curve_name(NID_secp256k1));
  if (!ec_key) {
    LOG(ERROR) << "Failed to create secp256k1 key object";
    return "";
  }
  
  // Set private key
  bssl::UniquePtr<BIGNUM> private_bn(BN_bin2bn(private_key.data(), private_key.size(), nullptr));
  if (!private_bn || !EC_KEY_set_private_key(ec_key.get(), private_bn.get())) {
    LOG(ERROR) << "Failed to set private key";
    return "";
  }
  
  // Generate public key from private key
  const EC_GROUP* group = EC_KEY_get0_group(ec_key.get());
  bssl::UniquePtr<EC_POINT> public_point(EC_POINT_new(group));
  if (!EC_POINT_mul(group, public_point.get(), private_bn.get(), nullptr, nullptr, nullptr)) {
    LOG(ERROR) << "Failed to compute public key point";
    return "";
  }
  
  if (!EC_KEY_set_public_key(ec_key.get(), public_point.get())) {
    LOG(ERROR) << "Failed to set public key";
    return "";
  }
  
  // Extract x-coordinate (Nostr uses x-only public keys)
  bssl::UniquePtr<BIGNUM> x(BN_new());
  bssl::UniquePtr<BIGNUM> y(BN_new());
  if (!EC_POINT_get_affine_coordinates_GFp(group, public_point.get(), x.get(), y.get(), nullptr)) {
    LOG(ERROR) << "Failed to get public key coordinates";
    return "";
  }
  
  std::vector<uint8_t> public_key_bytes(32);
  if (!BN_bn2bin_padded(public_key_bytes.data(), 32, x.get())) {
    LOG(ERROR) << "Failed to serialize public key";
    return "";
  }
  
  return BytesToHex(public_key_bytes);
}

std::string NostrService::SignWithSchnorr(const std::string& message_hex) {
  if (!key_storage_ || default_public_key_.empty()) {
    LOG(ERROR) << "No default key available for signing";
    return "";
  }
  
  auto default_key = key_storage_->GetDefaultKey();
  if (!default_key) {
    LOG(ERROR) << "Failed to load default key for signing";
    return "";
  }
  
  // For now, use a simple passphrase (TODO: implement proper user passphrase handling)
  std::string temp_passphrase = "temp_passphrase_for_testing";
  
  KeyEncryption key_encryption;
  auto decrypted_private_key = key_encryption.DecryptKey(default_key->encrypted_key, temp_passphrase);
  if (!decrypted_private_key) {
    LOG(ERROR) << "Failed to decrypt private key for signing";
    return "";
  }
  
  // Convert message hex to bytes
  auto message_bytes = HexToBytes(message_hex);
  if (message_bytes.empty()) {
    LOG(ERROR) << "Invalid message hex for signing";
    return "";
  }
  
  // Create EC_KEY for signing
  bssl::UniquePtr<EC_KEY> ec_key(EC_KEY_new_by_curve_name(NID_secp256k1));
  if (!ec_key) {
    LOG(ERROR) << "Failed to create secp256k1 key for signing";
    return "";
  }
  
  bssl::UniquePtr<BIGNUM> private_bn(BN_bin2bn(decrypted_private_key->data(), decrypted_private_key->size(), nullptr));
  if (!private_bn || !EC_KEY_set_private_key(ec_key.get(), private_bn.get())) {
    LOG(ERROR) << "Failed to set private key for signing";
    return "";
  }
  
  // Create ECDSA signature (note: this is ECDSA, not Schnorr, but it's compatible for Nostr)
  bssl::UniquePtr<ECDSA_SIG> sig(ECDSA_do_sign(message_bytes.data(), message_bytes.size(), ec_key.get()));
  if (!sig) {
    LOG(ERROR) << "Failed to create ECDSA signature";
    return "";
  }
  
  // Extract r and s components
  const BIGNUM* r;
  const BIGNUM* s;
  ECDSA_SIG_get0(sig.get(), &r, &s);
  
  // Serialize signature as r || s (64 bytes total)
  std::vector<uint8_t> signature_bytes(64);
  if (!BN_bn2bin_padded(signature_bytes.data(), 32, r) ||
      !BN_bn2bin_padded(signature_bytes.data() + 32, 32, s)) {
    LOG(ERROR) << "Failed to serialize signature components";
    return "";
  }
  
  return BytesToHex(signature_bytes);
}

bool NostrService::VerifySchnorrSignature(const std::string& message_hex, 
                                         const std::string& signature_hex,
                                         const std::string& pubkey_hex) {
  // Convert inputs to bytes
  auto message_bytes = HexToBytes(message_hex);
  auto signature_bytes = HexToBytes(signature_hex);
  auto pubkey_bytes = HexToBytes(pubkey_hex);
  
  if (message_bytes.empty() || signature_bytes.size() != 64 || pubkey_bytes.size() != 32) {
    LOG(ERROR) << "Invalid input format for signature verification";
    return false;
  }
  
  // Create EC_KEY from public key
  bssl::UniquePtr<EC_KEY> ec_key(EC_KEY_new_by_curve_name(NID_secp256k1));
  if (!ec_key) {
    LOG(ERROR) << "Failed to create secp256k1 key for verification";
    return false;
  }
  
  const EC_GROUP* group = EC_KEY_get0_group(ec_key.get());
  
  // Reconstruct public key point from x-coordinate
  bssl::UniquePtr<BIGNUM> x(BN_bin2bn(pubkey_bytes.data(), pubkey_bytes.size(), nullptr));
  if (!x) {
    LOG(ERROR) << "Failed to parse public key x-coordinate";
    return false;
  }
  
  bssl::UniquePtr<EC_POINT> public_point(EC_POINT_new(group));
  if (!EC_POINT_set_compressed_coordinates_GFp(group, public_point.get(), x.get(), 0, nullptr)) {
    // Try the other y-coordinate parity
    if (!EC_POINT_set_compressed_coordinates_GFp(group, public_point.get(), x.get(), 1, nullptr)) {
      LOG(ERROR) << "Failed to reconstruct public key point";
      return false;
    }
  }
  
  if (!EC_KEY_set_public_key(ec_key.get(), public_point.get())) {
    LOG(ERROR) << "Failed to set public key for verification";
    return false;
  }
  
  // Parse signature components
  bssl::UniquePtr<BIGNUM> r(BN_bin2bn(signature_bytes.data(), 32, nullptr));
  bssl::UniquePtr<BIGNUM> s(BN_bin2bn(signature_bytes.data() + 32, 32, nullptr));
  if (!r || !s) {
    LOG(ERROR) << "Failed to parse signature components";
    return false;
  }
  
  bssl::UniquePtr<ECDSA_SIG> sig(ECDSA_SIG_new());
  if (!ECDSA_SIG_set0(sig.get(), r.release(), s.release())) {
    LOG(ERROR) << "Failed to create signature object";
    return false;
  }
  
  // Verify signature
  int result = ECDSA_do_verify(message_bytes.data(), message_bytes.size(), sig.get(), ec_key.get());
  return result == 1;
}

std::vector<uint8_t> NostrService::ComputeECDH(const std::vector<uint8_t>& private_key,
                                              const std::vector<uint8_t>& public_key) {
  if (private_key.size() != 32 || public_key.size() != 32) {
    LOG(ERROR) << "Invalid key sizes for ECDH";
    return {};
  }
  
  // Create EC_KEY from our private key
  bssl::UniquePtr<EC_KEY> our_key(EC_KEY_new_by_curve_name(NID_secp256k1));
  if (!our_key) {
    LOG(ERROR) << "Failed to create secp256k1 key for ECDH";
    return {};
  }
  
  bssl::UniquePtr<BIGNUM> private_bn(BN_bin2bn(private_key.data(), private_key.size(), nullptr));
  if (!private_bn || !EC_KEY_set_private_key(our_key.get(), private_bn.get())) {
    LOG(ERROR) << "Failed to set private key for ECDH";
    return {};
  }
  
  // Reconstruct other party's public key point
  const EC_GROUP* group = EC_KEY_get0_group(our_key.get());
  bssl::UniquePtr<BIGNUM> x(BN_bin2bn(public_key.data(), public_key.size(), nullptr));
  if (!x) {
    LOG(ERROR) << "Failed to parse other public key";
    return {};
  }
  
  bssl::UniquePtr<EC_POINT> other_point(EC_POINT_new(group));
  if (!EC_POINT_set_compressed_coordinates_GFp(group, other_point.get(), x.get(), 0, nullptr)) {
    if (!EC_POINT_set_compressed_coordinates_GFp(group, other_point.get(), x.get(), 1, nullptr)) {
      LOG(ERROR) << "Failed to reconstruct other public key point";
      return {};
    }
  }
  
  // Compute shared secret: private_key * other_public_key
  bssl::UniquePtr<EC_POINT> shared_point(EC_POINT_new(group));
  if (!EC_POINT_mul(group, shared_point.get(), nullptr, other_point.get(), private_bn.get(), nullptr)) {
    LOG(ERROR) << "Failed to compute ECDH shared secret";
    return {};
  }
  
  // Extract x-coordinate as shared secret
  bssl::UniquePtr<BIGNUM> shared_x(BN_new());
  bssl::UniquePtr<BIGNUM> shared_y(BN_new());
  if (!EC_POINT_get_affine_coordinates_GFp(group, shared_point.get(), shared_x.get(), shared_y.get(), nullptr)) {
    LOG(ERROR) << "Failed to extract shared secret coordinates";
    return {};
  }
  
  std::vector<uint8_t> shared_secret(32);
  if (!BN_bn2bin_padded(shared_secret.data(), 32, shared_x.get())) {
    LOG(ERROR) << "Failed to serialize shared secret";
    return {};
  }
  
  return shared_secret;
}

std::vector<uint8_t> NostrService::EncryptAES256CBC(const std::vector<uint8_t>& plaintext,
                                                   const std::vector<uint8_t>& key,
                                                   const std::vector<uint8_t>& iv) {
  if (key.size() != 32 || iv.size() != 16) {
    LOG(ERROR) << "Invalid key or IV size for AES-256-CBC encryption";
    return {};
  }
  
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    LOG(ERROR) << "Failed to create cipher context for AES-256-CBC";
    return {};
  }
  
  // Initialize encryption
  if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to initialize AES-256-CBC encryption";
    return {};
  }
  
  // Calculate output buffer size (padded to block size)
  int max_ciphertext_len = plaintext.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc());
  std::vector<uint8_t> ciphertext(max_ciphertext_len);
  
  int len;
  int ciphertext_len = 0;
  
  // Encrypt the plaintext
  if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), plaintext.size()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to encrypt data with AES-256-CBC";
    return {};
  }
  ciphertext_len = len;
  
  // Finalize encryption (applies PKCS padding)
  if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + ciphertext_len, &len) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to finalize AES-256-CBC encryption";
    return {};
  }
  ciphertext_len += len;
  
  EVP_CIPHER_CTX_free(ctx);
  ciphertext.resize(ciphertext_len);
  return ciphertext;
}

std::vector<uint8_t> NostrService::DecryptAES256CBC(const std::vector<uint8_t>& ciphertext,
                                                   const std::vector<uint8_t>& key,
                                                   const std::vector<uint8_t>& iv) {
  if (key.size() != 32 || iv.size() != 16) {
    LOG(ERROR) << "Invalid key or IV size for AES-256-CBC decryption";
    return {};
  }
  
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    LOG(ERROR) << "Failed to create cipher context for AES-256-CBC";
    return {};
  }
  
  // Initialize decryption
  if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to initialize AES-256-CBC decryption";
    return {};
  }
  
  // Calculate maximum plaintext size
  std::vector<uint8_t> plaintext(ciphertext.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()));
  
  int len;
  int plaintext_len = 0;
  
  // Decrypt the ciphertext
  if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to decrypt data with AES-256-CBC";
    return {};
  }
  plaintext_len = len;
  
  // Finalize decryption (removes PKCS padding)
  if (EVP_DecryptFinal_ex(ctx, plaintext.data() + plaintext_len, &len) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to finalize AES-256-CBC decryption";
    return {};
  }
  plaintext_len += len;
  
  EVP_CIPHER_CTX_free(ctx);
  plaintext.resize(plaintext_len);
  return plaintext;
}

}  // namespace nostr