// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/key_encryption.h"

#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "crypto/openssl_util.h"

namespace nostr {

KeyEncryption::KeyEncryption() {
  crypto::EnsureOpenSSLInit();
}

KeyEncryption::~KeyEncryption() = default;

std::optional<EncryptedKey> KeyEncryption::EncryptKey(
    base::span<const uint8_t> private_key,
    const std::string& passphrase) {
  if (private_key.empty() || !ValidatePassphrase(passphrase)) {
    LOG(ERROR) << "Invalid private key or passphrase";
    return std::nullopt;
  }
  
  EncryptedKey result;
  result.kdf_algorithm = "PBKDF2-SHA256";
  result.kdf_iterations = kDefaultPBKDF2Iterations;
  result.encryption_algorithm = "AES-256-GCM";
  
  // Generate random salt and IV
  result.salt = GenerateSalt();
  result.iv = GenerateIV();
  
  // Derive encryption key from passphrase
  auto derived_key = DeriveKey(passphrase, result.salt, result.kdf_iterations);
  if (!derived_key) {
    LOG(ERROR) << "Failed to derive encryption key";
    return std::nullopt;
  }
  
  // Prepare auth tag buffer
  result.auth_tag.resize(kDefaultTagLength);
  
  // Encrypt the private key
  auto encrypted = EncryptAESGCM(private_key, *derived_key, result.iv, result.auth_tag);
  if (!encrypted) {
    LOG(ERROR) << "Failed to encrypt private key";
    return std::nullopt;
  }
  
  result.encrypted_data = std::move(*encrypted);
  return result;
}

std::optional<std::vector<uint8_t>> KeyEncryption::DecryptKey(
    const EncryptedKey& encrypted_key,
    const std::string& passphrase) {
  if (!ValidatePassphrase(passphrase)) {
    LOG(ERROR) << "Invalid passphrase";
    return std::nullopt;
  }
  
  // Verify encryption algorithm
  if (encrypted_key.encryption_algorithm != "AES-256-GCM") {
    LOG(ERROR) << "Unsupported encryption algorithm: " << encrypted_key.encryption_algorithm;
    return std::nullopt;
  }
  
  // Derive decryption key from passphrase
  auto derived_key = DeriveKey(passphrase, encrypted_key.salt, 
                              encrypted_key.kdf_iterations);
  if (!derived_key) {
    LOG(ERROR) << "Failed to derive decryption key";
    return std::nullopt;
  }
  
  // Decrypt the private key
  return DecryptAESGCM(encrypted_key.encrypted_data, *derived_key,
                      encrypted_key.iv, encrypted_key.auth_tag);
}

std::vector<uint8_t> KeyEncryption::GenerateSalt() {
  std::vector<uint8_t> salt(kDefaultSaltLength);
  if (RAND_bytes(salt.data(), salt.size()) != 1) {
    LOG(ERROR) << "Failed to generate random salt";
    salt.clear();
  }
  return salt;
}

std::vector<uint8_t> KeyEncryption::GenerateIV() {
  std::vector<uint8_t> iv(kDefaultIVLength);
  if (RAND_bytes(iv.data(), iv.size()) != 1) {
    LOG(ERROR) << "Failed to generate random IV";
    iv.clear();
  }
  return iv;
}

std::optional<std::vector<uint8_t>> KeyEncryption::DeriveKey(
    const std::string& passphrase,
    base::span<const uint8_t> salt,
    uint32_t iterations) {
  std::vector<uint8_t> key(kDefaultKeyLength);
  
  if (PKCS5_PBKDF2_HMAC(passphrase.c_str(), passphrase.length(),
                        salt.data(), salt.size(),
                        iterations, EVP_sha256(),
                        key.size(), key.data()) != 1) {
    LOG(ERROR) << "Failed to derive key using PBKDF2";
    return std::nullopt;
  }
  
  return key;
}

bool KeyEncryption::ValidatePassphrase(const std::string& passphrase) {
  if (passphrase.length() < GetMinPassphraseLength()) {
    LOG(WARNING) << "Passphrase too short (minimum " << GetMinPassphraseLength() << " characters)";
    return false;
  }
  
  // Check for basic complexity requirements
  bool has_upper = false;
  bool has_lower = false;
  bool has_digit = false;
  
  for (char c : passphrase) {
    if (base::IsAsciiUpper(c)) has_upper = true;
    if (base::IsAsciiLower(c)) has_lower = true;
    if (base::IsAsciiDigit(c)) has_digit = true;
  }
  
  if (!has_upper || !has_lower || !has_digit) {
    LOG(WARNING) << "Passphrase must contain uppercase, lowercase, and numeric characters";
    return false;
  }
  
  return true;
}

std::optional<std::vector<uint8_t>> KeyEncryption::EncryptAESGCM(
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> key,
    base::span<const uint8_t> iv,
    base::span<uint8_t> auth_tag) {
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    LOG(ERROR) << "Failed to create cipher context";
    return std::nullopt;
  }
  
  std::vector<uint8_t> ciphertext(plaintext.size());
  int len;
  int ciphertext_len = 0;
  
  // Initialize encryption operation
  if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to initialize AES-GCM encryption";
    return std::nullopt;
  }
  
  // Set IV length
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to set IV length";
    return std::nullopt;
  }
  
  // Initialize key and IV
  if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to set key and IV";
    return std::nullopt;
  }
  
  // Encrypt the plaintext
  if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, 
                       plaintext.data(), plaintext.size()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to encrypt data";
    return std::nullopt;
  }
  ciphertext_len = len;
  
  // Finalize encryption
  if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + ciphertext_len, &len) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to finalize encryption";
    return std::nullopt;
  }
  ciphertext_len += len;
  
  // Get authentication tag
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, auth_tag.size(), 
                         auth_tag.data()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to get authentication tag";
    return std::nullopt;
  }
  
  EVP_CIPHER_CTX_free(ctx);
  ciphertext.resize(ciphertext_len);
  return ciphertext;
}

std::optional<std::vector<uint8_t>> KeyEncryption::DecryptAESGCM(
    base::span<const uint8_t> ciphertext,
    base::span<const uint8_t> key,
    base::span<const uint8_t> iv,
    base::span<const uint8_t> auth_tag) {
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    LOG(ERROR) << "Failed to create cipher context";
    return std::nullopt;
  }
  
  std::vector<uint8_t> plaintext(ciphertext.size());
  int len;
  int plaintext_len = 0;
  
  // Initialize decryption operation
  if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to initialize AES-GCM decryption";
    return std::nullopt;
  }
  
  // Set IV length
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to set IV length";
    return std::nullopt;
  }
  
  // Initialize key and IV
  if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to set key and IV";
    return std::nullopt;
  }
  
  // Decrypt the ciphertext
  if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, 
                       ciphertext.data(), ciphertext.size()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to decrypt data";
    return std::nullopt;
  }
  plaintext_len = len;
  
  // Set authentication tag
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, auth_tag.size(),
                         const_cast<uint8_t*>(auth_tag.data())) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    LOG(ERROR) << "Failed to set authentication tag";
    return std::nullopt;
  }
  
  // Finalize decryption and verify tag
  int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + plaintext_len, &len);
  EVP_CIPHER_CTX_free(ctx);
  
  if (ret != 1) {
    LOG(ERROR) << "Failed to verify authentication tag";
    return std::nullopt;
  }
  
  plaintext_len += len;
  plaintext.resize(plaintext_len);
  return plaintext;
}

// static
std::optional<std::vector<uint8_t>> KeyEncryption::EncryptData(
    base::span<const uint8_t> data,
    const std::string& key) {
  KeyEncryption encryptor;
  
  // Use a simplified encryption approach for file storage
  // Generate salt and derive encryption key
  auto salt = encryptor.GenerateSalt();
  auto derived_key = encryptor.DeriveKey(key, salt);
  if (!derived_key) {
    return std::nullopt;
  }
  
  // Generate IV
  auto iv = encryptor.GenerateIV();
  
  // Perform encryption
  std::vector<uint8_t> auth_tag(kDefaultTagLength);
  auto ciphertext = encryptor.EncryptAESGCM(data, *derived_key, iv, auth_tag);
  if (!ciphertext) {
    return std::nullopt;
  }
  
  // Combine salt + iv + auth_tag + ciphertext
  std::vector<uint8_t> result;
  result.reserve(salt.size() + iv.size() + auth_tag.size() + ciphertext->size());
  
  result.insert(result.end(), salt.begin(), salt.end());
  result.insert(result.end(), iv.begin(), iv.end());
  result.insert(result.end(), auth_tag.begin(), auth_tag.end());
  result.insert(result.end(), ciphertext->begin(), ciphertext->end());
  
  return result;
}

// static
std::optional<std::vector<uint8_t>> KeyEncryption::DecryptData(
    base::span<const uint8_t> encrypted_data,
    const std::string& key) {
  if (encrypted_data.size() < kDefaultSaltLength + kDefaultIVLength + kDefaultTagLength) {
    LOG(ERROR) << "Encrypted data too small";
    return std::nullopt;
  }
  
  KeyEncryption encryptor;
  
  // Extract components
  auto salt = encrypted_data.subspan(0, kDefaultSaltLength);
  auto iv = encrypted_data.subspan(kDefaultSaltLength, kDefaultIVLength);
  auto auth_tag = encrypted_data.subspan(kDefaultSaltLength + kDefaultIVLength, kDefaultTagLength);
  auto ciphertext = encrypted_data.subspan(kDefaultSaltLength + kDefaultIVLength + kDefaultTagLength);
  
  // Derive decryption key
  auto derived_key = encryptor.DeriveKey(key, salt);
  if (!derived_key) {
    return std::nullopt;
  }
  
  // Perform decryption
  return encryptor.DecryptAESGCM(ciphertext, *derived_key, iv, auth_tag);
}

}  // namespace nostr