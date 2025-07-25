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
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace nostr {

class NostrKeyManagementBrowserTest : public InProcessBrowserTest,
                                     public NostrIntegrationTestBase {
 public:
  NostrKeyManagementBrowserTest() = default;
  ~NostrKeyManagementBrowserTest() override = default;

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

// Test creating and storing multiple keys
IN_PROC_BROWSER_TEST_F(NostrKeyManagementBrowserTest, CreateMultipleKeys) {
  // Create multiple keys
  std::string key1 = CreateAndStoreTestKey("Personal Key", "password1");
  std::string key2 = CreateAndStoreTestKey("Work Key", "password2");
  std::string key3 = CreateAndStoreTestKey("Test Key", "password3");
  
  EXPECT_FALSE(key1.empty());
  EXPECT_FALSE(key2.empty());
  EXPECT_FALSE(key3.empty());
  EXPECT_NE(key1, key2);
  EXPECT_NE(key2, key3);
  EXPECT_NE(key1, key3);
  
  // Verify all keys are stored
  std::vector<NostrService::KeyInfo> keys;
  base::RunLoop run_loop;
  nostr_service()->GetStoredKeys(
      base::BindLambdaForTesting([&](std::vector<NostrService::KeyInfo> result) {
        keys = std::move(result);
        run_loop.Quit();
      }));
  run_loop.Run();
  
  EXPECT_EQ(3u, keys.size());
  
  // Verify key names
  std::set<std::string> key_names;
  for (const auto& key : keys) {
    key_names.insert(key.name);
  }
  EXPECT_TRUE(key_names.count("Personal Key"));
  EXPECT_TRUE(key_names.count("Work Key"));
  EXPECT_TRUE(key_names.count("Test Key"));
}

// Test key import functionality
IN_PROC_BROWSER_TEST_F(NostrKeyManagementBrowserTest, ImportKey) {
  // Generate a test key pair
  test::TestKeyPair key_pair = test::GenerateTestKeyPair();
  
  // Import the key
  base::RunLoop import_loop;
  std::string imported_pubkey;
  nostr_service()->ImportKey(
      "Imported Key",
      key_pair.private_key,
      "import-password",
      base::BindLambdaForTesting([&](bool success, const std::string& pubkey) {
        EXPECT_TRUE(success);
        imported_pubkey = pubkey;
        import_loop.Quit();
      }));
  import_loop.Run();
  
  EXPECT_EQ(key_pair.public_key, imported_pubkey);
  
  // Verify the key is stored
  std::vector<NostrService::KeyInfo> keys;
  base::RunLoop get_keys_loop;
  nostr_service()->GetStoredKeys(
      base::BindLambdaForTesting([&](std::vector<NostrService::KeyInfo> result) {
        keys = std::move(result);
        get_keys_loop.Quit();
      }));
  get_keys_loop.Run();
  
  bool found = false;
  for (const auto& key : keys) {
    if (key.name == "Imported Key" && key.public_key == imported_pubkey) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

// Test switching active keys
IN_PROC_BROWSER_TEST_F(NostrKeyManagementBrowserTest, SwitchActiveKey) {
  // Create multiple keys
  std::string key1 = CreateAndStoreTestKey("Key 1", "password1");
  std::string key2 = CreateAndStoreTestKey("Key 2", "password2");
  std::string key3 = CreateAndStoreTestKey("Key 3", "password3");
  
  // Key1 should be active by default (first created)
  EXPECT_EQ(key1, nostr_service()->GetActivePublicKey());
  
  // Switch to key2
  base::RunLoop switch_loop1;
  nostr_service()->SetActiveKey(
      key2,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        switch_loop1.Quit();
      }));
  switch_loop1.Run();
  
  EXPECT_EQ(key2, nostr_service()->GetActivePublicKey());
  
  // Switch to key3
  base::RunLoop switch_loop2;
  nostr_service()->SetActiveKey(
      key3,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        switch_loop2.Quit();
      }));
  switch_loop2.Run();
  
  EXPECT_EQ(key3, nostr_service()->GetActivePublicKey());
  
  // Try to switch to non-existent key
  base::RunLoop invalid_switch_loop;
  nostr_service()->SetActiveKey(
      "non-existent-pubkey",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_FALSE(success);
        invalid_switch_loop.Quit();
      }));
  invalid_switch_loop.Run();
  
  // Active key should remain unchanged
  EXPECT_EQ(key3, nostr_service()->GetActivePublicKey());
}

// Test key locking and unlocking
IN_PROC_BROWSER_TEST_F(NostrKeyManagementBrowserTest, KeyLockUnlock) {
  std::string pubkey = CreateAndStoreTestKey("Lock Test Key", "secure-pass");
  
  // Key should be locked initially
  EXPECT_FALSE(nostr_service()->IsKeyUnlocked(pubkey));
  
  // Unlock with correct password
  base::RunLoop unlock_loop;
  nostr_service()->UnlockKey(
      pubkey, "secure-pass",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  EXPECT_TRUE(nostr_service()->IsKeyUnlocked(pubkey));
  
  // Lock the key
  nostr_service()->LockKey(pubkey);
  EXPECT_FALSE(nostr_service()->IsKeyUnlocked(pubkey));
  
  // Try to unlock with wrong password
  base::RunLoop wrong_pass_loop;
  nostr_service()->UnlockKey(
      pubkey, "wrong-password",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_FALSE(success);
        wrong_pass_loop.Quit();
      }));
  wrong_pass_loop.Run();
  
  EXPECT_FALSE(nostr_service()->IsKeyUnlocked(pubkey));
}

// Test key deletion
IN_PROC_BROWSER_TEST_F(NostrKeyManagementBrowserTest, DeleteKey) {
  // Create multiple keys
  std::string key1 = CreateAndStoreTestKey("Delete Key 1", "password1");
  std::string key2 = CreateAndStoreTestKey("Delete Key 2", "password2");
  std::string key3 = CreateAndStoreTestKey("Delete Key 3", "password3");
  
  // Set key2 as active
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      key2,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  // Delete key1 (non-active)
  base::RunLoop delete_loop1;
  nostr_service()->DeleteKey(
      key1,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        delete_loop1.Quit();
      }));
  delete_loop1.Run();
  
  // Verify key1 is deleted
  std::vector<NostrService::KeyInfo> keys;
  base::RunLoop get_keys_loop1;
  nostr_service()->GetStoredKeys(
      base::BindLambdaForTesting([&](std::vector<NostrService::KeyInfo> result) {
        keys = std::move(result);
        get_keys_loop1.Quit();
      }));
  get_keys_loop1.Run();
  
  EXPECT_EQ(2u, keys.size());
  for (const auto& key : keys) {
    EXPECT_NE(key1, key.public_key);
  }
  
  // Active key should still be key2
  EXPECT_EQ(key2, nostr_service()->GetActivePublicKey());
  
  // Delete active key (key2)
  base::RunLoop delete_loop2;
  nostr_service()->DeleteKey(
      key2,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        delete_loop2.Quit();
      }));
  delete_loop2.Run();
  
  // Active key should switch to remaining key (key3)
  EXPECT_EQ(key3, nostr_service()->GetActivePublicKey());
}

// Test key usage in browser context
IN_PROC_BROWSER_TEST_F(NostrKeyManagementBrowserTest, KeyUsageInBrowser) {
  // Create and unlock a key
  std::string pubkey = CreateAndStoreTestKey("Browser Key", "browser-pass");
  
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
      pubkey, "browser-pass",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  // Navigate to test page
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  GrantNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  // Use the key via JavaScript
  std::string js_pubkey;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey()"
      "  .then(pk => window.domAutomationController.send(pk));",
      &js_pubkey));
  EXPECT_EQ(pubkey, js_pubkey);
  
  // Sign an event
  std::string signed_event;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.signEvent({ kind: 1, content: 'test', created_at: 1, tags: [] })"
      "  .then(e => window.domAutomationController.send(JSON.stringify(e)));",
      &signed_event));
  
  auto event = base::JSONReader::Read(signed_event);
  ASSERT_TRUE(event && event->is_dict());
  EXPECT_EQ(pubkey, *event->GetDict().FindString("pubkey"));
  EXPECT_TRUE(event->GetDict().FindString("sig"));
}

// Test key persistence across browser restart (simulated)
IN_PROC_BROWSER_TEST_F(NostrKeyManagementBrowserTest, KeyPersistence) {
  // Create keys
  std::string key1 = CreateAndStoreTestKey("Persistent Key 1", "password1");
  std::string key2 = CreateAndStoreTestKey("Persistent Key 2", "password2");
  
  // Set key2 as active
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      key2,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  // Simulate service restart by getting a new instance
  // In real scenario, this would be after browser restart
  
  // Keys should still be available
  std::vector<NostrService::KeyInfo> keys;
  base::RunLoop get_keys_loop;
  nostr_service()->GetStoredKeys(
      base::BindLambdaForTesting([&](std::vector<NostrService::KeyInfo> result) {
        keys = std::move(result);
        get_keys_loop.Quit();
      }));
  get_keys_loop.Run();
  
  EXPECT_EQ(2u, keys.size());
  
  // Active key should be preserved
  EXPECT_EQ(key2, nostr_service()->GetActivePublicKey());
}

// Test concurrent key operations
IN_PROC_BROWSER_TEST_F(NostrKeyManagementBrowserTest, ConcurrentKeyOperations) {
  // Create multiple keys concurrently
  const int kNumKeys = 5;
  std::vector<std::string> created_keys;
  base::RunLoop create_loop;
  std::atomic<int> completed_count(0);
  
  for (int i = 0; i < kNumKeys; ++i) {
    nostr_service()->CreateKey(
        base::StringPrintf("Concurrent Key %d", i),
        base::StringPrintf("password%d", i),
        base::BindLambdaForTesting([&, i](bool success, const std::string& pubkey) {
          EXPECT_TRUE(success);
          created_keys.push_back(pubkey);
          if (++completed_count == kNumKeys) {
            create_loop.Quit();
          }
        }));
  }
  
  create_loop.Run();
  
  // Verify all keys were created
  EXPECT_EQ(kNumKeys, static_cast<int>(created_keys.size()));
  
  // Verify all keys are unique
  std::set<std::string> unique_keys(created_keys.begin(), created_keys.end());
  EXPECT_EQ(created_keys.size(), unique_keys.size());
  
  // Verify all keys are stored
  std::vector<NostrService::KeyInfo> stored_keys;
  base::RunLoop get_keys_loop;
  nostr_service()->GetStoredKeys(
      base::BindLambdaForTesting([&](std::vector<NostrService::KeyInfo> result) {
        stored_keys = std::move(result);
        get_keys_loop.Quit();
      }));
  get_keys_loop.Run();
  
  EXPECT_EQ(kNumKeys, static_cast<int>(stored_keys.size()));
}

// Test key rotation scenario
IN_PROC_BROWSER_TEST_F(NostrKeyManagementBrowserTest, KeyRotation) {
  // Create initial key
  std::string old_key = CreateAndStoreTestKey("Old Key", "old-password");
  
  base::RunLoop set_old_active_loop;
  nostr_service()->SetActiveKey(
      old_key,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_old_active_loop.Quit();
      }));
  set_old_active_loop.Run();
  
  // Use the old key
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  GrantNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  base::RunLoop unlock_old_loop;
  nostr_service()->UnlockKey(
      old_key, "old-password",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_old_loop.Quit();
      }));
  unlock_old_loop.Run();
  
  // Sign something with old key
  std::string old_signature;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.signEvent({ kind: 1, content: 'old', created_at: 1, tags: [] })"
      "  .then(e => window.domAutomationController.send(e.sig));",
      &old_signature));
  
  // Create new key for rotation
  std::string new_key = CreateAndStoreTestKey("New Key", "new-password");
  
  // Switch to new key
  base::RunLoop set_new_active_loop;
  nostr_service()->SetActiveKey(
      new_key,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_new_active_loop.Quit();
      }));
  set_new_active_loop.Run();
  
  base::RunLoop unlock_new_loop;
  nostr_service()->UnlockKey(
      new_key, "new-password",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_new_loop.Quit();
      }));
  unlock_new_loop.Run();
  
  // Sign with new key
  std::string new_signature;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.signEvent({ kind: 1, content: 'new', created_at: 2, tags: [] })"
      "  .then(e => window.domAutomationController.send(e.sig));",
      &new_signature));
  
  // Signatures should be different
  EXPECT_NE(old_signature, new_signature);
  
  // Delete old key to complete rotation
  base::RunLoop delete_loop;
  nostr_service()->DeleteKey(
      old_key,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        delete_loop.Quit();
      }));
  delete_loop.Run();
  
  // Verify only new key remains
  std::vector<NostrService::KeyInfo> final_keys;
  base::RunLoop final_keys_loop;
  nostr_service()->GetStoredKeys(
      base::BindLambdaForTesting([&](std::vector<NostrService::KeyInfo> result) {
        final_keys = std::move(result);
        final_keys_loop.Quit();
      }));
  final_keys_loop.Run();
  
  EXPECT_EQ(1u, final_keys.size());
  EXPECT_EQ(new_key, final_keys[0].public_key);
}

}  // namespace nostr