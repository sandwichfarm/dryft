// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/key_storage_linux.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "chrome/browser/nostr/file_fallback_storage.h"
#include "chrome/browser/nostr/key_encryption.h"
#include "chrome/browser/nostr/secret_service_client.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {

class KeyStorageLinuxTest : public testing::Test {
 public:
  KeyStorageLinuxTest() = default;
  ~KeyStorageLinuxTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_ = std::make_unique<TestingProfile>();
  }

  void TearDown() override {
    storage_.reset();
    profile_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<KeyStorageLinux> storage_;

  void CreateStorage() {
    storage_ = std::make_unique<KeyStorageLinux>(profile_.get());
  }

  KeyIdentifier CreateTestKey(const std::string& id = "test_key") {
    KeyIdentifier identifier;
    identifier.id = id;
    identifier.name = "Test Key";
    identifier.npub = "npub1test" + id;
    identifier.created_at = base::Time::Now();
    identifier.is_default = false;
    return identifier;
  }

  EncryptedKey CreateTestEncryptedKey() {
    // Create a test private key
    std::vector<uint8_t> private_key(32, 0xAB);  // Dummy key data
    std::string passphrase = "test_passphrase_123";
    
    KeyEncryption encryption;
    auto encrypted = encryption.EncryptKey(private_key, passphrase);
    EXPECT_TRUE(encrypted.has_value());
    return encrypted.value();
  }
};

TEST_F(KeyStorageLinuxTest, ConstructorDoesNotCrash) {
  CreateStorage();
  EXPECT_TRUE(storage_);
}

TEST_F(KeyStorageLinuxTest, DetectsDesktopEnvironment) {
  CreateStorage();
  
  // The desktop environment detection should not crash
  // We can't test specific environments without setting up the environment
  EXPECT_TRUE(storage_);
}

TEST_F(KeyStorageLinuxTest, StoreAndRetrieveKey) {
  CreateStorage();
  
  auto identifier = CreateTestKey();
  auto encrypted_key = CreateTestEncryptedKey();
  
  // Store the key
  EXPECT_TRUE(storage_->StoreKey(identifier, encrypted_key));
  
  // Retrieve the key
  auto retrieved = storage_->RetrieveKey(identifier);
  ASSERT_TRUE(retrieved.has_value());
  
  // Verify the data matches
  EXPECT_EQ(retrieved->encrypted_data, encrypted_key.encrypted_data);
  EXPECT_EQ(retrieved->salt, encrypted_key.salt);
  EXPECT_EQ(retrieved->iv, encrypted_key.iv);
  EXPECT_EQ(retrieved->algorithm, encrypted_key.algorithm);
}

TEST_F(KeyStorageLinuxTest, DeleteKey) {
  CreateStorage();
  
  auto identifier = CreateTestKey();
  auto encrypted_key = CreateTestEncryptedKey();
  
  // Store the key
  EXPECT_TRUE(storage_->StoreKey(identifier, encrypted_key));
  
  // Verify it exists
  EXPECT_TRUE(storage_->HasKey(identifier.id));
  
  // Delete the key
  EXPECT_TRUE(storage_->DeleteKey(identifier));
  
  // Verify it's gone
  EXPECT_FALSE(storage_->HasKey(identifier.id));
  auto retrieved = storage_->RetrieveKey(identifier);
  EXPECT_FALSE(retrieved.has_value());
}

TEST_F(KeyStorageLinuxTest, ListKeys) {
  CreateStorage();
  
  auto identifier1 = CreateTestKey("key1");
  auto identifier2 = CreateTestKey("key2");
  auto encrypted_key = CreateTestEncryptedKey();
  
  // Store two keys
  EXPECT_TRUE(storage_->StoreKey(identifier1, encrypted_key));
  EXPECT_TRUE(storage_->StoreKey(identifier2, encrypted_key));
  
  // List keys
  auto keys = storage_->ListKeys();
  EXPECT_EQ(keys.size(), 2u);
  
  // Verify both keys are present
  bool found_key1 = false, found_key2 = false;
  for (const auto& key : keys) {
    if (key.id == "key1") found_key1 = true;
    if (key.id == "key2") found_key2 = true;
  }
  EXPECT_TRUE(found_key1);
  EXPECT_TRUE(found_key2);
}

TEST_F(KeyStorageLinuxTest, DefaultKey) {
  CreateStorage();
  
  auto identifier = CreateTestKey();
  auto encrypted_key = CreateTestEncryptedKey();
  
  // Store the key
  EXPECT_TRUE(storage_->StoreKey(identifier, encrypted_key));
  
  // No default key initially
  auto default_key = storage_->GetDefaultKey();
  EXPECT_FALSE(default_key.has_value());
  
  // Set as default
  EXPECT_TRUE(storage_->SetDefaultKey(identifier.id));
  
  // Verify it's the default
  default_key = storage_->GetDefaultKey();
  ASSERT_TRUE(default_key.has_value());
  EXPECT_EQ(default_key->id, identifier.id);
  EXPECT_TRUE(default_key->is_default);
}

TEST_F(KeyStorageLinuxTest, UpdateKeyMetadata) {
  CreateStorage();
  
  auto identifier = CreateTestKey();
  auto encrypted_key = CreateTestEncryptedKey();
  
  // Store the key
  EXPECT_TRUE(storage_->StoreKey(identifier, encrypted_key));
  
  // Update metadata
  identifier.name = "Updated Test Key";
  identifier.last_used = base::Time::Now();
  EXPECT_TRUE(storage_->UpdateKeyMetadata(identifier));
  
  // Verify the update by listing keys
  auto keys = storage_->ListKeys();
  ASSERT_EQ(keys.size(), 1u);
  EXPECT_EQ(keys[0].name, "Updated Test Key");
  EXPECT_TRUE(keys[0].last_used.has_value());
}

TEST_F(KeyStorageLinuxTest, HasKey) {
  CreateStorage();
  
  auto identifier = CreateTestKey();
  auto encrypted_key = CreateTestEncryptedKey();
  
  // Key doesn't exist initially
  EXPECT_FALSE(storage_->HasKey(identifier.id));
  
  // Store the key
  EXPECT_TRUE(storage_->StoreKey(identifier, encrypted_key));
  
  // Now it exists
  EXPECT_TRUE(storage_->HasKey(identifier.id));
}

TEST_F(KeyStorageLinuxTest, NonExistentKey) {
  CreateStorage();
  
  KeyIdentifier identifier;
  identifier.id = "nonexistent";
  
  // Operations on non-existent key should fail gracefully
  EXPECT_FALSE(storage_->HasKey(identifier.id));
  
  auto retrieved = storage_->RetrieveKey(identifier);
  EXPECT_FALSE(retrieved.has_value());
  
  EXPECT_FALSE(storage_->DeleteKey(identifier));
  EXPECT_FALSE(storage_->UpdateKeyMetadata(identifier));
  EXPECT_FALSE(storage_->SetDefaultKey(identifier.id));
}

}  // namespace nostr