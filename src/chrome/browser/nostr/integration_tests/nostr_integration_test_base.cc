// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/integration_tests/nostr_integration_test_base.h"

#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "chrome/browser/nostr/nostr_service_factory.h"
#include "chrome/browser/nostr/local_relay/local_relay_service_factory.h"
#include "chrome/browser/nostr/nostr_permission_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/nostr/nostr_features.h"
#include "content/public/test/test_utils.h"

namespace nostr {

NostrIntegrationTestBase::NostrIntegrationTestBase()
    : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

NostrIntegrationTestBase::~NostrIntegrationTestBase() = default;

void NostrIntegrationTestBase::SetUp() {
  testing::Test::SetUp();
  
  // Enable Nostr features by default
  EnableNostrFeatures();
  
  // Create test profile
  TestingProfile::Builder profile_builder;
  profile_ = profile_builder.Build();
  
  // Get service instances
  nostr_service_ = NostrServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(nostr_service_);
  
  local_relay_service_ = LocalRelayServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(local_relay_service_);
  
  permission_manager_ = nostr_service_->GetPermissionManager();
  ASSERT_TRUE(permission_manager_);
}

void NostrIntegrationTestBase::TearDown() {
  // Clean up services
  if (local_relay_service_) {
    local_relay_service_->Shutdown();
  }
  
  // Reset profile
  profile_.reset();
  
  testing::Test::TearDown();
}

void NostrIntegrationTestBase::GrantNIP07Permission(const GURL& url) {
  permission_manager_->SetPermissionForTesting(
      url, NostrPermissionManager::PermissionType::kNIP07, true);
}

void NostrIntegrationTestBase::DenyNIP07Permission(const GURL& url) {
  permission_manager_->SetPermissionForTesting(
      url, NostrPermissionManager::PermissionType::kNIP07, false);
}

std::string NostrIntegrationTestBase::CreateAndStoreTestKey(
    const std::string& name,
    const std::string& passphrase) {
  std::string public_key;
  base::RunLoop run_loop;
  
  nostr_service_->CreateKey(
      name, passphrase,
      base::BindLambdaForTesting([&](bool success, const std::string& pubkey) {
        EXPECT_TRUE(success);
        public_key = pubkey;
        run_loop.Quit();
      }));
  
  run_loop.Run();
  return public_key;
}

void NostrIntegrationTestBase::WaitForLocalRelayReady() {
  base::RunLoop run_loop;
  
  auto check_ready = [&]() {
    if (local_relay_service_->IsReady()) {
      run_loop.Quit();
    } else {
      // Check again after a delay
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindLambdaForTesting([&]() { check_ready(); }),
          base::Milliseconds(100));
    }
  };
  
  check_ready();
  run_loop.Run();
}

void NostrIntegrationTestBase::SendTestEventToLocalRelay(
    const std::string& event_json) {
  ASSERT_TRUE(local_relay_service_->IsReady());
  
  base::RunLoop run_loop;
  local_relay_service_->PublishEvent(
      event_json,
      base::BindLambdaForTesting([&](bool success, const std::string& error) {
        EXPECT_TRUE(success) << "Failed to publish event: " << error;
        run_loop.Quit();
      }));
  
  run_loop.Run();
}

std::vector<std::string> NostrIntegrationTestBase::QueryEventsFromLocalRelay(
    const std::string& filter_json) {
  ASSERT_TRUE(local_relay_service_->IsReady());
  
  std::vector<std::string> events;
  base::RunLoop run_loop;
  
  local_relay_service_->QueryEvents(
      filter_json,
      base::BindLambdaForTesting(
          [&](bool success, const std::vector<std::string>& result) {
            EXPECT_TRUE(success);
            events = result;
            run_loop.Quit();
          }));
  
  run_loop.Run();
  return events;
}

void NostrIntegrationTestBase::EnableNostrFeatures() {
  std::vector<base::test::FeatureRef> enabled_features = {
      features::kNostrSupport,
      features::kNostrLocalRelay,
      features::kNostrProtocolHandler,
      features::kNostrNsiteSupport
  };
  
  scoped_feature_list_.InitWithFeatures(enabled_features, {});
}

void NostrIntegrationTestBase::DisableNostrFeatures() {
  std::vector<base::test::FeatureRef> disabled_features = {
      features::kNostrSupport,
      features::kNostrLocalRelay,
      features::kNostrProtocolHandler,
      features::kNostrNsiteSupport
  };
  
  scoped_feature_list_.InitWithFeatures({}, disabled_features);
}

}  // namespace nostr