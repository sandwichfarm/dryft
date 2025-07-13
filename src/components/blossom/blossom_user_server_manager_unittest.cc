// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blossom/blossom_user_server_manager.h"

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/nostr/nostr_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blossom {

class BlossomUserServerManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    BlossomUserServerManager::Config config;
    config.server_list_ttl = base::Hours(1);
    config.max_servers_per_user = 10;
    config.default_servers = {
        GURL("https://default1.example.com"),
        GURL("https://default2.example.com")
    };
    
    manager_ = std::make_unique<BlossomUserServerManager>(config);
  }

  std::unique_ptr<nostr::NostrEvent> CreateServerListEvent(
      const std::vector<std::pair<std::string, std::string>>& servers) {
    auto event = std::make_unique<nostr::NostrEvent>();
    event->kind = 10063;
    event->pubkey = "test_pubkey";
    event->created_at = base::Time::Now().ToTimeT();
    event->content = "";
    
    for (const auto& [url, name] : servers) {
      base::Value::List tag;
      tag.Append("server");
      tag.Append(url);
      if (!name.empty()) {
        tag.Append(name);
      }
      event->tags.Append(std::move(tag));
    }
    
    return event;
  }

  void GetServers(const std::string& pubkey,
                 std::vector<std::unique_ptr<BlossomServer>>* result) {
    base::RunLoop run_loop;
    manager_->GetUserServers(
        pubkey,
        base::BindLambdaForTesting([&](const std::vector<std::unique_ptr<BlossomServer>>& servers) {
          result->clear();
          for (const auto& server : servers) {
            result->push_back(std::make_unique<BlossomServer>(*server));
          }
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<BlossomUserServerManager> manager_;
};

TEST_F(BlossomUserServerManagerTest, ParseValidServerListEvent) {
  auto event = CreateServerListEvent({
      {"https://server1.example.com", "Server 1"},
      {"https://server2.example.com", ""},
      {"https://server3.example.com", "Server 3"}
  });
  
  manager_->UpdateServerList("test_user", std::move(event));
  
  std::vector<std::unique_ptr<BlossomServer>> servers;
  GetServers("test_user", &servers);
  
  ASSERT_EQ(3u, servers.size());
  EXPECT_EQ("https://server1.example.com/", servers[0]->url.spec());
  EXPECT_EQ("Server 1", servers[0]->name);
  EXPECT_EQ("https://server2.example.com/", servers[1]->url.spec());
  EXPECT_TRUE(servers[1]->name.empty());
  EXPECT_EQ("https://server3.example.com/", servers[2]->url.spec());
  EXPECT_EQ("Server 3", servers[2]->name);
}

TEST_F(BlossomUserServerManagerTest, RejectInvalidUrls) {
  auto event = CreateServerListEvent({
      {"https://valid.example.com", "Valid"},
      {"invalid-url", "Invalid"},
      {"ftp://ftp.example.com", "FTP"},
      {"", "Empty"}
  });
  
  manager_->UpdateServerList("test_user", std::move(event));
  
  std::vector<std::unique_ptr<BlossomServer>> servers;
  GetServers("test_user", &servers);
  
  // Only the valid HTTPS URL should be accepted
  ASSERT_EQ(1u, servers.size());
  EXPECT_EQ("https://valid.example.com/", servers[0]->url.spec());
}

TEST_F(BlossomUserServerManagerTest, DefaultServersWhenNoUserServers) {
  std::vector<std::unique_ptr<BlossomServer>> servers;
  GetServers("unknown_user", &servers);
  
  // Should return default servers
  ASSERT_EQ(2u, servers.size());
  EXPECT_EQ("https://default1.example.com/", servers[0]->url.spec());
  EXPECT_EQ("https://default2.example.com/", servers[1]->url.spec());
}

TEST_F(BlossomUserServerManagerTest, CacheExpiration) {
  auto event = CreateServerListEvent({
      {"https://cached.example.com", "Cached"}
  });
  
  manager_->UpdateServerList("test_user", std::move(event));
  
  // First request should return cached servers
  std::vector<std::unique_ptr<BlossomServer>> servers;
  GetServers("test_user", &servers);
  ASSERT_EQ(1u, servers.size());
  
  // Advance time beyond cache TTL
  task_environment_.FastForwardBy(base::Hours(2));
  
  // Should now return default servers (cache expired)
  GetServers("test_user", &servers);
  ASSERT_EQ(2u, servers.size());  // Default servers
}

TEST_F(BlossomUserServerManagerTest, ServerHealthScoring) {
  BlossomServer server(GURL("https://test.example.com"), "Test");
  
  // Initial health should be perfect
  EXPECT_EQ(1.0, server.GetHealthScore());
  
  // Mark some failures
  server.MarkFailure();
  EXPECT_LT(server.GetHealthScore(), 1.0);
  
  server.MarkFailure();
  server.MarkFailure();
  EXPECT_LT(server.GetHealthScore(), 0.5);
  
  // Mark success should improve score
  server.MarkSuccess();
  EXPECT_GT(server.GetHealthScore(), 0.5);
  EXPECT_TRUE(server.is_available);
}

TEST_F(BlossomUserServerManagerTest, ServerUnavailableAfterManyFailures) {
  BlossomServer server(GURL("https://test.example.com"), "Test");
  
  // Mark many failures
  for (int i = 0; i < 10; i++) {
    server.MarkFailure();
  }
  
  EXPECT_FALSE(server.is_available);
  EXPECT_EQ(0.0, server.GetHealthScore());
}

TEST_F(BlossomUserServerManagerTest, GetBestServersOrdering) {
  auto event = CreateServerListEvent({
      {"https://server1.example.com", "Server 1"},
      {"https://server2.example.com", "Server 2"},
      {"https://server3.example.com", "Server 3"}
  });
  
  manager_->UpdateServerList("test_user", std::move(event));
  
  // Get server pointers to manipulate health
  auto best_servers = manager_->GetBestServers("test_user", 5);
  ASSERT_EQ(3u, best_servers.size());
  
  // Make server 2 worse by marking failures
  best_servers[1]->MarkFailure();
  best_servers[1]->MarkFailure();
  
  // Make server 3 best by marking success
  best_servers[2]->MarkSuccess();
  
  // Get best servers again - should be reordered
  auto reordered = manager_->GetBestServers("test_user", 5);
  ASSERT_EQ(3u, reordered.size());
  
  // Server 3 should now be first (highest health score)
  EXPECT_EQ("https://server3.example.com/", reordered[0]->url.spec());
}

TEST_F(BlossomUserServerManagerTest, LimitMaxServersPerUser) {
  // Create event with many servers
  std::vector<std::pair<std::string, std::string>> many_servers;
  for (int i = 1; i <= 20; i++) {
    many_servers.emplace_back(
        "https://server" + base::NumberToString(i) + ".example.com",
        "Server " + base::NumberToString(i));
  }
  
  auto event = CreateServerListEvent(many_servers);
  manager_->UpdateServerList("test_user", std::move(event));
  
  std::vector<std::unique_ptr<BlossomServer>> servers;
  GetServers("test_user", &servers);
  
  // Should be limited to max_servers_per_user (10)
  EXPECT_EQ(10u, servers.size());
}

TEST_F(BlossomUserServerManagerTest, InvalidEventKind) {
  auto event = std::make_unique<nostr::NostrEvent>();
  event->kind = 1;  // Wrong kind
  event->pubkey = "test_pubkey";
  
  // Should not crash or update cache
  manager_->UpdateServerList("test_user", std::move(event));
  
  std::vector<std::unique_ptr<BlossomServer>> servers;
  GetServers("test_user", &servers);
  
  // Should return default servers
  EXPECT_EQ(2u, servers.size());
}

TEST_F(BlossomUserServerManagerTest, ClearCache) {
  auto event = CreateServerListEvent({
      {"https://cached.example.com", "Cached"}
  });
  
  manager_->UpdateServerList("test_user", std::move(event));
  
  std::vector<std::unique_ptr<BlossomServer>> servers;
  GetServers("test_user", &servers);
  ASSERT_EQ(1u, servers.size());
  
  // Clear cache
  manager_->ClearCache();
  
  // Should now return default servers
  GetServers("test_user", &servers);
  EXPECT_EQ(2u, servers.size());
}

}  // namespace blossom