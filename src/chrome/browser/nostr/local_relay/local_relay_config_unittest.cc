// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/local_relay/local_relay_config.h"

#include "base/test/task_environment.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {
namespace local_relay {

class LocalRelayConfigTest : public testing::Test {
 protected:
  void SetUp() override {
    LocalRelayConfigManager::RegisterProfilePrefs(pref_service_.registry());
    config_manager_ = std::make_unique<LocalRelayConfigManager>(&pref_service_);
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<LocalRelayConfigManager> config_manager_;
};

TEST_F(LocalRelayConfigTest, DefaultValues) {
  // Test default configuration values
  EXPECT_FALSE(config_manager_->IsEnabled());
  EXPECT_EQ(8081, config_manager_->GetPort());
  EXPECT_EQ("127.0.0.1", config_manager_->GetInterface());
  EXPECT_FALSE(config_manager_->AllowsExternalAccess());
  
  // Storage defaults
  EXPECT_EQ(1, config_manager_->GetMaxStorageGB());
  EXPECT_EQ(100000, config_manager_->GetMaxEvents());
  EXPECT_EQ(30, config_manager_->GetRetentionDays());
  
  // Performance defaults
  EXPECT_EQ(100, config_manager_->GetMaxConnections());
  EXPECT_EQ(20, config_manager_->GetMaxSubscriptionsPerConnection());
  EXPECT_EQ(512 * 1024, config_manager_->GetMaxMessageSize());
  EXPECT_EQ(256 * 1024, config_manager_->GetMaxEventSize());
  
  // Rate limiting defaults
  EXPECT_EQ(100, config_manager_->GetMaxEventsPerMinute());
  EXPECT_EQ(60, config_manager_->GetMaxReqPerMinute());
}

TEST_F(LocalRelayConfigTest, SetAndGetEnabled) {
  EXPECT_FALSE(config_manager_->IsEnabled());
  
  config_manager_->SetEnabled(true);
  EXPECT_TRUE(config_manager_->IsEnabled());
  
  config_manager_->SetEnabled(false);
  EXPECT_FALSE(config_manager_->IsEnabled());
}

TEST_F(LocalRelayConfigTest, SetAndGetPort) {
  // Valid port
  config_manager_->SetPort(8080);
  EXPECT_EQ(8080, config_manager_->GetPort());
  
  // Invalid port (too low) - should not change
  config_manager_->SetPort(80);
  EXPECT_EQ(8080, config_manager_->GetPort());
  
  // Invalid port (too high) - should not change
  config_manager_->SetPort(70000);
  EXPECT_EQ(8080, config_manager_->GetPort());
}

TEST_F(LocalRelayConfigTest, SetAndGetInterface) {
  // Valid interfaces
  config_manager_->SetInterface("0.0.0.0");
  EXPECT_EQ("0.0.0.0", config_manager_->GetInterface());
  
  config_manager_->SetInterface("localhost");
  EXPECT_EQ("localhost", config_manager_->GetInterface());
  
  config_manager_->SetInterface("192.168.1.100");
  EXPECT_EQ("192.168.1.100", config_manager_->GetInterface());
  
  // Invalid interface - should not change
  std::string previous = config_manager_->GetInterface();
  config_manager_->SetInterface("invalid.interface");
  EXPECT_EQ(previous, config_manager_->GetInterface());
}

TEST_F(LocalRelayConfigTest, ValidatePort) {
  EXPECT_TRUE(LocalRelayConfigManager::IsValidPort(1024));
  EXPECT_TRUE(LocalRelayConfigManager::IsValidPort(8080));
  EXPECT_TRUE(LocalRelayConfigManager::IsValidPort(65535));
  
  EXPECT_FALSE(LocalRelayConfigManager::IsValidPort(0));
  EXPECT_FALSE(LocalRelayConfigManager::IsValidPort(80));
  EXPECT_FALSE(LocalRelayConfigManager::IsValidPort(1023));
  EXPECT_FALSE(LocalRelayConfigManager::IsValidPort(65536));
  EXPECT_FALSE(LocalRelayConfigManager::IsValidPort(-1));
}

TEST_F(LocalRelayConfigTest, ValidateInterface) {
  EXPECT_TRUE(LocalRelayConfigManager::IsValidInterface("127.0.0.1"));
  EXPECT_TRUE(LocalRelayConfigManager::IsValidInterface("0.0.0.0"));
  EXPECT_TRUE(LocalRelayConfigManager::IsValidInterface("localhost"));
  EXPECT_TRUE(LocalRelayConfigManager::IsValidInterface("192.168.1.1"));
  EXPECT_TRUE(LocalRelayConfigManager::IsValidInterface("::1"));
  
  EXPECT_FALSE(LocalRelayConfigManager::IsValidInterface(""));
  EXPECT_FALSE(LocalRelayConfigManager::IsValidInterface("invalid"));
  EXPECT_FALSE(LocalRelayConfigManager::IsValidInterface("256.0.0.0"));
}

TEST_F(LocalRelayConfigTest, ValidateStorageLimit) {
  EXPECT_TRUE(LocalRelayConfigManager::IsValidStorageLimit(0));  // Unlimited
  EXPECT_TRUE(LocalRelayConfigManager::IsValidStorageLimit(1));
  EXPECT_TRUE(LocalRelayConfigManager::IsValidStorageLimit(50));
  EXPECT_TRUE(LocalRelayConfigManager::IsValidStorageLimit(100));
  
  EXPECT_FALSE(LocalRelayConfigManager::IsValidStorageLimit(-1));
  EXPECT_FALSE(LocalRelayConfigManager::IsValidStorageLimit(101));
}

TEST_F(LocalRelayConfigTest, GetConfig) {
  // Set some custom values
  config_manager_->SetPort(9000);
  config_manager_->SetInterface("0.0.0.0");
  pref_service_.SetInteger(LocalRelayConfigManager::kMaxConnectionsPref, 50);
  pref_service_.SetInteger(LocalRelayConfigManager::kMaxStorageGBPref, 5);
  
  LocalRelayConfig config = config_manager_->GetConfig();
  
  EXPECT_EQ("0.0.0.0", config.bind_address);
  EXPECT_EQ(9000, config.port);
  EXPECT_EQ(50, config.max_connections);
  EXPECT_EQ(5, config.database_config.max_size_gb);
}

TEST_F(LocalRelayConfigTest, GetStatistics) {
  config_manager_->SetEnabled(true);
  config_manager_->SetPort(8888);
  
  base::Value::Dict stats = config_manager_->GetStatistics();
  
  EXPECT_TRUE(stats.FindBool("enabled").value_or(false));
  EXPECT_EQ(8888, stats.FindInt("port").value_or(0));
  
  // Check nested dictionaries
  const base::Value::Dict* storage = stats.FindDict("storage");
  ASSERT_TRUE(storage);
  EXPECT_EQ(1, storage->FindInt("max_gb").value_or(0));
  
  const base::Value::Dict* performance = stats.FindDict("performance");
  ASSERT_TRUE(performance);
  EXPECT_EQ(100, performance->FindInt("max_connections").value_or(0));
}

TEST_F(LocalRelayConfigTest, AllowedOrigins) {
  // Default should return wildcard
  std::vector<std::string> origins = config_manager_->GetAllowedOrigins();
  ASSERT_EQ(1u, origins.size());
  EXPECT_EQ("*", origins[0]);
  
  // Set custom origins
  base::Value::List origin_list;
  origin_list.Append("https://example.com");
  origin_list.Append("https://localhost:3000");
  pref_service_.SetList(LocalRelayConfigManager::kAllowedOriginsPref, 
                        std::move(origin_list));
  
  origins = config_manager_->GetAllowedOrigins();
  ASSERT_EQ(2u, origins.size());
  EXPECT_EQ("https://example.com", origins[0]);
  EXPECT_EQ("https://localhost:3000", origins[1]);
}

TEST_F(LocalRelayConfigTest, BlockedPubkeys) {
  // Default should be empty
  std::vector<std::string> blocked = config_manager_->GetBlockedPubkeys();
  EXPECT_TRUE(blocked.empty());
  
  // Set blocked pubkeys
  base::Value::List blocked_list;
  blocked_list.Append("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
  blocked_list.Append("invalid");  // Should be filtered out
  blocked_list.Append("fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210");
  pref_service_.SetList(LocalRelayConfigManager::kBlockedPubkeysPref,
                        std::move(blocked_list));
  
  blocked = config_manager_->GetBlockedPubkeys();
  ASSERT_EQ(2u, blocked.size());
  EXPECT_EQ(64u, blocked[0].length());
  EXPECT_EQ(64u, blocked[1].length());
}

TEST_F(LocalRelayConfigTest, RequiresAuth) {
  // Default should be false
  EXPECT_FALSE(config_manager_->RequiresAuth());
  
  // Set to true
  pref_service_.SetBoolean(LocalRelayConfigManager::kRequireAuthPref, true);
  EXPECT_TRUE(config_manager_->RequiresAuth());
  
  // Set to false
  pref_service_.SetBoolean(LocalRelayConfigManager::kRequireAuthPref, false);
  EXPECT_FALSE(config_manager_->RequiresAuth());
}

TEST_F(LocalRelayConfigTest, GetAllowedKinds) {
  // Default should be empty (allow all)
  std::vector<int> kinds = config_manager_->GetAllowedKinds();
  EXPECT_TRUE(kinds.empty());
  
  // Set specific allowed kinds
  base::Value::List kinds_list;
  kinds_list.Append(1);  // Text note
  kinds_list.Append(3);  // Contact list
  kinds_list.Append(7);  // Reaction
  pref_service_.SetList(LocalRelayConfigManager::kAllowedKindsPref,
                        std::move(kinds_list));
  
  kinds = config_manager_->GetAllowedKinds();
  ASSERT_EQ(3u, kinds.size());
  EXPECT_EQ(1, kinds[0]);
  EXPECT_EQ(3, kinds[1]);
  EXPECT_EQ(7, kinds[2]);
  
  // Test with invalid values (should be filtered out)
  base::Value::List mixed_list;
  mixed_list.Append(1);
  mixed_list.Append("invalid");  // String, should be ignored
  mixed_list.Append(30023);  // Valid kind
  pref_service_.SetList(LocalRelayConfigManager::kAllowedKindsPref,
                        std::move(mixed_list));
  
  kinds = config_manager_->GetAllowedKinds();
  ASSERT_EQ(2u, kinds.size());
  EXPECT_EQ(1, kinds[0]);
  EXPECT_EQ(30023, kinds[1]);
}

}  // namespace local_relay
}  // namespace nostr