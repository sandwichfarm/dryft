// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/integration_tests/nostr_integration_test_base.h"

#include "base/test/bind.h"
#include "chrome/browser/nostr/integration_tests/test_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace nostr {

class NostrE2EBrowserTest : public InProcessBrowserTest,
                           public NostrIntegrationTestBase {
 public:
  NostrE2EBrowserTest() = default;
  ~NostrE2EBrowserTest() override = default;

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

// Full end-to-end test: Create key, grant permission, sign event, publish to relay
IN_PROC_BROWSER_TEST_F(NostrE2EBrowserTest, CompleteNostrWorkflow) {
  // Step 1: Create a new key
  std::string pubkey = CreateAndStoreTestKey("e2e-test-key", "secure-password");
  ASSERT_FALSE(pubkey.empty());
  
  // Step 2: Set as active key
  base::RunLoop set_active_loop;
  nostr_service()->SetActiveKey(
      pubkey,
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        set_active_loop.Quit();
      }));
  set_active_loop.Run();
  
  // Step 3: Navigate to test page
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Step 4: Grant NIP-07 permission
  GrantNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  // Step 5: Unlock the key
  base::RunLoop unlock_loop;
  nostr_service()->UnlockKey(
      pubkey, "secure-password",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  // Step 6: Wait for local relay to be ready
  WaitForLocalRelayReady();
  
  // Step 7: Verify window.nostr is available
  EXPECT_TRUE(test::CheckNostrAPIAvailable(web_contents()));
  
  // Step 8: Get public key via JavaScript
  std::string js_pubkey;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.nostr.getPublicKey()"
      "  .then(pk => window.domAutomationController.send(pk));",
      &js_pubkey));
  EXPECT_EQ(pubkey, js_pubkey);
  
  // Step 9: Create and sign an event
  std::string create_and_sign_script = R"(
    (async () => {
      const event = {
        kind: 1,
        content: 'Hello from E2E test!',
        created_at: Math.floor(Date.now() / 1000),
        tags: [['test', 'e2e']]
      };
      
      try {
        const signed = await window.nostr.signEvent(event);
        return JSON.stringify(signed);
      } catch (e) {
        return 'error: ' + e.message;
      }
    })().then(result => window.domAutomationController.send(result));
  )";
  
  std::string signed_event_json;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), create_and_sign_script, &signed_event_json));
  EXPECT_FALSE(base::StartsWith(signed_event_json, "error:"));
  
  // Step 10: Publish to local relay
  SendTestEventToLocalRelay(signed_event_json);
  
  // Step 11: Query the event back
  auto events = QueryEventsFromLocalRelay(
      test::CreateTestFilter({1}, {pubkey}));
  EXPECT_GE(events.size(), 1u);
  
  // Step 12: Verify the event content
  bool found_our_event = false;
  for (const auto& event_json : events) {
    auto event = base::JSONReader::Read(event_json);
    if (event && event->is_dict()) {
      const auto* content = event->GetDict().FindString("content");
      if (content && *content == "Hello from E2E test!") {
        found_our_event = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_our_event);
}

// Test complete Nsite publishing workflow
IN_PROC_BROWSER_TEST_F(NostrE2EBrowserTest, NsitePublishingWorkflow) {
  // Setup key and permissions
  std::string pubkey = CreateAndStoreTestKey("nsite-author", "password");
  
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
      pubkey, "password",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  WaitForLocalRelayReady();
  
  // Create and publish Nsite content
  std::string publish_nsite_script = R"(
    (async () => {
      const nsiteEvent = {
        kind: 34128,  // Correct Nsite kind
        content: '<h1>My Nsite</h1><p>Welcome to my decentralized site!</p>',
        created_at: Math.floor(Date.now() / 1000),
        tags: [
          ['title', 'My Test Nsite'],
          ['summary', 'A test Nsite created in E2E test'],
          ['theme', 'default'],
          ['path', '/index.html']
        ]
      };
      
      try {
        const signed = await window.nostr.signEvent(nsiteEvent);
        // Publish to local relay
        const ws = new WebSocket(window.nostr.relay.url);
        return new Promise((resolve) => {
          ws.onopen = () => {
            ws.send(JSON.stringify(['EVENT', signed]));
            ws.close();
            resolve(JSON.stringify(signed));
          };
          ws.onerror = () => resolve('error: websocket failed');
        });
      } catch (e) {
        return 'error: ' + e.message;
      }
    })().then(result => window.domAutomationController.send(result));
  )";
  
  std::string nsite_event_json;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), publish_nsite_script, &nsite_event_json));
  EXPECT_FALSE(base::StartsWith(nsite_event_json, "error:"));
  
  // Query Nsite events
  auto nsite_events = QueryEventsFromLocalRelay(
      test::CreateTestFilter({34128}, {pubkey}));
  EXPECT_GE(nsite_events.size(), 1u);
}

// Test multi-tab Nostr interaction
IN_PROC_BROWSER_TEST_F(NostrE2EBrowserTest, MultiTabNostrInteraction) {
  // Setup
  std::string pubkey = CreateAndStoreTestKey("multi-tab-key", "password");
  
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
  
  // Open first tab
  GURL test_url1 = embedded_test_server()->GetURL("a.com", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url1));
  GrantNIP07Permission(test_url1.DeprecatedGetOriginAsURL());
  
  // Open second tab
  GURL test_url2 = embedded_test_server()->GetURL("b.com", "/empty.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      test_url2,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  GrantNIP07Permission(test_url2.DeprecatedGetOriginAsURL());
  
  // Both tabs should be able to use window.nostr
  browser()->tab_strip_model()->ActivateTabAt(0);
  content::WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();
  
  std::string tab1_pubkey;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab1,
      "window.nostr.getPublicKey()"
      "  .then(pk => window.domAutomationController.send(pk));",
      &tab1_pubkey));
  EXPECT_EQ(pubkey, tab1_pubkey);
  
  browser()->tab_strip_model()->ActivateTabAt(1);
  content::WebContents* tab2 = browser()->tab_strip_model()->GetActiveWebContents();
  
  std::string tab2_pubkey;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab2,
      "window.nostr.getPublicKey()"
      "  .then(pk => window.domAutomationController.send(pk));",
      &tab2_pubkey));
  EXPECT_EQ(pubkey, tab2_pubkey);
}

// Test relay subscription and real-time events
IN_PROC_BROWSER_TEST_F(NostrE2EBrowserTest, RelaySubscriptionWorkflow) {
  // Setup
  std::string pubkey = CreateAndStoreTestKey("subscription-key", "password");
  
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
      pubkey, "password",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  WaitForLocalRelayReady();
  
  // Set up WebSocket subscription
  std::string subscription_script = R"(
    window.receivedEvents = [];
    window.ws = new WebSocket(window.nostr.relay.url);
    
    window.ws.onmessage = (msg) => {
      const data = JSON.parse(msg.data);
      if (data[0] === 'EVENT') {
        window.receivedEvents.push(data[2]);
      }
    };
    
    window.ws.onopen = () => {
      // Subscribe to all kind 1 events
      window.ws.send(JSON.stringify([
        'REQ',
        'test-sub',
        { kinds: [1] }
      ]));
      window.domAutomationController.send('subscribed');
    };
    
    window.ws.onerror = () => {
      window.domAutomationController.send('error');
    };
  )";
  
  std::string sub_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), subscription_script, &sub_result));
  EXPECT_EQ("subscribed", sub_result);
  
  // Give subscription time to establish
  base::PlatformThread::Sleep(base::Milliseconds(100));
  
  // Publish an event
  std::string event_json = test::CreateTestEvent(1, "Real-time test event");
  SendTestEventToLocalRelay(event_json);
  
  // Give event time to propagate
  base::PlatformThread::Sleep(base::Milliseconds(100));
  
  // Check if event was received
  int received_count = 0;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      web_contents(),
      "window.domAutomationController.send(window.receivedEvents.length);",
      &received_count));
  EXPECT_GT(received_count, 0);
  
  // Clean up WebSocket
  EXPECT_TRUE(content::ExecuteScript(web_contents(), "window.ws.close();"));
}

// Test library loading and usage
IN_PROC_BROWSER_TEST_F(NostrE2EBrowserTest, LibraryLoadingAndUsage) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Test loading NDK library
  std::string ndk_test_script = R"(
    (async () => {
      try {
        const NDK = await import(window.nostr.libs.ndk);
        // Check if NDK has expected exports
        return typeof NDK.NDK === 'function' ? 'success' : 'invalid-export';
      } catch (e) {
        return 'error: ' + e.message;
      }
    })().then(result => window.domAutomationController.send(result));
  )";
  
  std::string ndk_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), ndk_test_script, &ndk_result));
  EXPECT_EQ("success", ndk_result);
  
  // Test loading nostr-tools
  std::string tools_test_script = R"(
    (async () => {
      try {
        const tools = await import(window.nostr.libs['nostr-tools']);
        // Check if nostr-tools has expected functions
        return typeof tools.generatePrivateKey === 'function' ? 'success' : 'invalid-export';
      } catch (e) {
        return 'error: ' + e.message;
      }
    })().then(result => window.domAutomationController.send(result));
  )";
  
  std::string tools_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), tools_test_script, &tools_result));
  EXPECT_EQ("success", tools_result);
}

// Test performance of Nostr operations
IN_PROC_BROWSER_TEST_F(NostrE2EBrowserTest, PerformanceBenchmark) {
  // Setup
  std::string pubkey = CreateAndStoreTestKey("perf-key", "password");
  
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
      pubkey, "password",
      base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        unlock_loop.Quit();
      }));
  unlock_loop.Run();
  
  // Measure getPublicKey performance
  auto pubkey_metrics = test::MeasureNostrOperation(
      web_contents(),
      "window.nostr.getPublicKey()");
  
  // Should complete within 20ms as per spec
  EXPECT_LT(pubkey_metrics.operation_time_ms, 20.0);
  
  // Measure signEvent performance
  std::string sign_operation = R"(
    window.nostr.signEvent({
      kind: 1,
      content: 'Performance test',
      created_at: Math.floor(Date.now() / 1000),
      tags: []
    })
  )";
  
  auto sign_metrics = test::MeasureNostrOperation(
      web_contents(), sign_operation);
  
  // Should complete within 20ms as per spec
  EXPECT_LT(sign_metrics.operation_time_ms, 20.0);
  
  // Memory overhead should be reasonable
  EXPECT_LT(sign_metrics.memory_used_bytes, 1024 * 1024);  // Less than 1MB
}

}  // namespace nostr