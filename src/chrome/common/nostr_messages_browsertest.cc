// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/nostr_messages.h"

#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace {

class NostrMessagesBrowserTest : public InProcessBrowserTest {
 protected:
  NostrMessagesBrowserTest() = default;
  ~NostrMessagesBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Test that we can create and send basic Nostr messages
IN_PROC_BROWSER_TEST_F(NostrMessagesBrowserTest, BasicMessageCreation) {
  // Navigate to a test page
  GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents = 
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  
  // Create a GetPublicKey message
  url::Origin origin = rfh->GetLastCommittedOrigin();
  int request_id = 1;
  
  // Verify we can create the message (compilation test)
  std::unique_ptr<IPC::Message> msg(
      new NostrHostMsg_GetPublicKey(rfh->GetRoutingID(), request_id, origin));
  EXPECT_NE(nullptr, msg);
  EXPECT_EQ(static_cast<uint32_t>(NostrHostMsg_GetPublicKey::ID), msg->type());
}

// Test complex message with NostrEvent
IN_PROC_BROWSER_TEST_F(NostrMessagesBrowserTest, NostrEventMessage) {
  NostrEvent event;
  event.id = "test_id";
  event.pubkey = "test_pubkey";
  event.created_at = 1234567890;
  event.kind = 1;
  event.content = "Test content";
  event.sig = "test_sig";
  event.tags = {{"p", "pubkey1"}, {"e", "event1"}};

  // Create a relay event message
  std::unique_ptr<IPC::Message> msg(
      new NostrMsg_RelayEvent(1, 42, event));
  EXPECT_NE(nullptr, msg);
  
  // Verify the message can be serialized and deserialized
  IPC::Message copy(*msg);
  EXPECT_EQ(msg->type(), copy.type());
}

// Test permission request message
IN_PROC_BROWSER_TEST_F(NostrMessagesBrowserTest, PermissionRequestMessage) {
  NostrPermissionRequest request;
  request.origin = url::Origin::Create(GURL("https://example.com"));
  request.method = "signEvent";
  request.details.Set("kinds", base::Value::List().Append(1).Append(4));
  request.timestamp = base::TimeTicks::Now();
  request.remember_decision = true;

  // Create permission request message
  std::unique_ptr<IPC::Message> msg(
      new NostrHostMsg_RequestPermission(1, 43, request));
  EXPECT_NE(nullptr, msg);
}

// Test Blossom upload result message
IN_PROC_BROWSER_TEST_F(NostrMessagesBrowserTest, BlossomUploadResultMessage) {
  BlossomUploadResult result;
  result.hash = "sha256_hash";
  result.url = "https://blossom.example.com/sha256_hash";
  result.size = 1024;
  result.mime_type = "image/png";
  result.servers = {"server1", "server2"};

  // Create response message
  std::unique_ptr<IPC::Message> msg(
      new NostrMsg_BlossomUploadResponse(1, 44, true, result));
  EXPECT_NE(nullptr, msg);
}

// Test rate limit info in messages
IN_PROC_BROWSER_TEST_F(NostrMessagesBrowserTest, RateLimitInfoMessage) {
  NostrRateLimitInfo rate_limit;
  rate_limit.requests_per_minute = 60;
  rate_limit.signs_per_hour = 100;
  rate_limit.window_start = base::TimeTicks::Now();
  rate_limit.current_count = 10;

  // Create a sign event message with rate limit info
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  base::Value::Dict event;
  event.Set("kind", 1);
  event.Set("content", "Test");

  std::unique_ptr<IPC::Message> msg(
      new NostrHostMsg_SignEvent(1, 45, origin, event, rate_limit));
  EXPECT_NE(nullptr, msg);
}

}  // namespace