// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nostr_permission_manager.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/nostr/nostr_permission_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace nostr {

class NostrPermissionManagerTest : public testing::Test {
 public:
  NostrPermissionManagerTest() = default;
  ~NostrPermissionManagerTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    manager_ = std::make_unique<NostrPermissionManager>(profile_.get());
  }

  void TearDown() override {
    manager_.reset();
    profile_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<NostrPermissionManager> manager_;

  url::Origin CreateTestOrigin(const std::string& host = "example.com") {
    return url::Origin::Create(GURL("https://" + host));
  }

  NIP07Permission CreateTestPermission(const url::Origin& origin) {
    NIP07Permission permission;
    permission.origin = origin;
    permission.default_policy = NIP07Permission::Policy::ASK;
    permission.granted_until = base::Time::Now() + base::Days(30);
    permission.last_used = base::Time::Now();
    
    // Set default rate limits
    permission.rate_limits.requests_per_minute = 60;
    permission.rate_limits.signs_per_hour = 20;
    permission.rate_limits.request_window_start = base::Time::Now();
    permission.rate_limits.sign_window_start = base::Time::Now();
    
    return permission;
  }
};

TEST_F(NostrPermissionManagerTest, CheckPermissionWithoutGrant) {
  auto origin = CreateTestOrigin();
  
  // Without any granted permission, should ask user
  auto result = manager_->CheckPermission(
      origin, NIP07Permission::Method::GET_PUBLIC_KEY);
  EXPECT_EQ(result, NostrPermissionManager::PermissionResult::ASK_USER);
}

TEST_F(NostrPermissionManagerTest, GrantAndCheckPermission) {
  auto origin = CreateTestOrigin();
  auto permission = CreateTestPermission(origin);
  permission.method_policies[NIP07Permission::Method::GET_PUBLIC_KEY] = 
      NIP07Permission::Policy::ALLOW;
  
  // Grant permission
  auto grant_result = manager_->GrantPermission(origin, permission);
  EXPECT_EQ(grant_result, NostrPermissionManager::GrantResult::SUCCESS);
  
  // Check permission
  auto check_result = manager_->CheckPermission(
      origin, NIP07Permission::Method::GET_PUBLIC_KEY);
  EXPECT_EQ(check_result, NostrPermissionManager::PermissionResult::GRANTED);
}

TEST_F(NostrPermissionManagerTest, DenyPermission) {
  auto origin = CreateTestOrigin();
  auto permission = CreateTestPermission(origin);
  permission.method_policies[NIP07Permission::Method::SIGN_EVENT] = 
      NIP07Permission::Policy::DENY;
  
  manager_->GrantPermission(origin, permission);
  
  auto result = manager_->CheckPermission(
      origin, NIP07Permission::Method::SIGN_EVENT);
  EXPECT_EQ(result, NostrPermissionManager::PermissionResult::DENIED);
}

TEST_F(NostrPermissionManagerTest, KindSpecificPermissions) {
  auto origin = CreateTestOrigin();
  auto permission = CreateTestPermission(origin);
  
  // Allow kind 1 (text note), deny kind 0 (metadata)
  permission.kind_policies[1] = NIP07Permission::Policy::ALLOW;
  permission.kind_policies[0] = NIP07Permission::Policy::DENY;
  
  manager_->GrantPermission(origin, permission);
  
  // Check kind 1 - should be allowed
  auto result1 = manager_->CheckPermission(
      origin, NIP07Permission::Method::SIGN_EVENT, 1);
  EXPECT_EQ(result1, NostrPermissionManager::PermissionResult::GRANTED);
  
  // Check kind 0 - should be denied
  auto result0 = manager_->CheckPermission(
      origin, NIP07Permission::Method::SIGN_EVENT, 0);
  EXPECT_EQ(result0, NostrPermissionManager::PermissionResult::DENIED);
  
  // Check kind 2 (no specific policy) - should fall back to default (ASK)
  auto result2 = manager_->CheckPermission(
      origin, NIP07Permission::Method::SIGN_EVENT, 2);
  EXPECT_EQ(result2, NostrPermissionManager::PermissionResult::ASK_USER);
}

TEST_F(NostrPermissionManagerTest, ExpiredPermission) {
  auto origin = CreateTestOrigin();
  auto permission = CreateTestPermission(origin);
  permission.granted_until = base::Time::Now() - base::Days(1);  // Expired
  permission.method_policies[NIP07Permission::Method::GET_PUBLIC_KEY] = 
      NIP07Permission::Policy::ALLOW;
  
  manager_->GrantPermission(origin, permission);
  
  auto result = manager_->CheckPermission(
      origin, NIP07Permission::Method::GET_PUBLIC_KEY);
  EXPECT_EQ(result, NostrPermissionManager::PermissionResult::EXPIRED);
  
  // Permission should be removed from cache
  auto stored_permission = manager_->GetPermission(origin);
  EXPECT_FALSE(stored_permission.has_value());
}

TEST_F(NostrPermissionManagerTest, RateLimiting) {
  auto origin = CreateTestOrigin();
  auto permission = CreateTestPermission(origin);
  permission.rate_limits.requests_per_minute = 2;  // Very low limit for testing
  permission.method_policies[NIP07Permission::Method::GET_PUBLIC_KEY] = 
      NIP07Permission::Policy::ALLOW;
  
  manager_->GrantPermission(origin, permission);
  
  // First request should succeed
  auto result1 = manager_->CheckPermission(
      origin, NIP07Permission::Method::GET_PUBLIC_KEY);
  EXPECT_EQ(result1, NostrPermissionManager::PermissionResult::GRANTED);
  manager_->UpdateRateLimit(origin, NIP07Permission::Method::GET_PUBLIC_KEY);
  
  // Second request should succeed
  auto result2 = manager_->CheckPermission(
      origin, NIP07Permission::Method::GET_PUBLIC_KEY);
  EXPECT_EQ(result2, NostrPermissionManager::PermissionResult::GRANTED);
  manager_->UpdateRateLimit(origin, NIP07Permission::Method::GET_PUBLIC_KEY);
  
  // Third request should be rate limited
  auto result3 = manager_->CheckPermission(
      origin, NIP07Permission::Method::GET_PUBLIC_KEY);
  EXPECT_EQ(result3, NostrPermissionManager::PermissionResult::RATE_LIMITED);
}

TEST_F(NostrPermissionManagerTest, SigningRateLimit) {
  auto origin = CreateTestOrigin();
  auto permission = CreateTestPermission(origin);
  permission.rate_limits.signs_per_hour = 1;  // Very low limit for testing
  permission.method_policies[NIP07Permission::Method::SIGN_EVENT] = 
      NIP07Permission::Policy::ALLOW;
  
  manager_->GrantPermission(origin, permission);
  
  // First sign should succeed
  auto result1 = manager_->CheckPermission(
      origin, NIP07Permission::Method::SIGN_EVENT);
  EXPECT_EQ(result1, NostrPermissionManager::PermissionResult::GRANTED);
  manager_->UpdateRateLimit(origin, NIP07Permission::Method::SIGN_EVENT);
  
  // Second sign should be rate limited
  auto result2 = manager_->CheckPermission(
      origin, NIP07Permission::Method::SIGN_EVENT);
  EXPECT_EQ(result2, NostrPermissionManager::PermissionResult::RATE_LIMITED);
}

TEST_F(NostrPermissionManagerTest, RateLimitReset) {
  auto origin = CreateTestOrigin();
  auto permission = CreateTestPermission(origin);
  permission.rate_limits.requests_per_minute = 1;
  permission.method_policies[NIP07Permission::Method::GET_PUBLIC_KEY] = 
      NIP07Permission::Policy::ALLOW;
  
  manager_->GrantPermission(origin, permission);
  
  // Use up the rate limit
  manager_->CheckPermission(origin, NIP07Permission::Method::GET_PUBLIC_KEY);
  manager_->UpdateRateLimit(origin, NIP07Permission::Method::GET_PUBLIC_KEY);
  
  auto result1 = manager_->CheckPermission(
      origin, NIP07Permission::Method::GET_PUBLIC_KEY);
  EXPECT_EQ(result1, NostrPermissionManager::PermissionResult::RATE_LIMITED);
  
  // Advance time by 1 minute
  task_environment_.FastForwardBy(base::Minutes(1));
  
  // Should be allowed again
  auto result2 = manager_->CheckPermission(
      origin, NIP07Permission::Method::GET_PUBLIC_KEY);
  EXPECT_EQ(result2, NostrPermissionManager::PermissionResult::GRANTED);
}

TEST_F(NostrPermissionManagerTest, RevokePermission) {
  auto origin = CreateTestOrigin();
  auto permission = CreateTestPermission(origin);
  permission.method_policies[NIP07Permission::Method::GET_PUBLIC_KEY] = 
      NIP07Permission::Policy::ALLOW;
  
  manager_->GrantPermission(origin, permission);
  
  // Verify permission exists
  auto stored = manager_->GetPermission(origin);
  EXPECT_TRUE(stored.has_value());
  
  // Revoke permission
  EXPECT_TRUE(manager_->RevokePermission(origin));
  
  // Verify permission is gone
  auto stored_after = manager_->GetPermission(origin);
  EXPECT_FALSE(stored_after.has_value());
}

TEST_F(NostrPermissionManagerTest, RevokeMethodPermission) {
  auto origin = CreateTestOrigin();
  auto permission = CreateTestPermission(origin);
  permission.method_policies[NIP07Permission::Method::GET_PUBLIC_KEY] = 
      NIP07Permission::Policy::ALLOW;
  permission.method_policies[NIP07Permission::Method::SIGN_EVENT] = 
      NIP07Permission::Policy::ALLOW;
  
  manager_->GrantPermission(origin, permission);
  
  // Revoke just the sign event permission
  EXPECT_TRUE(manager_->RevokeMethodPermission(
      origin, NIP07Permission::Method::SIGN_EVENT));
  
  // Verify sign event permission is gone
  auto result1 = manager_->CheckPermission(
      origin, NIP07Permission::Method::SIGN_EVENT);
  EXPECT_EQ(result1, NostrPermissionManager::PermissionResult::ASK_USER);
  
  // Verify get public key permission still exists
  auto result2 = manager_->CheckPermission(
      origin, NIP07Permission::Method::GET_PUBLIC_KEY);
  EXPECT_EQ(result2, NostrPermissionManager::PermissionResult::GRANTED);
}

TEST_F(NostrPermissionManagerTest, GetAllPermissions) {
  auto origin1 = CreateTestOrigin("site1.com");
  auto origin2 = CreateTestOrigin("site2.com");
  
  auto permission1 = CreateTestPermission(origin1);
  auto permission2 = CreateTestPermission(origin2);
  
  manager_->GrantPermission(origin1, permission1);
  manager_->GrantPermission(origin2, permission2);
  
  auto all_permissions = manager_->GetAllPermissions();
  EXPECT_EQ(all_permissions.size(), 2u);
  
  // Check that both origins are present
  bool found_origin1 = false, found_origin2 = false;
  for (const auto& permission : all_permissions) {
    if (permission.origin == origin1) found_origin1 = true;
    if (permission.origin == origin2) found_origin2 = true;
  }
  EXPECT_TRUE(found_origin1);
  EXPECT_TRUE(found_origin2);
}

TEST_F(NostrPermissionManagerTest, CleanupExpiredPermissions) {
  auto origin1 = CreateTestOrigin("site1.com");
  auto origin2 = CreateTestOrigin("site2.com");
  
  // One expired, one valid
  auto permission1 = CreateTestPermission(origin1);
  permission1.granted_until = base::Time::Now() - base::Days(1);  // Expired
  
  auto permission2 = CreateTestPermission(origin2);
  permission2.granted_until = base::Time::Now() + base::Days(30);  // Valid
  
  manager_->GrantPermission(origin1, permission1);
  manager_->GrantPermission(origin2, permission2);
  
  // Verify both are stored
  EXPECT_EQ(manager_->GetAllPermissions().size(), 2u);
  
  // Cleanup expired permissions
  manager_->CleanupExpiredPermissions();
  
  // Only the valid one should remain
  auto remaining = manager_->GetAllPermissions();
  EXPECT_EQ(remaining.size(), 1u);
  EXPECT_EQ(remaining[0].origin, origin2);
}

TEST_F(NostrPermissionManagerTest, InvalidOrigin) {
  url::Origin invalid_origin;  // Opaque origin
  auto permission = CreateTestPermission(invalid_origin);
  
  auto result = manager_->GrantPermission(invalid_origin, permission);
  EXPECT_EQ(result, NostrPermissionManager::GrantResult::INVALID_ORIGIN);
}

TEST_F(NostrPermissionManagerTest, SerializationRoundTrip) {
  auto origin = CreateTestOrigin();
  auto permission = CreateTestPermission(origin);
  permission.method_policies[NIP07Permission::Method::GET_PUBLIC_KEY] = 
      NIP07Permission::Policy::ALLOW;
  permission.kind_policies[1] = NIP07Permission::Policy::DENY;
  
  // Convert to value and back
  auto value = permission.ToValue();
  auto deserialized = NIP07Permission::FromValue(value);
  
  ASSERT_TRUE(deserialized.has_value());
  
  // Verify all fields match
  EXPECT_EQ(deserialized->origin, permission.origin);
  EXPECT_EQ(deserialized->default_policy, permission.default_policy);
  EXPECT_EQ(deserialized->granted_until, permission.granted_until);
  EXPECT_EQ(deserialized->method_policies, permission.method_policies);
  EXPECT_EQ(deserialized->kind_policies, permission.kind_policies);
  EXPECT_EQ(deserialized->rate_limits.requests_per_minute, 
            permission.rate_limits.requests_per_minute);
  EXPECT_EQ(deserialized->rate_limits.signs_per_hour, 
            permission.rate_limits.signs_per_hour);
}

}  // namespace nostr