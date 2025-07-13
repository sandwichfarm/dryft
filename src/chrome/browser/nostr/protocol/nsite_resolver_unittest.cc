// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/protocol/nsite_resolver.h"

#include <memory>
#include <string>

#include "base/json/json_writer.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace nostr {

class NsiteResolverTest : public testing::Test {
 protected:
  NsiteResolverTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void SetUp() override {
    resolver_ = std::make_unique<NsiteResolver>(shared_url_loader_factory_);
  }

  // Helper to create NIP-05 response JSON
  std::string CreateNip05Response(const std::string& username,
                                 const std::string& pubkey) {
    base::Value::Dict names;
    names.Set(username, pubkey);
    
    base::Value::Dict response;
    response.Set("names", std::move(names));
    
    std::string json;
    base::JSONWriter::Write(response, &json);
    return json;
  }

  // Helper to simulate NIP-05 response
  void SimulateNip05Response(const std::string& domain,
                            const std::string& response_body,
                            net::HttpStatusCode status = net::HTTP_OK) {
    GURL nip05_url(std::string("https://") + domain + "/.well-known/nostr.json");
    test_url_loader_factory_.AddResponse(
        nip05_url.spec(), response_body, status);
  }

  // Helper to wait for async operations
  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<NsiteResolver> resolver_;
};

TEST_F(NsiteResolverTest, ParseValidNsiteUrl) {
  GURL url("nostr://nsite/example.com/path/to/content");
  
  bool callback_called = false;
  resolver_->Resolve(url, base::BindOnce(
      [](bool* called, std::optional<NsiteResolver::ResolveResult> result) {
        *called = true;
        // Will fail since we didn't set up NIP-05 response
        EXPECT_FALSE(result.has_value());
      },
      &callback_called));
  
  RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(NsiteResolverTest, ParseInvalidNsiteUrl) {
  GURL url("https://example.com");
  
  bool callback_called = false;
  resolver_->Resolve(url, base::BindOnce(
      [](bool* called, std::optional<NsiteResolver::ResolveResult> result) {
        *called = true;
        EXPECT_FALSE(result.has_value());
      },
      &callback_called));
  
  RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

// Npub subdomain test removed - local servers cannot have subdomains

TEST_F(NsiteResolverTest, ResolveNip05WithUsername) {
  const std::string test_pubkey = 
      "3bf0c63fcb93463407af97a5e5ee64fa883d107ef9e558472c4eb9aaaefa459d";
  
  SimulateNip05Response("example.com", 
                       CreateNip05Response("alice", test_pubkey));
  
  GURL url("nostr://nsite/alice@example.com/posts/123");
  
  bool callback_called = false;
  resolver_->Resolve(url, base::BindOnce(
      [](bool* called, const std::string& expected_pubkey, 
         std::optional<NsiteResolver::ResolveResult> result) {
        *called = true;
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->pubkey, expected_pubkey);
        EXPECT_EQ(result->domain, "example.com");
        EXPECT_EQ(result->path, "posts/123");
      },
      &callback_called, test_pubkey));
  
  RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(NsiteResolverTest, ResolveNip05DefaultUsername) {
  const std::string test_pubkey = 
      "3bf0c63fcb93463407af97a5e5ee64fa883d107ef9e558472c4eb9aaaefa459d";
  
  SimulateNip05Response("example.com", 
                       CreateNip05Response("_", test_pubkey));
  
  GURL url("nostr://nsite/example.com");
  
  bool callback_called = false;
  resolver_->Resolve(url, base::BindOnce(
      [](bool* called, const std::string& expected_pubkey,
         std::optional<NsiteResolver::ResolveResult> result) {
        *called = true;
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->pubkey, expected_pubkey);
        EXPECT_EQ(result->domain, "example.com");
        EXPECT_EQ(result->path, "");
      },
      &callback_called, test_pubkey));
  
  RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(NsiteResolverTest, Nip05NotFound) {
  SimulateNip05Response("example.com", 
                       CreateNip05Response("bob", "somepubkey"));
  
  GURL url("nostr://nsite/alice@example.com");
  
  bool callback_called = false;
  resolver_->Resolve(url, base::BindOnce(
      [](bool* called, std::optional<NsiteResolver::ResolveResult> result) {
        *called = true;
        EXPECT_FALSE(result.has_value());
      },
      &callback_called));
  
  RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(NsiteResolverTest, Nip05InvalidJson) {
  SimulateNip05Response("example.com", "invalid json");
  
  GURL url("nostr://nsite/alice@example.com");
  
  bool callback_called = false;
  resolver_->Resolve(url, base::BindOnce(
      [](bool* called, std::optional<NsiteResolver::ResolveResult> result) {
        *called = true;
        EXPECT_FALSE(result.has_value());
      },
      &callback_called));
  
  RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(NsiteResolverTest, Nip05HttpError) {
  SimulateNip05Response("example.com", "", net::HTTP_NOT_FOUND);
  
  GURL url("nostr://nsite/alice@example.com");
  
  bool callback_called = false;
  resolver_->Resolve(url, base::BindOnce(
      [](bool* called, std::optional<NsiteResolver::ResolveResult> result) {
        *called = true;
        EXPECT_FALSE(result.has_value());
      },
      &callback_called));
  
  RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(NsiteResolverTest, Nip05InvalidPubkey) {
  // Invalid pubkey (not 64 hex chars)
  SimulateNip05Response("example.com", 
                       CreateNip05Response("alice", "invalid"));
  
  GURL url("nostr://nsite/alice@example.com");
  
  bool callback_called = false;
  resolver_->Resolve(url, base::BindOnce(
      [](bool* called, std::optional<NsiteResolver::ResolveResult> result) {
        *called = true;
        EXPECT_FALSE(result.has_value());
      },
      &callback_called));
  
  RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(NsiteResolverTest, CacheHit) {
  const std::string test_pubkey = 
      "3bf0c63fcb93463407af97a5e5ee64fa883d107ef9e558472c4eb9aaaefa459d";
  
  SimulateNip05Response("example.com", 
                       CreateNip05Response("alice", test_pubkey));
  
  GURL url("nostr://nsite/alice@example.com");
  
  // First request
  bool first_called = false;
  resolver_->Resolve(url, base::BindOnce(
      [](bool* called, std::optional<NsiteResolver::ResolveResult> result) {
        *called = true;
        ASSERT_TRUE(result.has_value());
      },
      &first_called));
  
  RunUntilIdle();
  EXPECT_TRUE(first_called);
  
  // Clear responses to ensure second request uses cache
  test_url_loader_factory_.ClearResponses();
  
  // Second request should hit cache
  bool second_called = false;
  resolver_->Resolve(url, base::BindOnce(
      [](bool* called, const std::string& expected_pubkey,
         std::optional<NsiteResolver::ResolveResult> result) {
        *called = true;
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->pubkey, expected_pubkey);
      },
      &second_called, test_pubkey));
  
  RunUntilIdle();
  EXPECT_TRUE(second_called);
}

TEST_F(NsiteResolverTest, CacheClear) {
  const std::string test_pubkey = 
      "3bf0c63fcb93463407af97a5e5ee64fa883d107ef9e558472c4eb9aaaefa459d";
  
  SimulateNip05Response("example.com", 
                       CreateNip05Response("alice", test_pubkey));
  
  GURL url("nostr://nsite/alice@example.com");
  
  // First request
  bool first_called = false;
  resolver_->Resolve(url, base::BindOnce(
      [](bool* called, std::optional<NsiteResolver::ResolveResult> result) {
        *called = true;
        ASSERT_TRUE(result.has_value());
      },
      &first_called));
  
  RunUntilIdle();
  EXPECT_TRUE(first_called);
  
  // Clear cache
  resolver_->ClearCache();
  
  // Clear responses to ensure request fails without cache
  test_url_loader_factory_.ClearResponses();
  
  // Second request should fail
  bool second_called = false;
  resolver_->Resolve(url, base::BindOnce(
      [](bool* called, std::optional<NsiteResolver::ResolveResult> result) {
        *called = true;
        EXPECT_FALSE(result.has_value());
      },
      &second_called));
  
  RunUntilIdle();
  EXPECT_TRUE(second_called);
}

TEST_F(NsiteResolverTest, ComplexPath) {
  const std::string test_pubkey = 
      "3bf0c63fcb93463407af97a5e5ee64fa883d107ef9e558472c4eb9aaaefa459d";
  
  SimulateNip05Response("example.com", 
                       CreateNip05Response("alice", test_pubkey));
  
  GURL url("nostr://nsite/alice@example.com/path/to/deep/content.html?query=123#anchor");
  
  bool callback_called = false;
  resolver_->Resolve(url, base::BindOnce(
      [](bool* called, std::optional<NsiteResolver::ResolveResult> result) {
        *called = true;
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->path, "path/to/deep/content.html?query=123#anchor");
      },
      &callback_called));
  
  RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(NsiteResolverTest, DnsTxtLookupStub) {
  // DNS TXT lookup is not yet implemented and should always fail
  // This test ensures the stub behavior is consistent
  
  // Use a domain-only identifier that would trigger DNS lookup
  // after NIP-05 fails
  GURL url("nostr://nsite/dns-only-domain.com");
  
  // Don't set up any NIP-05 response, so it will fall through to DNS
  
  bool callback_called = false;
  resolver_->Resolve(url, base::BindOnce(
      [](bool* called, std::optional<NsiteResolver::ResolveResult> result) {
        *called = true;
        // DNS lookup is stubbed to always fail
        EXPECT_FALSE(result.has_value());
      },
      &callback_called));
  
  RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

}  // namespace nostr