// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/integration_tests/nostr_integration_test_base.h"

#include "base/test/bind.h"
#include "chrome/browser/nostr/integration_tests/test_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace nostr {

class NostrAPIIntegrationTest : public InProcessBrowserTest {
 public:
  NostrAPIIntegrationTest() = default;
  ~NostrAPIIntegrationTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

// Test that window.nostr is injected on regular pages
IN_PROC_BROWSER_TEST_F(NostrAPIIntegrationTest, WindowNostrInjected) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  bool has_nostr = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send(typeof window.nostr === 'object');",
      &has_nostr));
  EXPECT_TRUE(has_nostr);
}

// Test that window.nostr has all required NIP-07 methods
IN_PROC_BROWSER_TEST_F(NostrAPIIntegrationTest, NIP07MethodsAvailable) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  EXPECT_TRUE(test::CheckNostrAPIAvailable(web_contents()));
  
  // Check individual methods
  const std::vector<std::string> required_methods = {
      "getPublicKey", "signEvent", "getRelays",
      "nip04.encrypt", "nip04.decrypt",
      "nip44.encrypt", "nip44.decrypt"
  };
  
  for (const auto& method : required_methods) {
    bool has_method = false;
    std::string script = base::StringPrintf(
        "window.domAutomationController.send("
        "  typeof window.nostr.%s === 'function'"
        ");",
        method.c_str());
    
    if (method.find('.') != std::string::npos) {
      // Handle nested methods like nip04.encrypt
      std::string parent = method.substr(0, method.find('.'));
      std::string child = method.substr(method.find('.') + 1);
      script = base::StringPrintf(
          "window.domAutomationController.send("
          "  window.nostr.%s && typeof window.nostr.%s.%s === 'function'"
          ");",
          parent.c_str(), parent.c_str(), child.c_str());
    }
    
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents(), script, &has_method))
        << "Failed to check method: " << method;
    EXPECT_TRUE(has_method) << "Missing method: " << method;
  }
}

// Test that window.nostr is not injected on chrome:// URLs
IN_PROC_BROWSER_TEST_F(NostrAPIIntegrationTest, NoInjectionOnChromeURLs) {
  GURL chrome_url("chrome://settings");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chrome_url));
  
  bool has_nostr = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send(typeof window.nostr !== 'undefined');",
      &has_nostr));
  EXPECT_FALSE(has_nostr);
}

// Test getPublicKey without permission returns error
IN_PROC_BROWSER_TEST_F(NostrAPIIntegrationTest, GetPublicKeyRequiresPermission) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey()"
      "  .then(() => window.domAutomationController.send('resolved'))"
      "  .catch(() => window.domAutomationController.send('rejected'));",
      &result));
  EXPECT_EQ("rejected", result);
}

// Test signEvent without permission returns error
IN_PROC_BROWSER_TEST_F(NostrAPIIntegrationTest, SignEventRequiresPermission) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  std::string event_json = test::CreateTestEvent(1, "test content");
  std::string script = base::StringPrintf(
      "window.nostr.signEvent(%s)"
      "  .then(() => window.domAutomationController.send('resolved'))"
      "  .catch(() => window.domAutomationController.send('rejected'));",
      event_json.c_str());
  
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), script, &result));
  EXPECT_EQ("rejected", result);
}

// Test that window.nostr.libs contains library paths
IN_PROC_BROWSER_TEST_F(NostrAPIIntegrationTest, NostrLibsAvailable) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  bool has_libs = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send("
      "  typeof window.nostr.libs === 'object'"
      ");",
      &has_libs));
  EXPECT_TRUE(has_libs);
  
  // Check specific libraries
  const std::vector<std::string> expected_libs = {
      "ndk", "nostr-tools", "nostr-wasm", "rx-nostr"
  };
  
  for (const auto& lib : expected_libs) {
    EXPECT_TRUE(test::CheckNostrLibraryAvailable(web_contents(), lib))
        << "Missing library: " << lib;
  }
}

// Test that library paths can be dynamically imported
IN_PROC_BROWSER_TEST_F(NostrAPIIntegrationTest, LibraryDynamicImport) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Try to import NDK
  bool import_success = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      "import(window.nostr.libs.ndk)"
      "  .then(() => { window.domAutomationController.send(true); })"
      "  .catch(() => { window.domAutomationController.send(false); });",
      &import_success));
  EXPECT_TRUE(import_success);
}

// Test that window.nostr.libs properties are read-only
IN_PROC_BROWSER_TEST_F(NostrAPIIntegrationTest, NostrLibsReadOnly) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  bool modified = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      "try {"
      "  window.nostr.libs.ndk = 'modified';"
      "  window.domAutomationController.send(window.nostr.libs.ndk === 'modified');"
      "} catch (e) {"
      "  window.domAutomationController.send(false);"
      "}",
      &modified));
  EXPECT_FALSE(modified);
}

// Test window.nostr.relay properties
IN_PROC_BROWSER_TEST_F(NostrAPIIntegrationTest, NostrRelayProperties) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Check relay object exists
  bool has_relay = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send("
      "  typeof window.nostr.relay === 'object'"
      ");",
      &has_relay));
  EXPECT_TRUE(has_relay);
  
  // Check relay URL
  std::string relay_url;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(window.nostr.relay.url);",
      &relay_url));
  EXPECT_EQ("ws://localhost:4869", relay_url);
  
  // Check connected status (should be false initially)
  bool connected = true;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send(window.nostr.relay.connected);",
      &connected));
  EXPECT_FALSE(connected);
}

// Test that relay URL is read-only
IN_PROC_BROWSER_TEST_F(NostrAPIIntegrationTest, RelayURLReadOnly) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  bool modified = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      "try {"
      "  window.nostr.relay.url = 'ws://modified.com';"
      "  window.domAutomationController.send("
      "    window.nostr.relay.url === 'ws://modified.com'"
      "  );"
      "} catch (e) {"
      "  window.domAutomationController.send(false);"
      "}",
      &modified));
  EXPECT_FALSE(modified);
}

// Test NIP-04 encryption/decryption placeholder
IN_PROC_BROWSER_TEST_F(NostrAPIIntegrationTest, NIP04Methods) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Test that methods return promises
  bool returns_promise = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send("
      "  window.nostr.nip04.encrypt('pubkey', 'text') instanceof Promise &&"
      "  window.nostr.nip04.decrypt('pubkey', 'cipher') instanceof Promise"
      ");",
      &returns_promise));
  EXPECT_TRUE(returns_promise);
}

// Test NIP-44 encryption/decryption placeholder
IN_PROC_BROWSER_TEST_F(NostrAPIIntegrationTest, NIP44Methods) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Test that methods return promises
  bool returns_promise = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send("
      "  window.nostr.nip44.encrypt('pubkey', 'text') instanceof Promise &&"
      "  window.nostr.nip44.decrypt('pubkey', 'cipher') instanceof Promise"
      ");",
      &returns_promise));
  EXPECT_TRUE(returns_promise);
}

// Test error handling for invalid parameters
IN_PROC_BROWSER_TEST_F(NostrAPIIntegrationTest, InvalidParameterHandling) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Test signEvent with invalid JSON
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.signEvent('invalid json')"
      "  .then(() => window.domAutomationController.send('resolved'))"
      "  .catch(() => window.domAutomationController.send('rejected'));",
      &result));
  EXPECT_EQ("rejected", result);
  
  // Test nip04.encrypt with missing parameters
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.nip04.encrypt()"
      "  .then(() => window.domAutomationController.send('resolved'))"
      "  .catch(() => window.domAutomationController.send('rejected'));",
      &result));
  EXPECT_EQ("rejected", result);
}

}  // namespace nostr