// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_INTEGRATION_TESTS_NOSTR_INTEGRATION_TEST_BASE_H_
#define CHROME_BROWSER_NOSTR_INTEGRATION_TESTS_NOSTR_INTEGRATION_TEST_BASE_H_

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class BrowserContext;
}

namespace nostr {

class NostrService;
class LocalRelayService;
class NostrPermissionManager;

// Base class for Nostr integration tests that provides common setup
// and utilities for testing cross-component interactions.
class NostrIntegrationTestBase : public testing::Test {
 public:
  NostrIntegrationTestBase();
  ~NostrIntegrationTestBase() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 protected:
  // Returns the test profile
  TestingProfile* profile() { return profile_.get(); }
  
  // Returns the Nostr service instance
  NostrService* nostr_service() { return nostr_service_; }
  
  // Returns the local relay service instance
  LocalRelayService* local_relay_service() { return local_relay_service_; }
  
  // Returns the permission manager instance
  NostrPermissionManager* permission_manager() { return permission_manager_; }

  // Helper methods for common test operations
  
  // Simulates granting NIP-07 permission for a URL
  void GrantNIP07Permission(const GURL& url);
  
  // Simulates denying NIP-07 permission for a URL
  void DenyNIP07Permission(const GURL& url);
  
  // Creates a test keypair and stores it
  std::string CreateAndStoreTestKey(const std::string& name,
                                    const std::string& passphrase);
  
  // Waits for the local relay to be ready
  void WaitForLocalRelayReady();
  
  // Sends a test event to the local relay
  void SendTestEventToLocalRelay(const std::string& event_json);
  
  // Queries events from the local relay
  std::vector<std::string> QueryEventsFromLocalRelay(
      const std::string& filter_json);

  // Enables/disables Nostr features for testing
  void EnableNostrFeatures();
  void DisableNostrFeatures();

  // Task environment for async operations
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  // Test environment setup
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  
  // Test profile and services
  std::unique_ptr<TestingProfile> profile_;
  NostrService* nostr_service_ = nullptr;
  LocalRelayService* local_relay_service_ = nullptr;
  NostrPermissionManager* permission_manager_ = nullptr;
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_INTEGRATION_TESTS_NOSTR_INTEGRATION_TEST_BASE_H_