// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nostr_service.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/nostr/key_storage_factory.h"
#include "chrome/browser/nostr/nostr_permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/nostr_messages.h"
#include "crypto/openssl_util.h"
#include "crypto/secure_hash.h"

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
  // TODO: Implement permission dialog UI
  // For now, auto-grant for testing
  std::move(callback).Run(true, request.remember_decision);
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

std::string NostrService::GenerateNewKey() {
  // TODO: Implement secp256k1 key generation
  // For now, generate a dummy key
  auto random_bytes = GenerateRandomBytes(32);
  if (random_bytes.empty()) {
    return "";
  }
  
  std::string private_key_hex = BytesToHex(random_bytes);
  
  // TODO: Derive actual public key from private key
  // For now, use a placeholder
  auto public_key_bytes = GenerateRandomBytes(32);
  std::string public_key_hex = BytesToHex(public_key_bytes);
  
  // TODO: Store the key securely
  if (key_storage_) {
    // Create encrypted key structure
    // This would encrypt the private key with user's passphrase
    // For now, just log that we generated a key
    LOG(INFO) << "Generated new Nostr keypair (mock implementation)";
  }
  
  return public_key_hex;
}

std::string NostrService::ImportKey(const std::string& private_key_hex) {
  // TODO: Implement key import
  LOG(INFO) << "Key import not yet implemented";
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
  // TODO: Implement default key setting
  default_public_key_ = public_key_hex;
  return true;
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
  
  // TODO: Implement actual Schnorr signature
  // For now, generate a dummy signature
  auto random_sig = GenerateRandomBytes(64);
  std::string dummy_signature = BytesToHex(random_sig);
  signed_event.Set("sig", dummy_signature);
  
  LOG(INFO) << "Signed event (mock implementation): " << event_id;
  
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
  // TODO: Implement ECDH with secp256k1
  // For now, return a dummy shared secret
  auto dummy_secret = GenerateRandomBytes(32);
  LOG(INFO) << "Computing shared secret (mock implementation)";
  return dummy_secret;
}

std::string NostrService::Nip04EncryptInternal(const std::vector<uint8_t>& shared_secret,
                                             const std::string& plaintext) {
  // TODO: Implement NIP-04 encryption (AES-256-CBC)
  // For now, return a dummy encrypted string
  LOG(INFO) << "NIP-04 encryption (mock implementation)";
  return base::HexEncode(plaintext) + "?iv=" + base::HexEncode(GenerateRandomBytes(16));
}

std::string NostrService::Nip04DecryptInternal(const std::vector<uint8_t>& shared_secret,
                                             const std::string& ciphertext) {
  // TODO: Implement NIP-04 decryption (AES-256-CBC)
  // For now, attempt to reverse the mock encryption
  LOG(INFO) << "NIP-04 decryption (mock implementation)";
  
  size_t iv_pos = ciphertext.find("?iv=");
  if (iv_pos != std::string::npos) {
    std::string hex_data = ciphertext.substr(0, iv_pos);
    std::string plaintext;
    if (base::HexStringToString(hex_data, &plaintext)) {
      return plaintext;
    }
  }
  
  return "decryption_failed";
}

}  // namespace nostr