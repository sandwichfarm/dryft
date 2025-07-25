// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nostr_service.h"

#include "base/test/task_environment.h"
#include "chrome/browser/nostr/key_storage_in_memory.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {

class MultiAccountServiceTest : public testing::Test {
 public:
  MultiAccountServiceTest() = default;
  ~MultiAccountServiceTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    service_ = std::make_unique<NostrService>(profile_.get());
  }

  void TearDown() override {
    service_.reset();
    profile_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<NostrService> service_;
};

// Test account creation and listing
TEST_F(MultiAccountServiceTest, CreateAndListAccounts) {
  // Initially no accounts should exist
  auto accounts = service_->ListAccounts();
  EXPECT_TRUE(accounts.empty());
  
  // Generate first account
  std::string pubkey1 = service_->GenerateNewKey("Alice");
  EXPECT_FALSE(pubkey1.empty());
  EXPECT_EQ(64U, pubkey1.length());  // 32 bytes hex-encoded
  
  // Should now have one account
  accounts = service_->ListAccounts();
  EXPECT_EQ(1U, accounts.size());
  
  // Verify account details
  const auto& account1 = accounts[0].GetDict();
  EXPECT_EQ(pubkey1, *account1.FindString("pubkey"));
  EXPECT_EQ("Alice", *account1.FindString("name"));
  EXPECT_TRUE(account1.FindBool("is_default").value_or(false));
  
  // Generate second account
  std::string pubkey2 = service_->GenerateNewKey("Bob");
  EXPECT_FALSE(pubkey2.empty());
  EXPECT_NE(pubkey1, pubkey2);  // Should be different keys
  
  // Should now have two accounts
  accounts = service_->ListAccounts();
  EXPECT_EQ(2U, accounts.size());
  
  // First account should still be default
  bool found_default = false;
  for (const auto& account : accounts) {
    const auto& account_dict = account.GetDict();
    if (account_dict.FindBool("is_default").value_or(false)) {
      EXPECT_EQ(pubkey1, *account_dict.FindString("pubkey"));
      found_default = true;
    }
  }
  EXPECT_TRUE(found_default);
}

// Test current account retrieval
TEST_F(MultiAccountServiceTest, GetCurrentAccount) {
  // No current account initially
  auto current = service_->GetCurrentAccount();
  EXPECT_TRUE(current.empty());
  
  // Create an account
  std::string pubkey = service_->GenerateNewKey("Test Account");
  
  // Should now have a current account
  current = service_->GetCurrentAccount();
  EXPECT_FALSE(current.empty());
  EXPECT_EQ(pubkey, *current.FindString("pubkey"));
  EXPECT_EQ("Test Account", *current.FindString("name"));
}

// Test account switching
TEST_F(MultiAccountServiceTest, SwitchAccounts) {
  // Create two accounts
  std::string pubkey1 = service_->GenerateNewKey("Account 1");
  std::string pubkey2 = service_->GenerateNewKey("Account 2");
  
  // First account should be default
  auto current = service_->GetCurrentAccount();
  EXPECT_EQ(pubkey1, *current.FindString("pubkey"));
  
  // Switch to second account
  bool success = service_->SwitchAccount(pubkey2);
  EXPECT_TRUE(success);
  
  // Current account should now be the second one
  current = service_->GetCurrentAccount();
  EXPECT_EQ(pubkey2, *current.FindString("pubkey"));
  
  // Verify default key changed
  EXPECT_EQ(pubkey2, service_->GetPublicKey());
  
  // Switch back to first account
  success = service_->SwitchAccount(pubkey1);
  EXPECT_TRUE(success);
  
  current = service_->GetCurrentAccount();
  EXPECT_EQ(pubkey1, *current.FindString("pubkey"));
}

// Test switching to non-existent account
TEST_F(MultiAccountServiceTest, SwitchToNonExistentAccount) {
  // Create one account
  std::string pubkey1 = service_->GenerateNewKey("Real Account");
  
  // Try to switch to non-existent account
  std::string fake_pubkey = "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
  bool success = service_->SwitchAccount(fake_pubkey);
  EXPECT_FALSE(success);
  
  // Current account should remain unchanged
  auto current = service_->GetCurrentAccount();
  EXPECT_EQ(pubkey1, *current.FindString("pubkey"));
}

// Test account deletion
TEST_F(MultiAccountServiceTest, DeleteAccount) {
  // Create two accounts
  std::string pubkey1 = service_->GenerateNewKey("Account 1");
  std::string pubkey2 = service_->GenerateNewKey("Account 2");
  
  // Should have two accounts
  auto accounts = service_->ListAccounts();
  EXPECT_EQ(2U, accounts.size());
  
  // Delete the second account
  bool success = service_->DeleteAccount(pubkey2);
  EXPECT_TRUE(success);
  
  // Should now have one account
  accounts = service_->ListAccounts();
  EXPECT_EQ(1U, accounts.size());
  EXPECT_EQ(pubkey1, *accounts[0].GetDict().FindString("pubkey"));
}

// Test cannot delete last account
TEST_F(MultiAccountServiceTest, CannotDeleteLastAccount) {
  // Create one account
  std::string pubkey = service_->GenerateNewKey("Only Account");
  
  // Try to delete it
  bool success = service_->DeleteAccount(pubkey);
  EXPECT_FALSE(success);
  
  // Account should still exist
  auto accounts = service_->ListAccounts();
  EXPECT_EQ(1U, accounts.size());
}

// Test deleting default account sets new default
TEST_F(MultiAccountServiceTest, DeleteDefaultAccountSetsNewDefault) {
  // Create two accounts
  std::string pubkey1 = service_->GenerateNewKey("Account 1");  // Default
  std::string pubkey2 = service_->GenerateNewKey("Account 2");
  
  // Verify first is default
  auto current = service_->GetCurrentAccount();
  EXPECT_EQ(pubkey1, *current.FindString("pubkey"));
  
  // Delete default account
  bool success = service_->DeleteAccount(pubkey1);
  EXPECT_TRUE(success);
  
  // Second account should now be default
  current = service_->GetCurrentAccount();
  EXPECT_EQ(pubkey2, *current.FindString("pubkey"));
  
  // Service should use the new default key
  EXPECT_EQ(pubkey2, service_->GetPublicKey());
}

// Test account metadata updates
TEST_F(MultiAccountServiceTest, UpdateAccountMetadata) {
  // Create an account
  std::string pubkey = service_->GenerateNewKey("Original Name");
  
  // Update metadata
  base::Value::Dict metadata;
  metadata.Set("name", "Updated Name");
  
  base::Value::List relays;
  relays.Append("wss://relay1.example.com");
  relays.Append("wss://relay2.example.com");
  metadata.Set("relays", std::move(relays));
  
  bool success = service_->UpdateAccountMetadata(pubkey, metadata);
  EXPECT_TRUE(success);
  
  // Verify updates
  auto current = service_->GetCurrentAccount();
  EXPECT_EQ("Updated Name", *current.FindString("name"));
  
  auto* relay_list = current.FindList("relays");
  ASSERT_TRUE(relay_list);
  EXPECT_EQ(2U, relay_list->size());
  EXPECT_EQ("wss://relay1.example.com", (*relay_list)[0].GetString());
  EXPECT_EQ("wss://relay2.example.com", (*relay_list)[1].GetString());
}

// Test key import with custom name
TEST_F(MultiAccountServiceTest, ImportKeyWithName) {
  // Import a key with custom name
  std::string private_key = "a1b2c3d4e5f6789012345678901234567890123456789012345678901234567890";
  std::string pubkey = service_->ImportKey(private_key, "Imported Account");
  
  EXPECT_FALSE(pubkey.empty());
  
  // Verify account was created with correct name
  auto accounts = service_->ListAccounts();
  EXPECT_EQ(1U, accounts.size());
  
  const auto& account = accounts[0].GetDict();
  EXPECT_EQ(pubkey, *account.FindString("pubkey"));
  EXPECT_EQ("Imported Account", *account.FindString("name"));
}

}  // namespace nostr