// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/key_encryption.h"
#include "chrome/browser/nostr/key_storage_factory.h"
#include "chrome/browser/nostr/key_storage_interface.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {

class KeyStorageIntegrationTest : public testing::Test {
 protected:
  void SetUp() override {
    // Create in-memory storage for testing
    storage_ = KeyStorageFactory::CreateKeyStorage(
        nullptr, KeyStorageFactory::StorageBackend::IN_MEMORY);
    ASSERT_TRUE(storage_);
  }
  
  // Helper to create a test key identifier
  KeyIdentifier CreateTestKeyIdentifier(const std::string& id,
                                       const std::string& name) {
    KeyIdentifier key_id;
    key_id.id = id;
    key_id.name = name;
    key_id.public_key = "02" + std::string(62, 'a');  // Mock public key
    key_id.created_at = base::Time::Now();
    key_id.last_used_at = base::Time::Now();
    key_id.relay_urls = {"wss://relay1.example.com", "wss://relay2.example.com"};
    key_id.is_default = false;
    return key_id;
  }
  
  // Test private key (32 bytes)
  std::vector<uint8_t> test_private_key_{
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
  };
  
  std::string passphrase_ = "TestPassphrase123!";
  std::unique_ptr<KeyStorage> storage_;
  KeyEncryption key_encryption_;
};

TEST_F(KeyStorageIntegrationTest, StoreAndRetrieveKey) {
  // Create key identifier
  auto key_id = CreateTestKeyIdentifier("test_key_1", "Test Key 1");
  
  // Encrypt the private key
  auto encrypted = key_encryption_.EncryptKey(test_private_key_, passphrase_);
  ASSERT_TRUE(encrypted.has_value());
  
  // Store the encrypted key
  EXPECT_TRUE(storage_->StoreKey(key_id, *encrypted));
  
  // Verify key exists
  EXPECT_TRUE(storage_->HasKey("test_key_1"));
  
  // Retrieve the key
  auto retrieved = storage_->RetrieveKey(key_id);
  ASSERT_TRUE(retrieved.has_value());
  
  // Decrypt and verify
  auto decrypted = key_encryption_.DecryptKey(*retrieved, passphrase_);
  ASSERT_TRUE(decrypted.has_value());
  EXPECT_EQ(*decrypted, test_private_key_);
}

TEST_F(KeyStorageIntegrationTest, ListKeys) {
  // Store multiple keys
  for (int i = 0; i < 3; ++i) {
    auto key_id = CreateTestKeyIdentifier(
        "key_" + std::to_string(i),
        "Key " + std::to_string(i));
    
    auto encrypted = key_encryption_.EncryptKey(test_private_key_, passphrase_);
    ASSERT_TRUE(encrypted.has_value());
    EXPECT_TRUE(storage_->StoreKey(key_id, *encrypted));
  }
  
  // List all keys
  auto keys = storage_->ListKeys();
  EXPECT_EQ(keys.size(), 3u);
  
  // Verify key IDs
  std::set<std::string> key_ids;
  for (const auto& key : keys) {
    key_ids.insert(key.id);
  }
  EXPECT_TRUE(key_ids.count("key_0"));
  EXPECT_TRUE(key_ids.count("key_1"));
  EXPECT_TRUE(key_ids.count("key_2"));
}

TEST_F(KeyStorageIntegrationTest, DeleteKey) {
  auto key_id = CreateTestKeyIdentifier("delete_test", "Delete Test");
  
  auto encrypted = key_encryption_.EncryptKey(test_private_key_, passphrase_);
  ASSERT_TRUE(encrypted.has_value());
  
  // Store key
  EXPECT_TRUE(storage_->StoreKey(key_id, *encrypted));
  EXPECT_TRUE(storage_->HasKey("delete_test"));
  
  // Delete key
  EXPECT_TRUE(storage_->DeleteKey(key_id));
  EXPECT_FALSE(storage_->HasKey("delete_test"));
  
  // Verify it's gone
  auto retrieved = storage_->RetrieveKey(key_id);
  EXPECT_FALSE(retrieved.has_value());
  
  // Delete non-existent key should return false
  EXPECT_FALSE(storage_->DeleteKey(key_id));
}

TEST_F(KeyStorageIntegrationTest, DefaultKeyManagement) {
  // First key should become default automatically
  auto key1 = CreateTestKeyIdentifier("key1", "Key 1");
  auto encrypted1 = key_encryption_.EncryptKey(test_private_key_, passphrase_);
  ASSERT_TRUE(encrypted1.has_value());
  EXPECT_TRUE(storage_->StoreKey(key1, *encrypted1));
  
  auto default_key = storage_->GetDefaultKey();
  ASSERT_TRUE(default_key.has_value());
  EXPECT_EQ(default_key->id, "key1");
  
  // Add second key
  auto key2 = CreateTestKeyIdentifier("key2", "Key 2");
  auto encrypted2 = key_encryption_.EncryptKey(test_private_key_, passphrase_);
  ASSERT_TRUE(encrypted2.has_value());
  EXPECT_TRUE(storage_->StoreKey(key2, *encrypted2));
  
  // Default should still be key1
  default_key = storage_->GetDefaultKey();
  ASSERT_TRUE(default_key.has_value());
  EXPECT_EQ(default_key->id, "key1");
  
  // Change default to key2
  EXPECT_TRUE(storage_->SetDefaultKey("key2"));
  default_key = storage_->GetDefaultKey();
  ASSERT_TRUE(default_key.has_value());
  EXPECT_EQ(default_key->id, "key2");
  
  // Delete default key
  EXPECT_TRUE(storage_->DeleteKey(key2));
  
  // First remaining key should become default
  default_key = storage_->GetDefaultKey();
  ASSERT_TRUE(default_key.has_value());
  EXPECT_EQ(default_key->id, "key1");
}

TEST_F(KeyStorageIntegrationTest, UpdateKeyMetadata) {
  auto key_id = CreateTestKeyIdentifier("update_test", "Original Name");
  
  auto encrypted = key_encryption_.EncryptKey(test_private_key_, passphrase_);
  ASSERT_TRUE(encrypted.has_value());
  EXPECT_TRUE(storage_->StoreKey(key_id, *encrypted));
  
  // Update metadata
  key_id.name = "Updated Name";
  key_id.relay_urls.push_back("wss://relay3.example.com");
  EXPECT_TRUE(storage_->UpdateKeyMetadata(key_id));
  
  // Verify update
  auto keys = storage_->ListKeys();
  ASSERT_EQ(keys.size(), 1u);
  EXPECT_EQ(keys[0].name, "Updated Name");
  EXPECT_EQ(keys[0].relay_urls.size(), 3u);
}

TEST_F(KeyStorageIntegrationTest, MultiplePassphrases) {
  // Create two keys with different passphrases
  auto key1 = CreateTestKeyIdentifier("key1", "Key 1");
  auto encrypted1 = key_encryption_.EncryptKey(test_private_key_, "Password123!");
  ASSERT_TRUE(encrypted1.has_value());
  EXPECT_TRUE(storage_->StoreKey(key1, *encrypted1));
  
  auto key2 = CreateTestKeyIdentifier("key2", "Key 2");
  auto encrypted2 = key_encryption_.EncryptKey(test_private_key_, "DifferentPass456!");
  ASSERT_TRUE(encrypted2.has_value());
  EXPECT_TRUE(storage_->StoreKey(key2, *encrypted2));
  
  // Retrieve and decrypt with correct passphrases
  auto retrieved1 = storage_->RetrieveKey(key1);
  ASSERT_TRUE(retrieved1.has_value());
  auto decrypted1 = key_encryption_.DecryptKey(*retrieved1, "Password123!");
  ASSERT_TRUE(decrypted1.has_value());
  EXPECT_EQ(*decrypted1, test_private_key_);
  
  auto retrieved2 = storage_->RetrieveKey(key2);
  ASSERT_TRUE(retrieved2.has_value());
  auto decrypted2 = key_encryption_.DecryptKey(*retrieved2, "DifferentPass456!");
  ASSERT_TRUE(decrypted2.has_value());
  EXPECT_EQ(*decrypted2, test_private_key_);
  
  // Wrong passphrase should fail
  auto wrong_decrypt = key_encryption_.DecryptKey(*retrieved1, "DifferentPass456!");
  EXPECT_FALSE(wrong_decrypt.has_value());
}

}  // namespace nostr