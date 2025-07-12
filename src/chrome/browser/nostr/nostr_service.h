// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NOSTR_SERVICE_H_
#define CHROME_BROWSER_NOSTR_NOSTR_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

class Profile;

// Forward declarations for Nostr structures
struct NostrRelayPolicy;
struct NostrRateLimitInfo;

namespace nostr {

class KeyStorage;
class NostrPermissionManager;

// Main service for Nostr operations in Tungsten Browser
// Provides cryptographic operations, key management, and NIP-07 compliance
class NostrService : public KeyedService {
 public:
  // Callback types for async operations
  using PublicKeyCallback = base::OnceCallback<void(bool success, const std::string& result)>;
  using SignEventCallback = base::OnceCallback<void(bool success, const base::Value::Dict& result)>;
  using EncryptCallback = base::OnceCallback<void(bool success, const std::string& result)>;
  using DecryptCallback = base::OnceCallback<void(bool success, const std::string& result)>;
  using PermissionCallback = base::OnceCallback<void(bool granted, bool remember)>;

  explicit NostrService(Profile* profile);
  ~NostrService() override;

  // NIP-07 Core Methods
  
  /**
   * Get the public key for the current/default Nostr identity.
   * @return hex-encoded public key (32 bytes)
   */
  std::string GetPublicKey();

  /**
   * Sign a Nostr event with the private key.
   * @param unsigned_event Event object without signature
   * @param callback Callback with signed event or error
   */
  void SignEvent(const base::Value::Dict& unsigned_event,
                 SignEventCallback callback);

  /**
   * Get the current relay policy (read/write permissions per relay).
   * @return NostrRelayPolicy structure with relay configurations
   */
  NostrRelayPolicy GetRelayPolicy();

  /**
   * Encrypt plaintext using NIP-04 (ECDH + AES-256-CBC).
   * @param pubkey Recipient's public key (hex)
   * @param plaintext Message to encrypt
   * @param callback Callback with encrypted result or error
   */
  void Nip04Encrypt(const std::string& pubkey,
                    const std::string& plaintext,
                    EncryptCallback callback);

  /**
   * Decrypt ciphertext using NIP-04 (ECDH + AES-256-CBC).
   * @param pubkey Sender's public key (hex)
   * @param ciphertext Encrypted message
   * @param callback Callback with decrypted result or error
   */
  void Nip04Decrypt(const std::string& pubkey,
                    const std::string& ciphertext,
                    DecryptCallback callback);

  /**
   * Encrypt plaintext using NIP-44 (newer encryption standard).
   * @param pubkey Recipient's public key (hex)
   * @param plaintext Message to encrypt
   * @param callback Callback with encrypted result or error
   */
  void Nip44Encrypt(const std::string& pubkey,
                    const std::string& plaintext,
                    EncryptCallback callback);

  /**
   * Decrypt ciphertext using NIP-44 (newer encryption standard).
   * @param pubkey Sender's public key (hex)
   * @param ciphertext Encrypted message
   * @param callback Callback with decrypted result or error
   */
  void Nip44Decrypt(const std::string& pubkey,
                    const std::string& ciphertext,
                    DecryptCallback callback);

  // Permission System Integration

  /**
   * Check if origin has permission for a specific method.
   * @param origin The requesting origin
   * @param method The NIP-07 method name
   * @return true if permission granted
   */
  bool HasPermission(const url::Origin& origin, const std::string& method);

  /**
   * Request permission from user for an operation.
   * @param request Permission request details
   * @param callback Callback with grant result
   */
  void RequestPermission(const struct NostrPermissionRequest& request,
                        PermissionCallback callback);

  /**
   * Get current rate limiting info for an origin and method.
   * @param origin The requesting origin  
   * @param method The method being checked
   * @return Current rate limit status
   */
  NostrRateLimitInfo GetRateLimitInfo(const url::Origin& origin,
                                     const std::string& method);

  // Key Management

  /**
   * Generate a new Nostr keypair and store securely.
   * @return public key of generated pair (hex)
   */
  std::string GenerateNewKey();

  /**
   * Import an existing private key.
   * @param private_key_hex Private key in hex format
   * @return public key of imported pair (hex)
   */
  std::string ImportKey(const std::string& private_key_hex);

  /**
   * List all available public keys.
   * @return vector of public key hex strings
   */
  std::vector<std::string> ListKeys();

  /**
   * Set the default key for signing operations.
   * @param public_key_hex Public key to set as default
   * @return true if key was set successfully
   */
  bool SetDefaultKey(const std::string& public_key_hex);

  // KeyedService implementation
  void Shutdown() override;

 private:
  // Internal implementation methods
  
  // Initialize cryptographic subsystems
  void InitializeCrypto();
  
  // Validate Nostr event structure
  bool ValidateEvent(const base::Value::Dict& event);
  
  // Add signature to event
  base::Value::Dict SignEventInternal(const base::Value::Dict& unsigned_event);
  
  // Compute event ID (SHA-256 of canonical serialization)  
  std::string ComputeEventId(const base::Value::Dict& event);
  
  // Serialize event for signing (canonical JSON)
  std::string SerializeEventForSigning(const base::Value::Dict& event);
  
  // ECDH key agreement for encryption
  std::vector<uint8_t> ComputeSharedSecret(const std::string& pubkey_hex);
  
  // NIP-04 encryption primitives
  std::string Nip04EncryptInternal(const std::vector<uint8_t>& shared_secret,
                                  const std::string& plaintext);
  std::string Nip04DecryptInternal(const std::vector<uint8_t>& shared_secret,
                                  const std::string& ciphertext);

  // Profile for service isolation
  Profile* profile_;
  
  // Key storage backend
  std::unique_ptr<KeyStorage> key_storage_;
  
  // Permission manager
  NostrPermissionManager* permission_manager_;
  
  // Current default key (cached)
  std::string default_public_key_;
  
  // Weak pointer factory for async callbacks
  base::WeakPtrFactory<NostrService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NostrService);
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NOSTR_SERVICE_H_