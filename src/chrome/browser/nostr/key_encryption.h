// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_KEY_ENCRYPTION_H_
#define CHROME_BROWSER_NOSTR_KEY_ENCRYPTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "chrome/browser/nostr/key_storage_interface.h"

namespace nostr {

// Utility class for encrypting and decrypting Nostr private keys
class KeyEncryption {
 public:
  // Default parameters for key derivation
  static constexpr size_t kDefaultSaltLength = 32;  // 256 bits
  static constexpr size_t kDefaultIVLength = 12;    // 96 bits for AES-GCM
  static constexpr size_t kDefaultKeyLength = 32;   // 256 bits
  static constexpr size_t kDefaultTagLength = 16;   // 128 bits
  static constexpr uint32_t kDefaultPBKDF2Iterations = 100000;
  
  KeyEncryption();
  ~KeyEncryption();
  
  // Encrypt a private key using a passphrase
  // Returns an EncryptedKey structure with all necessary data for decryption
  std::optional<EncryptedKey> EncryptKey(
      base::span<const uint8_t> private_key,
      const std::string& passphrase);
  
  // Decrypt a private key using a passphrase
  // Returns the decrypted private key data
  std::optional<std::vector<uint8_t>> DecryptKey(
      const EncryptedKey& encrypted_key,
      const std::string& passphrase);
  
  // Generate a random salt for key derivation
  std::vector<uint8_t> GenerateSalt();
  
  // Generate a random IV for AES-GCM
  std::vector<uint8_t> GenerateIV();
  
  // Derive an encryption key from a passphrase using PBKDF2
  std::optional<std::vector<uint8_t>> DeriveKey(
      const std::string& passphrase,
      base::span<const uint8_t> salt,
      uint32_t iterations = kDefaultPBKDF2Iterations);
  
  // Validate that a passphrase meets minimum security requirements
  bool ValidatePassphrase(const std::string& passphrase);
  
  // Get minimum passphrase length requirement
  static constexpr size_t GetMinPassphraseLength() { return 12; }
  
  // Static utility methods for data encryption/decryption
  // Used by file fallback storage
  static std::optional<std::vector<uint8_t>> EncryptData(
      base::span<const uint8_t> data,
      const std::string& key);
  
  static std::optional<std::vector<uint8_t>> DecryptData(
      base::span<const uint8_t> encrypted_data,
      const std::string& key);
  
 private:
  // Perform AES-GCM encryption
  std::optional<std::vector<uint8_t>> EncryptAESGCM(
      base::span<const uint8_t> plaintext,
      base::span<const uint8_t> key,
      base::span<const uint8_t> iv,
      base::span<uint8_t> auth_tag);
  
  // Perform AES-GCM decryption
  std::optional<std::vector<uint8_t>> DecryptAESGCM(
      base::span<const uint8_t> ciphertext,
      base::span<const uint8_t> key,
      base::span<const uint8_t> iv,
      base::span<const uint8_t> auth_tag);
  
  DISALLOW_COPY_AND_ASSIGN(KeyEncryption);
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_KEY_ENCRYPTION_H_