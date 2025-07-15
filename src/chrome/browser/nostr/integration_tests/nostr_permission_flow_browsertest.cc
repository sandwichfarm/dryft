// Copyright 2024 The Tungsten Authors
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
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace nostr {

class NostrPermissionFlowBrowserTest : public InProcessBrowserTest,
                                      public NostrIntegrationTestBase {
 public:
  NostrPermissionFlowBrowserTest() = default;
  ~NostrPermissionFlowBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    NostrIntegrationTestBase::SetUp();
    
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    NostrIntegrationTestBase::TearDown();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

// Test initial permission request flow
IN_PROC_BROWSER_TEST_F(NostrPermissionFlowBrowserTest, InitialPermissionRequest) {
  // Setup key
  std::string pubkey = CreateAndStoreTestKey("perm-test-key", "password");
  
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      pubkey,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  base::RunLoop unlock_loop;
  nostr_service()->UnlockKey(
      pubkey, "password",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Initially, permission should not be granted
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey()"
      "  .then(() => window.domAutomationController.send('granted'))"
      "  .catch(() => window.domAutomationController.send('denied'));",
      &result));
  EXPECT_EQ("denied", result);
  
  // Grant permission
  GrantNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  // Now it should work
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey()"
      "  .then(() => window.domAutomationController.send('granted'))"
      "  .catch(() => window.domAutomationController.send('denied'));",
      &result));
  EXPECT_EQ("granted", result);
}

// Test permission persistence across navigations
IN_PROC_BROWSER_TEST_F(NostrPermissionFlowBrowserTest, PermissionPersistence) {
  // Setup
  std::string pubkey = CreateAndStoreTestKey("persist-key", "password");
  
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      pubkey,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  base::RunLoop unlock_loop;
  nostr_service()->UnlockKey(
      pubkey, "password",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  // Navigate to first page and grant permission
  GURL test_url1 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url1));
  GrantNIP07Permission(test_url1.DeprecatedGetOriginAsURL());
  
  // Verify permission works
  std::string pubkey_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey()"
      "  .then(pk => window.domAutomationController.send(pk));",
      &pubkey_result));
  EXPECT_EQ(pubkey, pubkey_result);
  
  // Navigate to another page on same origin
  GURL test_url2 = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url2));
  
  // Permission should still be granted
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey()"
      "  .then(pk => window.domAutomationController.send(pk));",
      &pubkey_result));
  EXPECT_EQ(pubkey, pubkey_result);
}

// Test different origins have separate permissions
IN_PROC_BROWSER_TEST_F(NostrPermissionFlowBrowserTest, PerOriginPermissions) {
  // Setup
  std::string pubkey = CreateAndStoreTestKey("origin-key", "password");
  
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      pubkey,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  base::RunLoop unlock_loop;
  nostr_service()->UnlockKey(
      pubkey, "password",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  // Grant permission to origin A
  GURL url_a = embedded_test_server()->GetURL("a.com", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  GrantNIP07Permission(url_a.DeprecatedGetOriginAsURL());
  
  // Verify it works on origin A
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey()"
      "  .then(() => window.domAutomationController.send('granted'))"
      "  .catch(() => window.domAutomationController.send('denied'));",
      &result));
  EXPECT_EQ("granted", result);
  
  // Navigate to origin B
  GURL url_b = embedded_test_server()->GetURL("b.com", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  
  // Should not have permission on origin B
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey()"
      "  .then(() => window.domAutomationController.send('granted'))"
      "  .catch(() => window.domAutomationController.send('denied'));",
      &result));
  EXPECT_EQ("denied", result);
}

// Test permission revocation
IN_PROC_BROWSER_TEST_F(NostrPermissionFlowBrowserTest, PermissionRevocation) {
  // Setup
  std::string pubkey = CreateAndStoreTestKey("revoke-key", "password");
  
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      pubkey,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  base::RunLoop unlock_loop;
  nostr_service()->UnlockKey(
      pubkey, "password",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Grant permission
  GrantNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  // Verify it works
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey()"
      "  .then(() => window.domAutomationController.send('granted'))"
      "  .catch(() => window.domAutomationController.send('denied'));",
      &result));
  EXPECT_EQ("granted", result);
  
  // Revoke permission
  DenyNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  // Should now be denied
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey()"
      "  .then(() => window.domAutomationController.send('granted'))"
      "  .catch(() => window.domAutomationController.send('denied'));",
      &result));
  EXPECT_EQ("denied", result);
}

// Test permission for different NIP-07 methods
IN_PROC_BROWSER_TEST_F(NostrPermissionFlowBrowserTest, MethodSpecificPermissions) {
  // Setup
  std::string pubkey = CreateAndStoreTestKey("method-key", "password");
  
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      pubkey,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  base::RunLoop unlock_loop;
  nostr_service()->UnlockKey(
      pubkey, "password",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  GrantNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  // Test all NIP-07 methods require permission
  std::string test_script = R"(
    (async () => {
      const results = {};
      
      // Test getPublicKey
      try {
        await window.nostr.getPublicKey();
        results.getPublicKey = 'granted';
      } catch (e) {
        results.getPublicKey = 'denied';
      }
      
      // Test signEvent
      try {
        await window.nostr.signEvent({ kind: 1, content: 'test' });
        results.signEvent = 'granted';
      } catch (e) {
        results.signEvent = 'denied';
      }
      
      // Test getRelays
      try {
        await window.nostr.getRelays();
        results.getRelays = 'granted';
      } catch (e) {
        results.getRelays = 'denied';
      }
      
      // Test nip04.encrypt
      try {
        await window.nostr.nip04.encrypt('pubkey', 'text');
        results.nip04Encrypt = 'granted';
      } catch (e) {
        results.nip04Encrypt = 'denied';
      }
      
      // Test nip44.encrypt
      try {
        await window.nostr.nip44.encrypt('pubkey', 'text');
        results.nip44Encrypt = 'granted';
      } catch (e) {
        results.nip44Encrypt = 'denied';
      }
      
      return JSON.stringify(results);
    })().then(result => window.domAutomationController.send(result));
  )";
  
  std::string results_json;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), test_script, &results_json));
  
  auto results = base::JSONReader::Read(results_json);
  ASSERT_TRUE(results && results->is_dict());
  
  const auto& dict = results->GetDict();
  // All methods should be granted with NIP-07 permission
  EXPECT_EQ("granted", *dict.FindString("getPublicKey"));
  EXPECT_EQ("granted", *dict.FindString("signEvent"));
  EXPECT_EQ("granted", *dict.FindString("getRelays"));
  EXPECT_EQ("granted", *dict.FindString("nip04Encrypt"));
  EXPECT_EQ("granted", *dict.FindString("nip44Encrypt"));
}

// Test permission prompt UI elements (mock)
IN_PROC_BROWSER_TEST_F(NostrPermissionFlowBrowserTest, PermissionPromptUI) {
  // Setup
  std::string pubkey = CreateAndStoreTestKey("ui-key", "password");
  
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      pubkey,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  base::RunLoop unlock_loop;
  nostr_service()->UnlockKey(
      pubkey, "password",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Inject test helper for simulating permission response
  EXPECT_TRUE(content::ExecuteScript(
      web_contents(),
      R"(
        window.__tungsten_permission_handler = (resolve, reject) => {
          // In real implementation, this would show UI
          // For testing, we'll simulate user response
          if (window.__tungsten_test_permission_response) {
            resolve();
          } else {
            reject(new Error('User denied permission'));
          }
        };
      )"));
  
  // Simulate allowing permission
  test::SimulatePermissionPromptResponse(web_contents(), true);
  GrantNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey()"
      "  .then(() => window.domAutomationController.send('allowed'))"
      "  .catch(() => window.domAutomationController.send('denied'));",
      &result));
  EXPECT_EQ("allowed", result);
  
  // Simulate denying permission on a different method
  DenyNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  test::SimulatePermissionPromptResponse(web_contents(), false);
  
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.signEvent({ kind: 1, content: 'test' })"
      "  .then(() => window.domAutomationController.send('allowed'))"
      "  .catch(() => window.domAutomationController.send('denied'));",
      &result));
  EXPECT_EQ("denied", result);
}

// Test permission with no active key
IN_PROC_BROWSER_TEST_F(NostrPermissionFlowBrowserTest, NoActiveKeyPermission) {
  // Don't set any active key
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Even with permission granted, operations should fail without active key
  GrantNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey()"
      "  .then(() => window.domAutomationController.send('success'))"
      "  .catch((e) => window.domAutomationController.send('error: ' + e.message));",
      &result));
  EXPECT_TRUE(base::StartsWith(result, "error:"));
}

// Test permission with locked key
IN_PROC_BROWSER_TEST_F(NostrPermissionFlowBrowserTest, LockedKeyPermission) {
  // Setup key but don't unlock it
  std::string pubkey = CreateAndStoreTestKey("locked-key", "password");
  
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      pubkey,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  // Don't unlock the key
  
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  GrantNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  // getPublicKey might work with locked key (just returns pubkey)
  // but signEvent should fail
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.signEvent({ kind: 1, content: 'test' })"
      "  .then(() => window.domAutomationController.send('success'))"
      "  .catch(() => window.domAutomationController.send('error'));",
      &result));
  EXPECT_EQ("error", result);
}

}  // namespace nostr