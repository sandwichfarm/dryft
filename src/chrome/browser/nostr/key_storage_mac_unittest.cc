// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)

#include "chrome/browser/nostr/key_storage_mac.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/nostr/key_encryption.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {

class KeyStorageMacTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    storage_ = std::make_unique<KeyStorageMac>(profile_.get());
    
    // Clean up any existing test credentials
    CleanupTestCredentials();
  }
  
  void TearDown() override {
    // Clean up test credentials
    CleanupTestCredentials();
    
    storage_.reset();
    profile_.reset();
  }
  
  // Helper to create a test key identifier
  KeyIdentifier CreateTestKeyIdentifier(const std::string& id,
                                       const std::string& name) {
    KeyIdentifier key_id;
    key_id.id = "test_mac_" + id;  // Prefix to avoid conflicts
    key_id.name = name;
    key_id.public_key = "02" + std::string(62, 'b');  // Mock public key
    key_id.created_at = base::Time::Now();
    key_id.last_used_at = base::Time::Now();
    key_id.relay_urls = {"wss://relay1.mac.test", "wss://relay2.mac.test"};
    key_id.is_default = false;
    return key_id;
  }
  
  // Clean up any test credentials from previous runs
  void CleanupTestCredentials() {
    auto keys = storage_->ListKeys();
    for (const auto& key : keys) {
      if (key.id.find("test_mac_") == 0) {
        storage_->DeleteKey(key);
      }
    }
  }
  
  // Test private key (32 bytes)
  std::vector<uint8_t> test_private_key_{
    0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
    0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
    0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
    0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe
  };
  
  std::string passphrase_ = "MacTestPassphrase123!";
  
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<KeyStorageMac> storage_;
  KeyEncryption key_encryption_;
};

TEST_F(KeyStorageMacTest, StoreAndRetrieveKey) {
  // Create key identifier
  auto key_id = CreateTestKeyIdentifier("key1", "Mac Test Key 1");
  
  // Encrypt the private key
  auto encrypted = key_encryption_.EncryptKey(test_private_key_, passphrase_);
  ASSERT_TRUE(encrypted.has_value());
  
  // Store the encrypted key
  EXPECT_TRUE(storage_->StoreKey(key_id, *encrypted));
  
  // Verify key exists
  EXPECT_TRUE(storage_->HasKey(key_id.id));
  
  // Retrieve the key
  auto retrieved = storage_->RetrieveKey(key_id);
  ASSERT_TRUE(retrieved.has_value());
  
  // Decrypt and verify
  auto decrypted = key_encryption_.DecryptKey(*retrieved, passphrase_);
  ASSERT_TRUE(decrypted.has_value());
  EXPECT_EQ(*decrypted, test_private_key_);
}

TEST_F(KeyStorageMacTest, ListKeys) {
  // Store multiple keys
  for (int i = 0; i < 3; ++i) {
    auto key_id = CreateTestKeyIdentifier(
        "list_key_" + std::to_string(i),
        "Mac List Key " + std::to_string(i));
    
    auto encrypted = key_encryption_.EncryptKey(test_private_key_, passphrase_);
    ASSERT_TRUE(encrypted.has_value());
    EXPECT_TRUE(storage_->StoreKey(key_id, *encrypted));
  }
  
  // List all keys
  auto keys = storage_->ListKeys();
  
  // Should have at least our 3 test keys
  int test_key_count = 0;
  for (const auto& key : keys) {
    if (key.id.find("test_mac_list_key_") == 0) {
      test_key_count++;
    }
  }
  EXPECT_EQ(test_key_count, 3);
}

TEST_F(KeyStorageMacTest, DeleteKey) {
  auto key_id = CreateTestKeyIdentifier("delete_test", "Mac Delete Test");
  
  auto encrypted = key_encryption_.EncryptKey(test_private_key_, passphrase_);
  ASSERT_TRUE(encrypted.has_value());
  
  // Store key
  EXPECT_TRUE(storage_->StoreKey(key_id, *encrypted));
  EXPECT_TRUE(storage_->HasKey(key_id.id));
  
  // Delete key
  EXPECT_TRUE(storage_->DeleteKey(key_id));
  EXPECT_FALSE(storage_->HasKey(key_id.id));
  
  // Verify it's gone
  auto retrieved = storage_->RetrieveKey(key_id);
  EXPECT_FALSE(retrieved.has_value());
}

TEST_F(KeyStorageMacTest, DefaultKeyManagement) {
  // First key should become default automatically
  auto key1 = CreateTestKeyIdentifier("default_key1", "Mac Default Key 1");
  auto encrypted1 = key_encryption_.EncryptKey(test_private_key_, passphrase_);
  ASSERT_TRUE(encrypted1.has_value());
  EXPECT_TRUE(storage_->StoreKey(key1, *encrypted1));
  
  // Check if it's default
  auto default_key = storage_->GetDefaultKey();
  ASSERT_TRUE(default_key.has_value());
  EXPECT_EQ(default_key->id, key1.id);
  
  // Add second key
  auto key2 = CreateTestKeyIdentifier("default_key2", "Mac Default Key 2");
  auto encrypted2 = key_encryption_.EncryptKey(test_private_key_, passphrase_);
  ASSERT_TRUE(encrypted2.has_value());
  EXPECT_TRUE(storage_->StoreKey(key2, *encrypted2));
  
  // Default should still be key1
  default_key = storage_->GetDefaultKey();
  ASSERT_TRUE(default_key.has_value());
  EXPECT_EQ(default_key->id, key1.id);
  
  // Change default to key2
  EXPECT_TRUE(storage_->SetDefaultKey(key2.id));
  default_key = storage_->GetDefaultKey();
  ASSERT_TRUE(default_key.has_value());
  EXPECT_EQ(default_key->id, key2.id);
}

TEST_F(KeyStorageMacTest, DeleteDefaultKey) {
  // Create three keys
  auto key1 = CreateTestKeyIdentifier("del_default_key1", "Mac Delete Default 1");
  auto key2 = CreateTestKeyIdentifier("del_default_key2", "Mac Delete Default 2");
  auto key3 = CreateTestKeyIdentifier("del_default_key3", "Mac Delete Default 3");
  
  auto encrypted = key_encryption_.EncryptKey(test_private_key_, passphrase_);
  ASSERT_TRUE(encrypted.has_value());
  
  EXPECT_TRUE(storage_->StoreKey(key1, *encrypted));
  EXPECT_TRUE(storage_->StoreKey(key2, *encrypted));
  EXPECT_TRUE(storage_->StoreKey(key3, *encrypted));
  
  // Set key2 as default
  EXPECT_TRUE(storage_->SetDefaultKey(key2.id));
  auto default_key = storage_->GetDefaultKey();
  ASSERT_TRUE(default_key.has_value());
  EXPECT_EQ(default_key->id, key2.id);
  
  // Delete the default key
  EXPECT_TRUE(storage_->DeleteKey(key2));
  
  // Another key should become default
  default_key = storage_->GetDefaultKey();
  ASSERT_TRUE(default_key.has_value());
  // Should be either key1 or key3
  EXPECT_TRUE(default_key->id == key1.id || default_key->id == key3.id);
  
  // Verify key2 is gone
  EXPECT_FALSE(storage_->HasKey(key2.id));
}

TEST_F(KeyStorageMacTest, UpdateKeyMetadata) {
  auto key_id = CreateTestKeyIdentifier("update_test", "Mac Original Name");
  
  auto encrypted = key_encryption_.EncryptKey(test_private_key_, passphrase_);
  ASSERT_TRUE(encrypted.has_value());
  EXPECT_TRUE(storage_->StoreKey(key_id, *encrypted));
  
  // Update metadata
  key_id.name = "Mac Updated Name";
  key_id.relay_urls.push_back("wss://relay3.mac.test");
  base::Time original_created = key_id.created_at;
  
  // Wait a bit to ensure last_used_at will be different
  base::PlatformThread::Sleep(base::Milliseconds(10));
  
  EXPECT_TRUE(storage_->UpdateKeyMetadata(key_id));
  
  // Verify update
  auto keys = storage_->ListKeys();
  bool found = false;
  for (const auto& key : keys) {
    if (key.id == key_id.id) {
      found = true;
      EXPECT_EQ(key.name, "Mac Updated Name");
      EXPECT_EQ(key.relay_urls.size(), 3u);
      // Created time should be preserved
      EXPECT_EQ(key.created_at, original_created);
      // Last used should be updated
      EXPECT_GT(key.last_used_at, original_created);
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(KeyStorageMacTest, PersistenceAcrossInstances) {
  auto key_id = CreateTestKeyIdentifier("persist_test", "Mac Persistence Test");
  
  auto encrypted = key_encryption_.EncryptKey(test_private_key_, passphrase_);
  ASSERT_TRUE(encrypted.has_value());
  
  // Store with first instance
  EXPECT_TRUE(storage_->StoreKey(key_id, *encrypted));
  
  // Create new storage instance
  auto storage2 = std::make_unique<KeyStorageMac>(profile_.get());
  
  // Should be able to retrieve the key
  EXPECT_TRUE(storage2->HasKey(key_id.id));
  auto retrieved = storage2->RetrieveKey(key_id);
  ASSERT_TRUE(retrieved.has_value());
  
  // Verify it's the same key
  auto decrypted = key_encryption_.DecryptKey(*retrieved, passphrase_);
  ASSERT_TRUE(decrypted.has_value());
  EXPECT_EQ(*decrypted, test_private_key_);
}

TEST_F(KeyStorageMacTest, KeychainAccessDeniedHandling) {
  // This test would require simulating keychain access denial
  // which is difficult in unit tests. In real usage, the keychain
  // may prompt for access or deny based on user settings.
  // We can at least verify that operations don't crash.
  
  auto key_id = CreateTestKeyIdentifier("access_test", "Mac Access Test");
  auto encrypted = key_encryption_.EncryptKey(test_private_key_, passphrase_);
  ASSERT_TRUE(encrypted.has_value());
  
  // These operations should handle errors gracefully
  storage_->StoreKey(key_id, *encrypted);
  storage_->HasKey(key_id.id);
  storage_->RetrieveKey(key_id);
  storage_->DeleteKey(key_id);
}

TEST_F(KeyStorageMacTest, LargeKeyStorage) {
  // Test storing a key with maximum metadata
  auto key_id = CreateTestKeyIdentifier("large_test", "Mac Large Test");
  
  // Add many relay URLs
  for (int i = 0; i < 50; ++i) {
    key_id.relay_urls.push_back("wss://relay" + std::to_string(i) + ".mac.test");
  }
  
  auto encrypted = key_encryption_.EncryptKey(test_private_key_, passphrase_);
  ASSERT_TRUE(encrypted.has_value());
  
  // Should handle large metadata
  EXPECT_TRUE(storage_->StoreKey(key_id, *encrypted));
  
  // Verify retrieval
  auto retrieved = storage_->RetrieveKey(key_id);
  ASSERT_TRUE(retrieved.has_value());
  
  // Check metadata
  auto keys = storage_->ListKeys();
  bool found = false;
  for (const auto& key : keys) {
    if (key.id == key_id.id) {
      found = true;
      EXPECT_EQ(key.relay_urls.size(), 52u);  // 2 original + 50 added
      break;
    }
  }
  EXPECT_TRUE(found);
}

}  // namespace nostr

#endif  // BUILDFLAG(IS_MAC)