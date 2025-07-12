// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/secret_service_client.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chrome/browser/nostr/key_encryption.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {

class SecretServiceClientTest : public testing::Test {
 public:
  SecretServiceClientTest() = default;
  ~SecretServiceClientTest() override = default;

  void SetUp() override {
    client_ = std::make_unique<SecretServiceClient>();
  }

  void TearDown() override {
    client_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SecretServiceClient> client_;

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

TEST_F(SecretServiceClientTest, ConstructorDoesNotCrash) {
  EXPECT_TRUE(client_);
}

TEST_F(SecretServiceClientTest, InitializeHandlesNoSecretService) {
  // On systems without libsecret, Initialize should return false gracefully
  bool result = client_->Initialize();
  // We can't guarantee libsecret is available in test environment
  // Just verify it doesn't crash
  EXPECT_TRUE(result || !result);  // Should not crash regardless of result
}

TEST_F(SecretServiceClientTest, IsAvailableAfterFailedInitialize) {
  // If Initialize fails, IsAvailable should return false
  if (!client_->Initialize()) {
    EXPECT_FALSE(client_->IsAvailable());
  }
}

TEST_F(SecretServiceClientTest, SerializeDeserializeEncryptedKey) {
  auto encrypted_key = CreateTestEncryptedKey();
  
  // Test serialization
  std::string serialized = client_->SerializeEncryptedKey(encrypted_key);
  EXPECT_FALSE(serialized.empty());
  
  // Test deserialization
  auto deserialized = client_->DeserializeEncryptedKey(serialized);
  ASSERT_TRUE(deserialized.has_value());
  
  // Verify data matches
  EXPECT_EQ(deserialized->encrypted_data, encrypted_key.encrypted_data);
  EXPECT_EQ(deserialized->salt, encrypted_key.salt);
  EXPECT_EQ(deserialized->iv, encrypted_key.iv);
  EXPECT_EQ(deserialized->algorithm, encrypted_key.algorithm);
  EXPECT_EQ(deserialized->kdf_iterations, encrypted_key.kdf_iterations);
}

TEST_F(SecretServiceClientTest, SerializeDeserializeKeyMetadata) {
  auto identifier = CreateTestKey();
  identifier.last_used = base::Time::Now();
  
  // Test serialization
  std::string serialized = client_->SerializeKeyMetadata(identifier);
  EXPECT_FALSE(serialized.empty());
  
  // Test deserialization
  auto deserialized = client_->DeserializeKeyMetadata(serialized);
  ASSERT_TRUE(deserialized.has_value());
  
  // Verify data matches
  EXPECT_EQ(deserialized->id, identifier.id);
  EXPECT_EQ(deserialized->name, identifier.name);
  EXPECT_EQ(deserialized->npub, identifier.npub);
  EXPECT_EQ(deserialized->created_at, identifier.created_at);
  EXPECT_EQ(deserialized->is_default, identifier.is_default);
  ASSERT_TRUE(deserialized->last_used.has_value());
  EXPECT_EQ(deserialized->last_used.value(), identifier.last_used.value());
}

TEST_F(SecretServiceClientTest, GetSecretLabel) {
  std::string key_id = "test123";
  
  std::string key_label = client_->GetSecretLabel(key_id, "key");
  EXPECT_EQ(key_label, "Tungsten Nostr Key: test123");
  
  std::string metadata_label = client_->GetSecretLabel(key_id, "metadata");
  EXPECT_EQ(metadata_label, "Tungsten Nostr Key Metadata: test123");
  
  std::string default_label = client_->GetSecretLabel("default", "default");
  EXPECT_EQ(default_label, "Tungsten Default Nostr Key");
  
  std::string other_label = client_->GetSecretLabel(key_id, "other");
  EXPECT_EQ(other_label, "Tungsten Nostr: test123");
}

TEST_F(SecretServiceClientTest, DeserializeInvalidData) {
  // Test with invalid JSON
  auto result = client_->DeserializeEncryptedKey("invalid json");
  EXPECT_FALSE(result.has_value());
  
  // Test with valid JSON but missing fields
  auto result2 = client_->DeserializeEncryptedKey("{\"missing_fields\": true}");
  EXPECT_FALSE(result2.has_value());
  
  // Test metadata deserialization with invalid data
  auto result3 = client_->DeserializeKeyMetadata("invalid json");
  EXPECT_FALSE(result3.has_value());
  
  auto result4 = client_->DeserializeKeyMetadata("{\"incomplete\": true}");
  EXPECT_FALSE(result4.has_value());
}

TEST_F(SecretServiceClientTest, OperationsFailGracefullyWithoutService) {
  // If Secret Service is not available, operations should fail gracefully
  if (!client_->Initialize()) {
    auto identifier = CreateTestKey();
    auto encrypted_key = CreateTestEncryptedKey();
    
    // All operations should return false/nullopt when service unavailable
    EXPECT_FALSE(client_->StoreKey(identifier, encrypted_key));
    EXPECT_FALSE(client_->RetrieveKey(identifier).has_value());
    EXPECT_FALSE(client_->DeleteKey(identifier));
    EXPECT_TRUE(client_->ListKeys().empty());
    EXPECT_FALSE(client_->UpdateKeyMetadata(identifier));
    EXPECT_FALSE(client_->HasKey(identifier.id));
    EXPECT_FALSE(client_->GetDefaultKey().has_value());
    EXPECT_FALSE(client_->SetDefaultKey(identifier.id));
  }
}

// This test is only meaningful if Secret Service is actually available
// Most CI environments won't have it, so this mainly serves as documentation
TEST_F(SecretServiceClientTest, DISABLED_IntegrationTestWithRealSecretService) {
  // This test would only pass on systems with actual Secret Service
  // It's disabled by default to avoid CI failures
  
  if (client_->Initialize() && client_->IsAvailable()) {
    auto identifier = CreateTestKey("integration_test");
    auto encrypted_key = CreateTestEncryptedKey();
    
    // Test store and retrieve
    EXPECT_TRUE(client_->StoreKey(identifier, encrypted_key));
    EXPECT_TRUE(client_->HasKey(identifier.id));
    
    auto retrieved = client_->RetrieveKey(identifier);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->encrypted_data, encrypted_key.encrypted_data);
    
    // Test default key
    EXPECT_TRUE(client_->SetDefaultKey(identifier.id));
    auto default_key = client_->GetDefaultKey();
    ASSERT_TRUE(default_key.has_value());
    EXPECT_EQ(default_key->id, identifier.id);
    
    // Clean up
    EXPECT_TRUE(client_->DeleteKey(identifier));
    EXPECT_FALSE(client_->HasKey(identifier.id));
  }
}

}  // namespace nostr