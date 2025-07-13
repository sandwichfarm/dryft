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

  // Create request without X-Nsite-Pubkey header
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
  request.headers["X-Nsite-Pubkey"] = "npub1234567890abcdef";

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
  request.headers["x-nsite-pubkey"] = "npub1test";

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
  request.headers["X-Nsite-Pubkey"] = "invalid";

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

}  // namespace nostr