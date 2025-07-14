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

// Test that window.nostr.libs exists and has library URLs
IN_PROC_BROWSER_TEST_F(NostrInjectionBrowserTest, NostrLibsExists) {
  GURL url = embedded_test_server()->GetURL("/simple.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Check that window.nostr.libs exists
  bool has_libs = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send("
      "  typeof window.nostr.libs === 'object');",
      &has_libs));
  EXPECT_TRUE(has_libs);

  // Check that each library URL is a string
  std::string ndk_type;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(typeof window.nostr.libs.ndk);",
      &ndk_type));
  EXPECT_EQ("string", ndk_type);

  // Check the actual URL format
  std::string ndk_url;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(window.nostr.libs.ndk);",
      &ndk_url));
  EXPECT_EQ("chrome://resources/js/nostr/ndk.js", ndk_url);

  // Check nostr-tools URL
  std::string nostr_tools_url;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(window.nostr.libs['nostr-tools']);",
      &nostr_tools_url));
  EXPECT_EQ("chrome://resources/js/nostr/nostr-tools.js", nostr_tools_url);

  // Check applesauce-core URL
  std::string applesauce_core_url;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(window.nostr.libs['applesauce-core']);",
      &applesauce_core_url));
  EXPECT_EQ("chrome://resources/js/nostr/applesauce-core.js", applesauce_core_url);

  // Check applesauce-content URL
  std::string applesauce_content_url;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(window.nostr.libs['applesauce-content']);",
      &applesauce_content_url));
  EXPECT_EQ("chrome://resources/js/nostr/applesauce-content.js", applesauce_content_url);

  // Check applesauce-lists URL
  std::string applesauce_lists_url;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(window.nostr.libs['applesauce-lists']);",
      &applesauce_lists_url));
  EXPECT_EQ("chrome://resources/js/nostr/applesauce-lists.js", applesauce_lists_url);

  // Check alby-sdk URL
  std::string alby_sdk_url;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(window.nostr.libs['alby-sdk']);",
      &alby_sdk_url));
  EXPECT_EQ("chrome://resources/js/nostr/alby-sdk.js", alby_sdk_url);
}

// Test that window.nostr.libs properties are read-only
IN_PROC_BROWSER_TEST_F(NostrInjectionBrowserTest, NostrLibsReadOnly) {
  GURL url = embedded_test_server()->GetURL("/simple.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Try to modify a library URL
  bool modified = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(),
      "try {"
      "  window.nostr.libs.ndk = 'modified';"
      "  window.domAutomationController.send(window.nostr.libs.ndk === 'modified');"
      "} catch (e) {"
      "  window.domAutomationController.send(false);"
      "}",
      &modified));
  EXPECT_FALSE(modified);

  // Verify the URL is unchanged
  std::string ndk_url;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(window.nostr.libs.ndk);",
      &ndk_url));
  EXPECT_EQ("chrome://resources/js/nostr/ndk.js", ndk_url);
}

// Test that window.nostr.libs.versions exists
IN_PROC_BROWSER_TEST_F(NostrInjectionBrowserTest, NostrLibsVersions) {
  GURL url = embedded_test_server()->GetURL("/simple.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Check that versions object exists
  bool has_versions = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send("
      "  typeof window.nostr.libs.versions === 'object');",
      &has_versions));
  EXPECT_TRUE(has_versions);

  // Check NDK version
  std::string ndk_version;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(window.nostr.libs.versions.ndk);",
      &ndk_version));
  EXPECT_EQ("2.0.0", ndk_version);

  // Check nostr-tools version
  std::string nostr_tools_version;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(window.nostr.libs.versions['nostr-tools']);",
      &nostr_tools_version));
  EXPECT_EQ("1.17.0", nostr_tools_version);
}

// Test dynamic import of libraries
IN_PROC_BROWSER_TEST_F(NostrInjectionBrowserTest, DynamicImportLibraries) {
  GURL url = embedded_test_server()->GetURL("/simple.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Test importing NDK (this will fail in tests as chrome:// URLs aren't served)
  std::string import_result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(),
      "import(window.nostr.libs.ndk)"
      "  .then(() => window.domAutomationController.send('loaded'))"
      "  .catch(e => window.domAutomationController.send('failed: ' + e.message));",
      &import_result));
  // In browser tests, chrome:// URLs may not be fully functional
  EXPECT_TRUE(import_result.find("failed") != std::string::npos);
}

}  // namespace tungsten