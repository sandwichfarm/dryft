// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_streaming_server.h"

#include <memory>
#include <set>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {

class NsiteStreamingServerTest : public testing::Test {
 protected:
  NsiteStreamingServerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    server_ = std::make_unique<NsiteStreamingServer>(temp_dir_.GetPath());
  }

  void TearDown() override {
    server_.reset();
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<NsiteStreamingServer> server_;
};

TEST_F(NsiteStreamingServerTest, StartStop) {
  // Test starting the server
  uint16_t port = server_->Start();
  EXPECT_GT(port, 0);
  EXPECT_GE(port, 49152);
  EXPECT_LE(port, 65535);
  EXPECT_TRUE(server_->IsRunning());
  EXPECT_EQ(port, server_->GetPort());

  // Test stopping the server
  server_->Stop();
  EXPECT_FALSE(server_->IsRunning());
  EXPECT_EQ(0, server_->GetPort());
}

TEST_F(NsiteStreamingServerTest, DoubleStart) {
  // Start server
  uint16_t port1 = server_->Start();
  EXPECT_GT(port1, 0);

  // Try to start again - should return same port
  uint16_t port2 = server_->Start();
  EXPECT_EQ(port1, port2);
  EXPECT_TRUE(server_->IsRunning());
}

TEST_F(NsiteStreamingServerTest, PortInEphemeralRange) {
  uint16_t port = server_->Start();
  EXPECT_GT(port, 0);
  
  // Check port is in ephemeral range
  EXPECT_GE(port, 49152);
  EXPECT_LE(port, 65535);
}

TEST_F(NsiteStreamingServerTest, AvoidBlacklistedPorts) {
  // Start multiple servers to increase chance of hitting different ports
  std::set<uint16_t> allocated_ports;
  const std::set<uint16_t> blacklisted = {
      3000, 3001, 3333, 4200, 5000, 5173, 5432, 6379,
      7000, 8000, 8080, 8081, 8082, 8083, 8888, 9000,
      9200, 9229, 27017
  };

  for (int i = 0; i < 10; ++i) {
    auto test_server = std::make_unique<NsiteStreamingServer>(temp_dir_.GetPath());
    uint16_t port = test_server->Start();
    
    if (port > 0) {
      // Ensure not in blacklist
      EXPECT_EQ(blacklisted.find(port), blacklisted.end());
      allocated_ports.insert(port);
      test_server->Stop();
    }
  }

  // Should have allocated at least one port
  EXPECT_GT(allocated_ports.size(), 0u);
}

TEST_F(NsiteStreamingServerTest, ParseRequestMissingHeader) {
  // Start server
  uint16_t port = server_->Start();
  ASSERT_GT(port, 0);

  // Create request without X-Npub header
  net::HttpServerRequestInfo request;
  request.method = "GET";
  request.path = "/index.html";

  // Parse should fail
  auto context = server_->ParseNsiteRequest(request);
  EXPECT_FALSE(context.valid);
}

TEST_F(NsiteStreamingServerTest, ParseRequestValidHeader) {
  // Start server
  uint16_t port = server_->Start();
  ASSERT_GT(port, 0);

  // Create request with valid header
  net::HttpServerRequestInfo request;
  request.method = "GET";
  request.path = "/path/to/file.html";
  request.headers["X-Npub"] = "npub1234567890abcdef";

  // Parse should succeed
  auto context = server_->ParseNsiteRequest(request);
  EXPECT_TRUE(context.valid);
  EXPECT_EQ(context.npub, "npub1234567890abcdef");
  EXPECT_EQ(context.path, "path/to/file.html");
}

TEST_F(NsiteStreamingServerTest, ParseRequestCaseInsensitiveHeader) {
  // Start server
  uint16_t port = server_->Start();
  ASSERT_GT(port, 0);

  // Create request with different case header
  net::HttpServerRequestInfo request;
  request.method = "GET";
  request.path = "/test.js";
  request.headers["x-npub"] = "npub1test";

  // Parse should succeed (case-insensitive)
  auto context = server_->ParseNsiteRequest(request);
  EXPECT_TRUE(context.valid);
  EXPECT_EQ(context.npub, "npub1test");
}

TEST_F(NsiteStreamingServerTest, ParseRequestInvalidNpub) {
  // Start server
  uint16_t port = server_->Start();
  ASSERT_GT(port, 0);

  // Create request with invalid npub (doesn't start with npub1)
  net::HttpServerRequestInfo request;
  request.method = "GET";
  request.path = "/index.html";
  request.headers["X-Npub"] = "invalid";

  // Parse should fail
  auto context = server_->ParseNsiteRequest(request);
  EXPECT_FALSE(context.valid);
}

TEST_F(NsiteStreamingServerTest, MultipleServerInstances) {
  // Create multiple servers with different profiles
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());
  
  auto server2 = std::make_unique<NsiteStreamingServer>(temp_dir2.GetPath());

  // Both should start on different ports
  uint16_t port1 = server_->Start();
  uint16_t port2 = server2->Start();

  EXPECT_GT(port1, 0);
  EXPECT_GT(port2, 0);
  EXPECT_NE(port1, port2);

  EXPECT_TRUE(server_->IsRunning());
  EXPECT_TRUE(server2->IsRunning());

  // Clean up
  server2->Stop();
}

TEST_F(NsiteStreamingServerTest, ServerRestartNewPort) {
  // Start server
  uint16_t port1 = server_->Start();
  EXPECT_GT(port1, 0);

  // Stop and restart
  server_->Stop();
  uint16_t port2 = server_->Start();
  EXPECT_GT(port2, 0);

  // Port might be the same or different, both are valid
  // Just verify it's in the valid range
  EXPECT_GE(port2, 49152);
  EXPECT_LE(port2, 65535);
}

TEST_F(NsiteStreamingServerTest, ValidateNpubLength) {
  // Start server
  uint16_t port = server_->Start();
  ASSERT_GT(port, 0);

  // Test various npub lengths
  struct TestCase {
    std::string npub;
    bool expected_valid;
  };
  
  TestCase cases[] = {
    // Valid npub (63 chars)
    {"npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r", true},
    // Too short
    {"npub1short", false},
    // Too long
    {"npub1verylongnpubthatexceedstheexpectedlengthof63characterswhichisinvalid", false},
    // Wrong prefix
    {"xpub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r", false},
    // Empty
    {"", false},
  };

  for (const auto& test : cases) {
    net::HttpServerRequestInfo request;
    request.method = "GET";
    request.path = "/test.html";
    if (!test.npub.empty()) {
      request.headers["X-Npub"] = test.npub;
    }

    auto context = server_->ParseNsiteRequest(request);
    EXPECT_EQ(context.valid, test.expected_valid) 
        << "Failed for npub: " << test.npub;
  }
}

TEST_F(NsiteStreamingServerTest, SessionFallback) {
  // Start server
  uint16_t port = server_->Start();
  ASSERT_GT(port, 0);

  // First request with header - should create session
  net::HttpServerRequestInfo request1;
  request1.method = "GET";
  request1.path = "/index.html";
  request1.headers["X-Npub"] = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";

  auto context1 = server_->ParseNsiteRequest(request1);
  EXPECT_TRUE(context1.valid);
  EXPECT_FALSE(context1.from_session);
  EXPECT_FALSE(context1.session_id.empty());

  // Second request without header but with session cookie
  net::HttpServerRequestInfo request2;
  request2.method = "GET";
  request2.path = "/page2.html";
  request2.headers["cookie"] = "nsite_session=" + context1.session_id;

  auto context2 = server_->ParseNsiteRequest(request2);
  EXPECT_TRUE(context2.valid);
  EXPECT_TRUE(context2.from_session);
  EXPECT_EQ(context1.npub, context2.npub);
  EXPECT_EQ(context1.session_id, context2.session_id);
}

TEST_F(NsiteStreamingServerTest, SessionWithDifferentNpub) {
  // Start server
  uint16_t port = server_->Start();
  ASSERT_GT(port, 0);

  std::string session_id;
  
  // First request establishes session with npub1
  {
    net::HttpServerRequestInfo request;
    request.method = "GET";
    request.path = "/site1/index.html";
    request.headers["X-Npub"] = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";

    auto context = server_->ParseNsiteRequest(request);
    EXPECT_TRUE(context.valid);
    session_id = context.session_id;
  }

  // Second request with same session but different npub - should update session
  {
    net::HttpServerRequestInfo request;
    request.method = "GET";
    request.path = "/site2/index.html";
    request.headers["X-Npub"] = "npub14nr0ux0cn38r5rvf3wen3p9sgfxv2ydqchtqt5gu8r8rpa0x97q330wjj";
    request.headers["cookie"] = "nsite_session=" + session_id;

    auto context = server_->ParseNsiteRequest(request);
    EXPECT_TRUE(context.valid);
    EXPECT_EQ(context.npub, "npub14nr0ux0cn38r5rvf3wen3p9sgfxv2ydqchtqt5gu8r8rpa0x97q330wjj");
  }

  // Third request with session only - should use updated npub
  {
    net::HttpServerRequestInfo request;
    request.method = "GET";
    request.path = "/site2/page.html";
    request.headers["cookie"] = "nsite_session=" + session_id;

    auto context = server_->ParseNsiteRequest(request);
    EXPECT_TRUE(context.valid);
    EXPECT_TRUE(context.from_session);
    EXPECT_EQ(context.npub, "npub14nr0ux0cn38r5rvf3wen3p9sgfxv2ydqchtqt5gu8r8rpa0x97q330wjj");
  }
}

TEST_F(NsiteStreamingServerTest, CacheIntegration) {
  // Start server
  uint16_t port = server_->Start();
  ASSERT_GT(port, 0);

  const std::string npub = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  const std::string path = "index.html";
  const std::string content = "<html><body>Hello Nsite!</body></html>";
  
  // Cache a file
  server_->CacheFile(npub, path, content, "text/html");
  
  // Verify it was cached
  auto cache_manager = server_->GetCacheManager();
  ASSERT_TRUE(cache_manager);
  
  auto cached_file = cache_manager->GetFile(npub, path);
  ASSERT_TRUE(cached_file);
  EXPECT_EQ(cached_file->content, content);
  EXPECT_EQ(cached_file->content_type, "text/html");
}

TEST_F(NsiteStreamingServerTest, CacheStats) {
  // Start server
  uint16_t port = server_->Start();
  ASSERT_GT(port, 0);

  auto cache_manager = server_->GetCacheManager();
  ASSERT_TRUE(cache_manager);
  
  // Initial stats
  auto stats = cache_manager->GetStats();
  EXPECT_EQ(stats.file_count, 0u);
  EXPECT_EQ(stats.total_size, 0u);
  
  // Cache some files
  server_->CacheFile("npub1test1", "file1.html", "content1", "text/html");
  server_->CacheFile("npub1test2", "file2.js", "content2", "application/javascript");
  
  // Check updated stats
  stats = cache_manager->GetStats();
  EXPECT_EQ(stats.file_count, 2u);
  EXPECT_EQ(stats.total_size, 16u);  // "content1" + "content2"
}

TEST_F(NsiteStreamingServerTest, CachePersistence) {
  // Start server
  uint16_t port = server_->Start();
  ASSERT_GT(port, 0);

  const std::string npub = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  const std::string path = "persistent.html";
  const std::string content = "persistent content";
  
  // Cache file and save metadata
  server_->CacheFile(npub, path, content, "text/html");
  server_->GetCacheManager()->SaveMetadata();
  
  // Stop and restart server (simulating restart)
  server_->Stop();
  server_.reset();
  server_ = std::make_unique<NsiteStreamingServer>(temp_dir_.GetPath());
  port = server_->Start();
  ASSERT_GT(port, 0);
  
  // File should still be accessible
  auto cached_file = server_->GetCacheManager()->GetFile(npub, path);
  ASSERT_TRUE(cached_file);
  EXPECT_EQ(cached_file->content, content);
}

TEST_F(NsiteStreamingServerTest, CacheClearNsite) {
  // Start server
  uint16_t port = server_->Start();
  ASSERT_GT(port, 0);

  const std::string npub1 = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  const std::string npub2 = "npub14nr0ux0cn38r5rvf3wen3p9sgfxv2ydqchtqt5gu8r8rpa0x97q330wjj";
  
  // Cache files for both nsites
  server_->CacheFile(npub1, "file1.html", "content1", "text/html");
  server_->CacheFile(npub1, "file2.css", "content2", "text/css");
  server_->CacheFile(npub2, "file3.js", "content3", "application/javascript");
  
  auto cache_manager = server_->GetCacheManager();
  
  // Clear npub1
  cache_manager->ClearNsite(npub1);
  
  // Verify npub1 files are gone
  EXPECT_FALSE(cache_manager->GetFile(npub1, "file1.html"));
  EXPECT_FALSE(cache_manager->GetFile(npub1, "file2.css"));
  
  // Verify npub2 file still exists
  EXPECT_TRUE(cache_manager->GetFile(npub2, "file3.js"));
}

}  // namespace nostr