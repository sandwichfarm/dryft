// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blossom/blossom_content_resolver.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/blossom/blossom_user_server_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blossom {

class MockBlossomUserServerManager : public BlossomUserServerManager {
 public:
  MockBlossomUserServerManager() : BlossomUserServerManager(BlossomUserServerManager::Config()) {}
  
  std::vector<BlossomServer*> GetBestServers(const std::string& pubkey,
                                            size_t max_count = 5) override {
    std::vector<BlossomServer*> result;
    
    // Return empty if servers_empty_ is set
    if (servers_empty_) {
      return result;
    }
    
    // Return test servers
    static std::vector<std::unique_ptr<BlossomServer>> test_servers;
    if (test_servers.empty()) {
      test_servers.push_back(std::make_unique<BlossomServer>(
          GURL("https://server1.example.com"), "Server 1"));
      test_servers.push_back(std::make_unique<BlossomServer>(
          GURL("https://server2.example.com"), "Server 2"));
      test_servers.push_back(std::make_unique<BlossomServer>(
          GURL("https://server3.example.com"), "Server 3"));
    }
    
    size_t count = std::min(max_count, test_servers.size());
    for (size_t i = 0; i < count; i++) {
      result.push_back(test_servers[i].get());
    }
    
    return result;
  }
  
  void SetServersEmpty(bool empty) {
    servers_empty_ = empty;
  }
  
 private:
  bool servers_empty_ = false;
};

class BlossomContentResolverTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_server_manager_ = std::make_unique<MockBlossomUserServerManager>();
    
    BlossomContentResolver::Config config;
    config.server_timeout = base::Seconds(1);
    config.total_timeout = base::Seconds(5);
    config.max_concurrent_requests = 2;
    config.max_servers_to_try = 3;
    
    resolver_ = std::make_unique<BlossomContentResolver>(
        config, mock_server_manager_.get());
  }

  void ResolveContent(const std::string& pubkey,
                     const std::string& hash,
                     ContentResolutionResult* result) {
    base::RunLoop run_loop;
    resolver_->ResolveContent(
        pubkey, hash,
        base::BindLambdaForTesting([&](ContentResolutionResult res) {
          *result = std::move(res);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  void UploadContent(const std::string& pubkey,
                    const std::string& hash,
                    const std::string& content,
                    std::vector<GURL>* success_urls,
                    std::vector<std::pair<GURL, std::string>>* failures) {
    base::RunLoop run_loop;
    resolver_->UploadContent(
        pubkey, hash, content, "text/plain",
        base::BindLambdaForTesting([&](std::vector<GURL> successes,
                                     std::vector<std::pair<GURL, std::string>> fails) {
          *success_urls = std::move(successes);
          *failures = std::move(fails);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<MockBlossomUserServerManager> mock_server_manager_;
  std::unique_ptr<BlossomContentResolver> resolver_;
};

TEST_F(BlossomContentResolverTest, ResolveContentNotFound) {
  ContentResolutionResult result;
  ResolveContent("test_user", "test_hash", &result);
  
  // Should fail since we're simulating network errors
  EXPECT_EQ(ContentResolutionResult::NOT_FOUND, result.status);
  EXPECT_FALSE(result.error_message.empty());
  EXPECT_GT(result.duration.InMilliseconds(), 0);
}

TEST_F(BlossomContentResolverTest, ResolveContentNoServers) {
  mock_server_manager_->SetServersEmpty(true);
  
  ContentResolutionResult result;
  ResolveContent("test_user", "test_hash", &result);
  
  EXPECT_EQ(ContentResolutionResult::NOT_FOUND, result.status);
  EXPECT_EQ("No servers configured", result.error_message);
}

TEST_F(BlossomContentResolverTest, UploadToMultipleServers) {
  std::vector<GURL> success_urls;
  std::vector<std::pair<GURL, std::string>> failures;
  
  UploadContent("test_user", "test_hash", "test content", 
               &success_urls, &failures);
  
  // Should attempt upload to available servers
  // Since we're simulating success, we should get successful uploads
  EXPECT_GE(success_urls.size(), 1u);
}

TEST_F(BlossomContentResolverTest, ContentExists) {
  bool exists = false;
  GURL found_url;
  
  base::RunLoop run_loop;
  resolver_->CheckContentExists(
      "test_user", "test_hash",
      base::BindLambdaForTesting([&](bool result, GURL url) {
        exists = result;
        found_url = url;
        run_loop.Quit();
      }));
  run_loop.Run();
  
  // Should return true with a URL (mocked)
  EXPECT_TRUE(exists);
  EXPECT_TRUE(found_url.is_valid());
}

TEST_F(BlossomContentResolverTest, ResolveTimeout) {
  // Set very short timeout
  BlossomContentResolver::Config config;
  config.total_timeout = base::Milliseconds(1);
  config.server_timeout = base::Milliseconds(1);
  
  auto short_resolver = std::make_unique<BlossomContentResolver>(
      config, mock_server_manager_.get());
  
  base::RunLoop run_loop;
  ContentResolutionResult result;
  
  short_resolver->ResolveContent(
      "test_user", "test_hash",
      base::BindLambdaForTesting([&](ContentResolutionResult res) {
        result = std::move(res);
        run_loop.Quit();
      }));
  
  // Fast forward past timeout
  task_environment_.FastForwardBy(base::Milliseconds(10));
  
  run_loop.Run();
  
  EXPECT_EQ(ContentResolutionResult::TIMEOUT, result.status);
}

// TODO: Add more tests for:
// - Successful content resolution (would require mocking HTTP requests)
// - Server failover scenarios
// - Concurrent upload limits
// - Network error handling
// - Content verification

}  // namespace blossom