// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nostr_passphrase_manager.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {

class NostrPassphraseManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    manager_ = std::make_unique<NostrPassphraseManager>(profile_.get());
  }

  void TearDown() override {
    manager_.reset();
    profile_.reset();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<NostrPassphraseManager> manager_;
};

TEST_F(NostrPassphraseManagerTest, InitiallyNoCachedPassphrase) {
  EXPECT_FALSE(manager_->HasCachedPassphrase());
}

TEST_F(NostrPassphraseManagerTest, ClearPassphrase) {
  // Initially no cached passphrase
  EXPECT_FALSE(manager_->HasCachedPassphrase());
  
  // Clear should not crash even with no cached passphrase
  manager_->ClearCachedPassphrase();
  EXPECT_FALSE(manager_->HasCachedPassphrase());
}

TEST_F(NostrPassphraseManagerTest, SyncRequestReturnsEmptyWithoutUI) {
  // Without UI implementation, sync request should return empty
  std::string result = manager_->RequestPassphraseSync("Test prompt");
  EXPECT_TRUE(result.empty());
}

TEST_F(NostrPassphraseManagerTest, CacheTimeoutSetting) {
  // Default timeout should be 5 minutes
  EXPECT_EQ(manager_->GetCacheTimeout(), base::Minutes(5));
  
  // Should be able to set custom timeout
  manager_->SetCacheTimeout(base::Minutes(10));
  EXPECT_EQ(manager_->GetCacheTimeout(), base::Minutes(10));
}

TEST_F(NostrPassphraseManagerTest, CacheExpirationAfterTimeout) {
  // This test requires implementation of the UI dialog or a test mock
  // For now, we can only test that the cache timeout mechanism exists
  
  // Set a short timeout for testing
  manager_->SetCacheTimeout(base::Seconds(1));
  
  // Without a working dialog, we can't actually cache a passphrase yet
  // This test will be completed when the UI dialog is implemented
  EXPECT_FALSE(manager_->HasCachedPassphrase());
  
  // TODO: Once dialog is implemented, test should:
  // 1. Cache a passphrase successfully
  // 2. Verify HasCachedPassphrase() returns true
  // 3. Advance clock past timeout using task_environment_.FastForwardBy()
  // 4. Verify HasCachedPassphrase() returns false
}

}  // namespace nostr