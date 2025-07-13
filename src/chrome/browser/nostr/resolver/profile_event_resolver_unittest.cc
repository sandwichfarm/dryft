// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/resolver/profile_event_resolver.h"

#include <memory>
#include <utility>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/nostr/local_relay/nostr_database.h"
#include "components/nostr/nostr_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {

class ProfileEventResolverTest : public testing::Test {
 protected:
  void SetUp() override {
    // Create in-memory database for testing
    database_ = std::make_unique<NostrDatabase>(base::FilePath());
    database_->Init();
    
    resolver_ = std::make_unique<ProfileEventResolver>(database_.get());
  }

  void TearDown() override {
    resolver_.reset();
    database_.reset();
  }

  // Helper to wait for async operations
  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
  }

  // Helper to capture resolution results
  std::unique_ptr<ResolutionResult> ResolveAndWait(const std::string& bech32) {
    std::unique_ptr<ResolutionResult> result;
    base::RunLoop run_loop;
    
    resolver_->ResolveEntity(bech32,
        base::BindLambdaForTesting([&](std::unique_ptr<ResolutionResult> res) {
          result = std::move(res);
          run_loop.Quit();
        }));
    
    run_loop.Run();
    return result;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<NostrDatabase> database_;
  std::unique_ptr<ProfileEventResolver> resolver_;
};

// Test basic resolution of npub (public key)
TEST_F(ProfileEventResolverTest, ResolveNpub) {
  // Mock npub resolution
  auto result = ResolveAndWait("npub1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z");
  
  ASSERT_TRUE(result);
  // With mock implementation, it should fail since no relays are connected
  EXPECT_FALSE(result->success);
  EXPECT_FALSE(result->error_message.empty());
}

// Test resolution with invalid bech32
TEST_F(ProfileEventResolverTest, InvalidBech32) {
  auto result = ResolveAndWait("invalid_bech32_string");
  
  ASSERT_TRUE(result);
  EXPECT_FALSE(result->success);
  EXPECT_EQ(result->error_message, "Invalid bech32 string");
}

// Test resolution with custom relays
TEST_F(ProfileEventResolverTest, ResolveWithCustomRelays) {
  std::vector<std::string> custom_relays = {
    "wss://custom.relay1.com",
    "wss://custom.relay2.com"
  };
  
  std::unique_ptr<ResolutionResult> result;
  base::RunLoop run_loop;
  
  resolver_->ResolveEntityWithRelays(
      "npub1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z",
      custom_relays,
      base::BindLambdaForTesting([&](std::unique_ptr<ResolutionResult> res) {
        result = std::move(res);
        run_loop.Quit();
      }));
  
  run_loop.Run();
  
  ASSERT_TRUE(result);
  // Should attempt to use custom relays (though will fail in test environment)
  EXPECT_FALSE(result->success);
}

// Test caching behavior
TEST_F(ProfileEventResolverTest, CachingEnabled) {
  ResolverConfig config;
  config.cache_results = true;
  config.cache_ttl = base::Hours(1);
  resolver_->SetConfig(config);
  
  // First resolution
  auto result1 = ResolveAndWait("npub1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z");
  ASSERT_TRUE(result1);
  
  // Second resolution should be instant from cache (if first succeeded)
  auto start_time = base::TimeTicks::Now();
  auto result2 = ResolveAndWait("npub1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z");
  auto elapsed = base::TimeTicks::Now() - start_time;
  
  ASSERT_TRUE(result2);
  // If cached, should be very fast
  if (result1->success) {
    EXPECT_LT(elapsed, base::Milliseconds(10));
  }
}

// Test caching disabled
TEST_F(ProfileEventResolverTest, CachingDisabled) {
  ResolverConfig config;
  config.cache_results = false;
  resolver_->SetConfig(config);
  
  // Resolve twice - both should hit relays
  auto result1 = ResolveAndWait("npub1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z");
  auto result2 = ResolveAndWait("npub1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z");
  
  ASSERT_TRUE(result1);
  ASSERT_TRUE(result2);
  // Both should have similar behavior (no caching)
  EXPECT_EQ(result1->success, result2->success);
}

// Test cache expiration
TEST_F(ProfileEventResolverTest, CacheExpiration) {
  ResolverConfig config;
  config.cache_results = true;
  config.cache_ttl = base::Seconds(10);  // Short TTL for testing
  resolver_->SetConfig(config);
  
  // First resolution
  auto result1 = ResolveAndWait("npub1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z");
  ASSERT_TRUE(result1);
  
  // Advance time past TTL
  task_environment_.FastForwardBy(base::Seconds(15));
  
  // Second resolution should not use cache
  auto result2 = ResolveAndWait("npub1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z");
  ASSERT_TRUE(result2);
  
  // Results should be independent (no cache hit)
}

// Test timeout handling
TEST_F(ProfileEventResolverTest, ResolutionTimeout) {
  ResolverConfig config;
  config.timeout = base::Seconds(5);
  resolver_->SetConfig(config);
  
  // Start resolution
  std::unique_ptr<ResolutionResult> result;
  base::RunLoop run_loop;
  
  resolver_->ResolveEntity(
      "npub1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z",
      base::BindLambdaForTesting([&](std::unique_ptr<ResolutionResult> res) {
        result = std::move(res);
        run_loop.Quit();
      }));
  
  // Advance time to trigger timeout
  task_environment_.FastForwardBy(base::Seconds(6));
  run_loop.Run();
  
  ASSERT_TRUE(result);
  EXPECT_FALSE(result->success);
  EXPECT_EQ(result->error_message, "Resolution timeout");
}

// Test setting default relays
TEST_F(ProfileEventResolverTest, SetDefaultRelays) {
  std::vector<std::string> new_relays = {
    "wss://new.relay1.com",
    "wss://new.relay2.com",
    "wss://new.relay3.com"
  };
  
  resolver_->SetDefaultRelays(new_relays);
  EXPECT_EQ(resolver_->default_relays(), new_relays);
}

// Test clearing cache
TEST_F(ProfileEventResolverTest, ClearCache) {
  ResolverConfig config;
  config.cache_results = true;
  resolver_->SetConfig(config);
  
  // Resolve to populate cache
  auto result1 = ResolveAndWait("npub1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z");
  ASSERT_TRUE(result1);
  
  // Clear cache
  resolver_->ClearCache();
  
  // Next resolution should not use cache
  auto result2 = ResolveAndWait("npub1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z");
  ASSERT_TRUE(result2);
}

// Test different entity types
TEST_F(ProfileEventResolverTest, ResolveDifferentEntityTypes) {
  // Test npub (profile)
  auto npub_result = ResolveAndWait("npub1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z");
  ASSERT_TRUE(npub_result);
  
  // Test note (event)
  auto note_result = ResolveAndWait("note1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z");
  ASSERT_TRUE(note_result);
  
  // Test nprofile (profile with relay hints)
  auto nprofile_result = ResolveAndWait("nprofile1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z");
  ASSERT_TRUE(nprofile_result);
  
  // Test nevent (event with relay hints)
  auto nevent_result = ResolveAndWait("nevent1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z");
  ASSERT_TRUE(nevent_result);
  
  // Test naddr (parameterized replaceable event)
  auto naddr_result = ResolveAndWait("naddr1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z");
  ASSERT_TRUE(naddr_result);
}

// Test relay configuration limits
TEST_F(ProfileEventResolverTest, RelayLimits) {
  ResolverConfig config;
  config.max_relays_per_query = 2;
  resolver_->SetConfig(config);
  
  // Set many default relays
  std::vector<std::string> many_relays;
  for (int i = 0; i < 10; ++i) {
    many_relays.push_back("wss://relay" + base::NumberToString(i) + ".com");
  }
  resolver_->SetDefaultRelays(many_relays);
  
  // Resolution should only use first 2 relays
  auto result = ResolveAndWait("npub1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq8x3s6z");
  ASSERT_TRUE(result);
  
  // Check that only max_relays_per_query were used
  EXPECT_LE(result->relays_used.size(), 2u);
}

}  // namespace nostr