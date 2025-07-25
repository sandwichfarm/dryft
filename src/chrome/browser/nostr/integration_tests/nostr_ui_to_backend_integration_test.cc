// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/integration_tests/nostr_integration_test_base.h"

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "chrome/browser/nostr/integration_tests/test_helpers.h"
#include "chrome/browser/nostr/nostr_permission_manager.h"
#include "chrome/browser/nostr/nostr_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace nostr {

class NostrUIToBackendIntegrationTest : public InProcessBrowserTest,
                                       public NostrIntegrationTestBase {
 public:
  NostrUIToBackendIntegrationTest() = default;
  ~NostrUIToBackendIntegrationTest() override = default;

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

// Test key creation and retrieval flow
IN_PROC_BROWSER_TEST_F(NostrUIToBackendIntegrationTest, KeyCreationFlow) {
  // Create a test key
  std::string pubkey = CreateAndStoreTestKey("test-key", "password123");
  EXPECT_FALSE(pubkey.empty());
  
  // Verify key is stored
  std::vector<NostrService::KeyInfo> keys;
  base::RunLoop run_loop;
  nostr_service()->GetStoredKeys(
      base::BindLambdaForTesting([&](std::vector<NostrService::KeyInfo> result) {
        keys = std::move(result);
        run_loop.Quit();
      }));
  run_loop.Run();
  
  EXPECT_EQ(1u, keys.size());
  EXPECT_EQ("test-key", keys[0].name);
  EXPECT_EQ(pubkey, keys[0].public_key);
  EXPECT_FALSE(keys[0].is_unlocked);  // Should be locked by default
}

// Test permission grant and API access flow
IN_PROC_BROWSER_TEST_F(NostrUIToBackendIntegrationTest, PermissionAndAPIFlow) {
  // Create and set active key
  std::string pubkey = CreateAndStoreTestKey("test-key", "password123");
  
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      pubkey,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  // Navigate to test page
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Grant permission for this origin
  GrantNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  // Unlock the key
  base::RunLoop unlock_loop;
  nostr_service()->UnlockKey(
      pubkey, "password123",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  // Now getPublicKey should work
  std::string js_pubkey;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey()"
      "  .then(pk => window.domAutomationController.send(pk))"
      "  .catch(e => window.domAutomationController.send('error'));",
      &js_pubkey));
  EXPECT_EQ(pubkey, js_pubkey);
}

// Test signing event flow
IN_PROC_BROWSER_TEST_F(NostrUIToBackendIntegrationTest, SignEventFlow) {
  // Setup: Create key, grant permission, navigate to page
  std::string pubkey = CreateAndStoreTestKey("signing-key", "password123");
  
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      pubkey,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  GrantNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  base::RunLoop unlock_loop;
  nostr_service()->UnlockKey(
      pubkey, "password123",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  // Create a test event
  std::string event_json = test::CreateTestEvent(1, "Hello Nostr!");
  
  // Sign the event
  std::string script = base::StringPrintf(
      "window.nostr.signEvent(%s)"
      "  .then(signed => window.domAutomationController.send("
      "    JSON.stringify(signed)"
      "  ))"
      "  .catch(e => window.domAutomationController.send('error'));",
      event_json.c_str());
  
  std::string signed_event_json;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), script, &signed_event_json));
  EXPECT_NE("error", signed_event_json);
  
  // Verify the signed event has required fields
  auto signed_event = base::JSONReader::Read(signed_event_json);
  ASSERT_TRUE(signed_event && signed_event->is_dict());
  
  const auto& dict = signed_event->GetDict();
  EXPECT_TRUE(dict.FindString("id"));
  EXPECT_TRUE(dict.FindString("pubkey"));
  EXPECT_TRUE(dict.FindString("sig"));
  EXPECT_EQ(pubkey, *dict.FindString("pubkey"));
}

// Test relay management flow
IN_PROC_BROWSER_TEST_F(NostrUIToBackendIntegrationTest, RelayManagementFlow) {
  // Setup permission
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  GrantNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  // Get relays (should return default/configured relays)
  std::string relays_json;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getRelays()"
      "  .then(relays => window.domAutomationController.send("
      "    JSON.stringify(relays)"
      "  ))"
      "  .catch(e => window.domAutomationController.send('error'));",
      &relays_json));
  EXPECT_NE("error", relays_json);
  
  auto relays = base::JSONReader::Read(relays_json);
  ASSERT_TRUE(relays && relays->is_dict());
  
  // Should contain at least the local relay
  const auto& relay_dict = relays->GetDict();
  EXPECT_TRUE(relay_dict.contains("ws://localhost:4869"));
}

// Test NIP-04 encryption/decryption flow
IN_PROC_BROWSER_TEST_F(NostrUIToBackendIntegrationTest, NIP04EncryptionFlow) {
  // Setup keys and permissions
  std::string our_pubkey = CreateAndStoreTestKey("our-key", "password123");
  test::TestKeyPair peer = test::GenerateTestKeyPair();
  
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      our_pubkey,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  GrantNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  base::RunLoop unlock_loop;
  nostr_service()->UnlockKey(
      our_pubkey, "password123",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  // Test encryption
  std::string plaintext = "Secret message";
  std::string encrypt_script = base::StringPrintf(
      "window.nostr.nip04.encrypt('%s', '%s')"
      "  .then(cipher => window.domAutomationController.send(cipher))"
      "  .catch(e => window.domAutomationController.send('error'));",
      peer.public_key.c_str(), plaintext.c_str());
  
  std::string ciphertext;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), encrypt_script, &ciphertext));
  EXPECT_NE("error", ciphertext);
  EXPECT_NE(plaintext, ciphertext);  // Should be encrypted
}

// Test permission denial flow
IN_PROC_BROWSER_TEST_F(NostrUIToBackendIntegrationTest, PermissionDenialFlow) {
  // Create key but don't grant permission
  std::string pubkey = CreateAndStoreTestKey("denied-key", "password123");
  
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      pubkey,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Explicitly deny permission
  DenyNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  // API calls should fail
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey()"
      "  .then(() => window.domAutomationController.send('success'))"
      "  .catch(() => window.domAutomationController.send('denied'));",
      &result));
  EXPECT_EQ("denied", result);
}

// Test local relay connection status
IN_PROC_BROWSER_TEST_F(NostrUIToBackendIntegrationTest, LocalRelayConnectionStatus) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Wait for local relay to be ready
  WaitForLocalRelayReady();
  
  // Check connection status via JavaScript
  bool connected = test::WaitForLocalRelayConnection(web_contents());
  EXPECT_TRUE(connected);
  
  // Verify we can query the relay URL
  std::string relay_url;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(window.nostr.relay.url);",
      &relay_url));
  EXPECT_EQ(local_relay_service()->GetRelayURL(), relay_url);
}

// Test multiple key management
IN_PROC_BROWSER_TEST_F(NostrUIToBackendIntegrationTest, MultipleKeyManagement) {
  // Create multiple keys
  std::string key1 = CreateAndStoreTestKey("key-1", "password1");
  std::string key2 = CreateAndStoreTestKey("key-2", "password2");
  std::string key3 = CreateAndStoreTestKey("key-3", "password3");
  
  // Get all stored keys
  std::vector<NostrService::KeyInfo> keys;
  base::RunLoop run_loop;
  nostr_service()->GetStoredKeys(
      base::BindLambdaForTesting([&](std::vector<NostrService::KeyInfo> result) {
        keys = std::move(result);
        run_loop.Quit();
      }));
  run_loop.Run();
  
  EXPECT_EQ(3u, keys.size());
  
  // Switch active key
  base::RunLoop switch_loop;
  nostr_service()->SetActiveKey(
      key2,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        switch_loop.Quit();
      }));
  switch_loop.Run();
  
  EXPECT_EQ(key2, nostr_service()->GetActivePublicKey());
  
  // Delete a key
  base::RunLoop delete_loop;
  nostr_service()->DeleteKey(
      key1,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        delete_loop.Quit();
      }));
  delete_loop.Run();
  
  // Verify deletion
  base::RunLoop verify_loop;
  nostr_service()->GetStoredKeys(
      base::BindLambdaForTesting([&](std::vector<NostrService::KeyInfo> result) {
        keys = std::move(result);
        verify_loop.Quit();
      }));
  verify_loop.Run();
  
  EXPECT_EQ(2u, keys.size());
}

}  // namespace nostr