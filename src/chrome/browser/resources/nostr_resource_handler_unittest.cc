// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources/nostr_resource_handler.h"

#include "base/memory/ref_counted_memory.h"
#include "chrome/grit/nostr_resources.h"
#include "content/public/test/test_browser_context.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chrome {

class NostrResourceHandlerTest : public testing::Test {
 protected:
  NostrResourceHandlerTest() = default;
  ~NostrResourceHandlerTest() override = default;

  void SetUp() override {
    handler_ = std::make_unique<NostrResourceHandler>();
  }

  std::unique_ptr<NostrResourceHandler> handler_;
};

TEST_F(NostrResourceHandlerTest, GetSource) {
  EXPECT_EQ("resources/js/nostr", handler_->GetSource());
}

TEST_F(NostrResourceHandlerTest, GetMimeType) {
  GURL url("chrome://resources/js/nostr/ndk.js");
  EXPECT_EQ("application/javascript", handler_->GetMimeType(url));
}

TEST_F(NostrResourceHandlerTest, ShouldServeMimeTypeAsContentTypeHeader) {
  EXPECT_TRUE(handler_->ShouldServeMimeTypeAsContentTypeHeader());
}

TEST_F(NostrResourceHandlerTest, AllowCaching) {
  EXPECT_TRUE(handler_->AllowCaching());
}

TEST_F(NostrResourceHandlerTest, AddResponseHeaders) {
  GURL url("chrome://resources/js/nostr/ndk.js");
  scoped_refptr<content::HttpResponseHeaders> headers =
      content::HttpResponseHeaders::Builder("HTTP/1.1 200 OK").Build();
  
  handler_->AddResponseHeaders(url, headers.get());
  
  // Check CORS header
  std::string cors_value;
  EXPECT_TRUE(headers->GetNormalizedHeader("Access-Control-Allow-Origin", &cors_value));
  EXPECT_EQ("*", cors_value);
  
  // Check cache header
  std::string cache_value;
  EXPECT_TRUE(headers->GetNormalizedHeader("Cache-Control", &cache_value));
  EXPECT_EQ("public, max-age=31536000", cache_value);
  
  // Check security header
  std::string security_value;
  EXPECT_TRUE(headers->GetNormalizedHeader("X-Content-Type-Options", &security_value));
  EXPECT_EQ("nosniff", security_value);
}

TEST_F(NostrResourceHandlerTest, GetResourceIdForPath) {
  // Test with leading slash
  EXPECT_EQ(IDR_NOSTR_NDK_JS, handler_->GetResourceIdForPath("/ndk.js"));
  EXPECT_EQ(IDR_NOSTR_TOOLS_JS, handler_->GetResourceIdForPath("/nostr-tools.js"));
  EXPECT_EQ(IDR_NOSTR_APPLESAUCE_JS, handler_->GetResourceIdForPath("/applesauce.js"));
  EXPECT_EQ(IDR_NOSTR_NOSTRIFY_JS, handler_->GetResourceIdForPath("/nostrify.js"));
  EXPECT_EQ(IDR_NOSTR_ALBY_SDK_JS, handler_->GetResourceIdForPath("/alby-sdk.js"));
  
  // Test without leading slash
  EXPECT_EQ(IDR_NOSTR_NDK_JS, handler_->GetResourceIdForPath("ndk.js"));
  EXPECT_EQ(IDR_NOSTR_TOOLS_JS, handler_->GetResourceIdForPath("nostr-tools.js"));
  
  // Test unknown resource
  EXPECT_EQ(0, handler_->GetResourceIdForPath("/unknown.js"));
  EXPECT_EQ(0, handler_->GetResourceIdForPath(""));
}

// Note: StartDataRequest would need a mock environment to test properly
// as it relies on ResourceBundle which requires a full Chrome environment

}  // namespace chrome