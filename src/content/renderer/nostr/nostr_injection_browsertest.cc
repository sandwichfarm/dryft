// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace tungsten {

class NostrInjectionBrowserTest : public content::ContentBrowserTest {
 public:
  NostrInjectionBrowserTest() = default;
  ~NostrInjectionBrowserTest() override = default;

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  content::WebContents* web_contents() {
    return shell()->web_contents();
  }
};

// Test that window.nostr is injected on regular pages
IN_PROC_BROWSER_TEST_F(NostrInjectionBrowserTest, WindowNostrExists) {
  GURL url = embedded_test_server()->GetURL("/simple.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Check that window.nostr exists
  bool has_nostr = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send(typeof window.nostr === 'object');",
      &has_nostr));
  EXPECT_TRUE(has_nostr);
}

// Test that window.nostr has required NIP-07 methods
IN_PROC_BROWSER_TEST_F(NostrInjectionBrowserTest, NostrHasRequiredMethods) {
  GURL url = embedded_test_server()->GetURL("/simple.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Check getPublicKey exists
  bool has_get_pubkey = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send("
      "  typeof window.nostr.getPublicKey === 'function');",
      &has_get_pubkey));
  EXPECT_TRUE(has_get_pubkey);

  // Check signEvent exists
  bool has_sign_event = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send("
      "  typeof window.nostr.signEvent === 'function');",
      &has_sign_event));
  EXPECT_TRUE(has_sign_event);

  // Check getRelays exists
  bool has_get_relays = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send("
      "  typeof window.nostr.getRelays === 'function');",
      &has_get_relays));
  EXPECT_TRUE(has_get_relays);

  // Check nip04 object exists
  bool has_nip04 = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send("
      "  typeof window.nostr.nip04 === 'object');",
      &has_nip04));
  EXPECT_TRUE(has_nip04);
}

// Test that window.nostr is NOT injected on chrome:// URLs
IN_PROC_BROWSER_TEST_F(NostrInjectionBrowserTest, NoInjectionOnChromeURLs) {
  GURL url("chrome://version");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Check that window.nostr does NOT exist
  bool has_nostr = true;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send(typeof window.nostr !== 'undefined');",
      &has_nostr));
  EXPECT_FALSE(has_nostr);
}

// Test that NIP-07 methods return promises
IN_PROC_BROWSER_TEST_F(NostrInjectionBrowserTest, MethodsReturnPromises) {
  GURL url = embedded_test_server()->GetURL("/simple.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Test getPublicKey returns a promise
  std::string pubkey_result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey()"
      "  .then(() => window.domAutomationController.send('resolved'))"
      "  .catch(() => window.domAutomationController.send('rejected'));",
      &pubkey_result));
  EXPECT_EQ("rejected", pubkey_result);  // Should reject as it's a stub

  // Test signEvent returns a promise
  std::string sign_result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.signEvent({kind: 1, content: 'test'})"
      "  .then(() => window.domAutomationController.send('resolved'))"
      "  .catch(() => window.domAutomationController.send('rejected'));",
      &sign_result));
  EXPECT_EQ("rejected", sign_result);  // Should reject as it's a stub
}

// Test that window.nostr is not injected in iframes (for now)
IN_PROC_BROWSER_TEST_F(NostrInjectionBrowserTest, NoInjectionInIframes) {
  GURL url = embedded_test_server()->GetURL("/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Check main frame has window.nostr
  bool main_has_nostr = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send(typeof window.nostr === 'object');",
      &main_has_nostr));
  EXPECT_TRUE(main_has_nostr);

  // Check iframe does NOT have window.nostr
  bool iframe_has_nostr = true;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(),
      "var iframe = document.querySelector('iframe');"
      "var iframeNostr = iframe.contentWindow.nostr;"
      "window.domAutomationController.send(typeof iframeNostr !== 'undefined');",
      &iframe_has_nostr));
  EXPECT_FALSE(iframe_has_nostr);
}

// Test that nip04 methods exist and return promises
IN_PROC_BROWSER_TEST_F(NostrInjectionBrowserTest, Nip04MethodsExist) {
  GURL url = embedded_test_server()->GetURL("/simple.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Check nip04.encrypt exists
  bool has_encrypt = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send("
      "  typeof window.nostr.nip04.encrypt === 'function');",
      &has_encrypt));
  EXPECT_TRUE(has_encrypt);

  // Check nip04.decrypt exists
  bool has_decrypt = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send("
      "  typeof window.nostr.nip04.decrypt === 'function');",
      &has_decrypt));
  EXPECT_TRUE(has_decrypt);

  // Test encrypt returns a promise
  std::string encrypt_result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.nip04.encrypt('pubkey', 'message')"
      "  .then(() => window.domAutomationController.send('resolved'))"
      "  .catch(() => window.domAutomationController.send('rejected'));",
      &encrypt_result));
  EXPECT_EQ("rejected", encrypt_result);  // Should reject as it's a stub
}

}  // namespace tungsten