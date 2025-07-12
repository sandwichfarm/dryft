// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/key_encryption.h"

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {

class KeyEncryptionTest : public testing::Test {
 protected:
  KeyEncryption key_encryption_;
  
  // Test private key (32 bytes)
  std::vector<uint8_t> test_private_key_{
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
  };
  
  std::string valid_passphrase_ = "TestPassphrase123!";
  std::string weak_passphrase_ = "weak";
};

TEST_F(KeyEncryptionTest, ValidatePassphrase) {
  // Valid passphrase
  EXPECT_TRUE(key_encryption_.ValidatePassphrase(valid_passphrase_));
  
  // Too short
  EXPECT_FALSE(key_encryption_.ValidatePassphrase("Short1!"));
  
  // No uppercase
  EXPECT_FALSE(key_encryption_.ValidatePassphrase("testpassphrase123!"));
  
  // No lowercase
  EXPECT_FALSE(key_encryption_.ValidatePassphrase("TESTPASSPHRASE123!"));
  
  // No digit
  EXPECT_FALSE(key_encryption_.ValidatePassphrase("TestPassphrase!"));
  
  // Empty
  EXPECT_FALSE(key_encryption_.ValidatePassphrase(""));
}

TEST_F(KeyEncryptionTest, GenerateSalt) {
  auto salt1 = key_encryption_.GenerateSalt();
  auto salt2 = key_encryption_.GenerateSalt();
  
  // Should be correct length
  EXPECT_EQ(salt1.size(), KeyEncryption::kDefaultSaltLength);
  EXPECT_EQ(salt2.size(), KeyEncryption::kDefaultSaltLength);
  
  // Should be different (random)
  EXPECT_NE(salt1, salt2);
}

TEST_F(KeyEncryptionTest, GenerateIV) {
  auto iv1 = key_encryption_.GenerateIV();
  auto iv2 = key_encryption_.GenerateIV();
  
  // Should be correct length
  EXPECT_EQ(iv1.size(), KeyEncryption::kDefaultIVLength);
  EXPECT_EQ(iv2.size(), KeyEncryption::kDefaultIVLength);
  
  // Should be different (random)
  EXPECT_NE(iv1, iv2);
}

TEST_F(KeyEncryptionTest, DeriveKey) {
  auto salt = key_encryption_.GenerateSalt();
  
  auto key1 = key_encryption_.DeriveKey(valid_passphrase_, salt);
  auto key2 = key_encryption_.DeriveKey(valid_passphrase_, salt);
  
  ASSERT_TRUE(key1.has_value());
  ASSERT_TRUE(key2.has_value());
  
  // Same passphrase and salt should produce same key
  EXPECT_EQ(*key1, *key2);
  EXPECT_EQ(key1->size(), KeyEncryption::kDefaultKeyLength);
  
  // Different passphrase should produce different key
  auto key3 = key_encryption_.DeriveKey("DifferentPass123!", salt);
  ASSERT_TRUE(key3.has_value());
  EXPECT_NE(*key1, *key3);
  
  // Different salt should produce different key
  auto salt2 = key_encryption_.GenerateSalt();
  auto key4 = key_encryption_.DeriveKey(valid_passphrase_, salt2);
  ASSERT_TRUE(key4.has_value());
  EXPECT_NE(*key1, *key4);
}

TEST_F(KeyEncryptionTest, EncryptDecryptKey) {
  // Encrypt the test key
  auto encrypted = key_encryption_.EncryptKey(test_private_key_, valid_passphrase_);
  ASSERT_TRUE(encrypted.has_value());
  
  // Verify encrypted data structure
  EXPECT_FALSE(encrypted->encrypted_data.empty());
  EXPECT_EQ(encrypted->salt.size(), KeyEncryption::kDefaultSaltLength);
  EXPECT_EQ(encrypted->iv.size(), KeyEncryption::kDefaultIVLength);
  EXPECT_EQ(encrypted->auth_tag.size(), KeyEncryption::kDefaultTagLength);
  EXPECT_EQ(encrypted->kdf_algorithm, "PBKDF2-SHA256");
  EXPECT_EQ(encrypted->kdf_iterations, KeyEncryption::kDefaultPBKDF2Iterations);
  EXPECT_EQ(encrypted->encryption_algorithm, "AES-256-GCM");
  
  // Encrypted data should be different from plaintext
  EXPECT_NE(encrypted->encrypted_data, test_private_key_);
  
  // Decrypt with correct passphrase
  auto decrypted = key_encryption_.DecryptKey(*encrypted, valid_passphrase_);
  ASSERT_TRUE(decrypted.has_value());
  EXPECT_EQ(*decrypted, test_private_key_);
  
  // Decrypt with wrong passphrase should fail
  auto decrypted_wrong = key_encryption_.DecryptKey(*encrypted, "WrongPass123!");
  EXPECT_FALSE(decrypted_wrong.has_value());
}

TEST_F(KeyEncryptionTest, EncryptWithInvalidInput) {
  // Empty private key
  std::vector<uint8_t> empty_key;
  auto encrypted = key_encryption_.EncryptKey(empty_key, valid_passphrase_);
  EXPECT_FALSE(encrypted.has_value());
  
  // Invalid passphrase
  auto encrypted2 = key_encryption_.EncryptKey(test_private_key_, weak_passphrase_);
  EXPECT_FALSE(encrypted2.has_value());
}

TEST_F(KeyEncryptionTest, DecryptWithTamperedData) {
  auto encrypted = key_encryption_.EncryptKey(test_private_key_, valid_passphrase_);
  ASSERT_TRUE(encrypted.has_value());
  
  // Tamper with encrypted data
  encrypted->encrypted_data[0] ^= 0xFF;
  auto decrypted = key_encryption_.DecryptKey(*encrypted, valid_passphrase_);
  EXPECT_FALSE(decrypted.has_value());
  
  // Restore and tamper with auth tag
  encrypted->encrypted_data[0] ^= 0xFF;
  encrypted->auth_tag[0] ^= 0xFF;
  auto decrypted2 = key_encryption_.DecryptKey(*encrypted, valid_passphrase_);
  EXPECT_FALSE(decrypted2.has_value());
}

TEST_F(KeyEncryptionTest, MultipleKeysWithSamePassphrase) {
  // Encrypt multiple keys with the same passphrase
  std::vector<uint8_t> key1(32, 0x11);
  std::vector<uint8_t> key2(32, 0x22);
  
  auto encrypted1 = key_encryption_.EncryptKey(key1, valid_passphrase_);
  auto encrypted2 = key_encryption_.EncryptKey(key2, valid_passphrase_);
  
  ASSERT_TRUE(encrypted1.has_value());
  ASSERT_TRUE(encrypted2.has_value());
  
  // Different salts and IVs should be used
  EXPECT_NE(encrypted1->salt, encrypted2->salt);
  EXPECT_NE(encrypted1->iv, encrypted2->iv);
  EXPECT_NE(encrypted1->encrypted_data, encrypted2->encrypted_data);
  
  // Both should decrypt correctly
  auto decrypted1 = key_encryption_.DecryptKey(*encrypted1, valid_passphrase_);
  auto decrypted2 = key_encryption_.DecryptKey(*encrypted2, valid_passphrase_);
  
  ASSERT_TRUE(decrypted1.has_value());
  ASSERT_TRUE(decrypted2.has_value());
  EXPECT_EQ(*decrypted1, key1);
  EXPECT_EQ(*decrypted2, key2);
}

}  // namespace nostr