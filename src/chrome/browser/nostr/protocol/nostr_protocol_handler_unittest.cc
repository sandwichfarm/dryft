// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/protocol/nostr_protocol_handler.h"

#include "base/test/task_environment.h"
#include "chrome/common/nostr_scheme.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace nostr {

class NostrProtocolHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    factory_ = NostrProtocolURLLoaderFactory::Create(&browser_context_);
  }

  base::test::TaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_;
};

TEST_F(NostrProtocolHandlerTest, ConvertNostrUrlToLocalhost) {
  auto factory = new NostrProtocolURLLoaderFactory(
      &browser_context_, factory_.InitWithNewPipeAndPassReceiver());
  
  // Test nostr:// URL conversion
  GURL nostr_url("nostr://example.com/path?query=value#fragment");
  GURL converted = factory->ConvertNostrUrlToLocalhost(nostr_url);
  
  EXPECT_TRUE(converted.is_valid());
  EXPECT_EQ("http", converted.scheme());
  EXPECT_EQ("localhost", converted.host());
  EXPECT_EQ("/nostr/example.com/path", converted.path());
  EXPECT_EQ("query=value", converted.query());
  EXPECT_EQ("fragment", converted.ref());
}

TEST_F(NostrProtocolHandlerTest, ConvertSecureNostrUrlToLocalhost) {
  auto factory = new NostrProtocolURLLoaderFactory(
      &browser_context_, factory_.InitWithNewPipeAndPassReceiver());
  
  // Test snostr:// URL conversion
  GURL snostr_url("snostr://example.com/secure/path");
  GURL converted = factory->ConvertNostrUrlToLocalhost(snostr_url);
  
  EXPECT_TRUE(converted.is_valid());
  EXPECT_EQ("https", converted.scheme());
  EXPECT_EQ("localhost", converted.host());
  EXPECT_EQ("/nostr/example.com/secure/path", converted.path());
}

TEST_F(NostrProtocolHandlerTest, InvalidUrlReturnsEmpty) {
  auto factory = new NostrProtocolURLLoaderFactory(
      &browser_context_, factory_.InitWithNewPipeAndPassReceiver());
  
  // Test invalid URL
  GURL invalid_url("invalid-url");
  GURL converted = factory->ConvertNostrUrlToLocalhost(invalid_url);
  
  EXPECT_FALSE(converted.is_valid());
}

TEST_F(NostrProtocolHandlerTest, NonNostrSchemeReturnsEmpty) {
  auto factory = new NostrProtocolURLLoaderFactory(
      &browser_context_, factory_.InitWithNewPipeAndPassReceiver());
  
  // Test non-nostr scheme
  GURL http_url("http://example.com");
  GURL converted = factory->ConvertNostrUrlToLocalhost(http_url);
  
  EXPECT_FALSE(converted.is_valid());
}

TEST_F(NostrProtocolHandlerTest, NostrUrlWithoutHost) {
  auto factory = new NostrProtocolURLLoaderFactory(
      &browser_context_, factory_.InitWithNewPipeAndPassReceiver());
  
  // Test nostr:// URL without host
  GURL nostr_url("nostr:///path/only");
  GURL converted = factory->ConvertNostrUrlToLocalhost(nostr_url);
  
  EXPECT_TRUE(converted.is_valid());
  EXPECT_EQ("http", converted.scheme());
  EXPECT_EQ("localhost", converted.host());
  EXPECT_EQ("/nostr/path/only", converted.path());
}

}  // namespace nostr