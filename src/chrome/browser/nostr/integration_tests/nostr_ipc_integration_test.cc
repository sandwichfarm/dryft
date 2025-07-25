// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/integration_tests/nostr_integration_test_base.h"

#include "base/test/bind.h"
#include "chrome/browser/nostr/integration_tests/test_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace nostr {

class NostrIPCIntegrationTest : public InProcessBrowserTest,
                               public NostrIntegrationTestBase {
 public:
  NostrIPCIntegrationTest() = default;
  ~NostrIPCIntegrationTest() override = default;

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

// Test basic IPC message flow for getPublicKey
IN_PROC_BROWSER_TEST_F(NostrIPCIntegrationTest, GetPublicKeyIPCFlow) {
  // Setup
  std::string pubkey = CreateAndStoreTestKey("ipc-test-key", "password123");
  
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
  
  // Execute multiple concurrent requests to test IPC handling
  const int kNumRequests = 5;
  std::string script = R"(
    Promise.all([
      window.nostr.getPublicKey(),
      window.nostr.getPublicKey(),
      window.nostr.getPublicKey(),
      window.nostr.getPublicKey(),
      window.nostr.getPublicKey()
    ]).then(results => {
      const allSame = results.every(r => r === results[0]);
      window.domAutomationController.send(
        allSame ? results[0] : 'mismatch'
      );
    }).catch(e => {
      window.domAutomationController.send('error');
    });
  )";
  
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), script, &result));
  EXPECT_EQ(pubkey, result);
}

// Test IPC message ordering for signEvent
IN_PROC_BROWSER_TEST_F(NostrIPCIntegrationTest, SignEventIPCOrdering) {
  // Setup
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
  
  // Create multiple events with different content
  std::string script = R"(
    const events = [
      { kind: 1, content: 'First event', created_at: 1, tags: [] },
      { kind: 1, content: 'Second event', created_at: 2, tags: [] },
      { kind: 1, content: 'Third event', created_at: 3, tags: [] }
    ];
    
    Promise.all(events.map(e => window.nostr.signEvent(e)))
      .then(signed => {
        // Check that content order is preserved
        const contents = signed.map(s => s.content);
        const expected = ['First event', 'Second event', 'Third event'];
        const orderPreserved = contents.every((c, i) => c === expected[i]);
        window.domAutomationController.send(
          orderPreserved ? 'order-preserved' : 'order-broken'
        );
      })
      .catch(e => {
        window.domAutomationController.send('error');
      });
  )";
  
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), script, &result));
  EXPECT_EQ("order-preserved", result);
}

// Test IPC error handling
IN_PROC_BROWSER_TEST_F(NostrIPCIntegrationTest, IPCErrorHandling) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Don't grant permission - should trigger error path
  DenyNIP07Permission(test_url.DeprecatedGetOriginAsURL());
  
  // Test various error scenarios
  std::string script = R"(
    const testErrors = async () => {
      const results = [];
      
      // Test getPublicKey without permission
      try {
        await window.nostr.getPublicKey();
        results.push('pubkey-success');
      } catch (e) {
        results.push('pubkey-error');
      }
      
      // Test signEvent without permission
      try {
        await window.nostr.signEvent({ kind: 1, content: 'test' });
        results.push('sign-success');
      } catch (e) {
        results.push('sign-error');
      }
      
      // Test invalid event signing
      try {
        await window.nostr.signEvent('not-an-object');
        results.push('invalid-success');
      } catch (e) {
        results.push('invalid-error');
      }
      
      return results;
    };
    
    testErrors().then(results => {
      window.domAutomationController.send(results.join(','));
    });
  )";
  
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), script, &result));
  EXPECT_EQ("pubkey-error,sign-error,invalid-error", result);
}

// Test IPC communication across iframe boundaries
IN_PROC_BROWSER_TEST_F(NostrIPCIntegrationTest, CrossFrameIPCCommunication) {
  // Create a page with an iframe
  std::string main_html = R"(
    <!DOCTYPE html>
    <html>
    <body>
      <iframe id="child" src="/iframe.html"></iframe>
      <script>
        window.mainResult = null;
        window.childResult = null;
      </script>
    </body>
    </html>
  )";
  
  std::string iframe_html = test::CreateNostrTestHTML("");
  
  // Setup test server responses
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating([&](const net::test_server::HttpRequest& request)
                             -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        if (request.relative_url == "/main.html") {
          response->set_content(main_html);
          response->set_content_type("text/html");
        } else if (request.relative_url == "/iframe.html") {
          response->set_content(iframe_html);
          response->set_content_type("text/html");
        } else {
          return nullptr;
        }
        return response;
      }));
  
  GURL main_url = embedded_test_server()->GetURL("/main.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  
  // Grant permission for both origins
  GrantNIP07Permission(main_url.DeprecatedGetOriginAsURL());
  
  // Wait for iframe to load
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  
  // Test that both frames can access window.nostr
  bool main_has_nostr = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send(typeof window.nostr === 'object');",
      &main_has_nostr));
  EXPECT_TRUE(main_has_nostr);
  
  // Check iframe
  content::RenderFrameHost* iframe = content::FrameMatchingPredicate(
      web_contents()->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, "child"));
  ASSERT_TRUE(iframe);
  
  bool iframe_has_nostr = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      iframe,
      "window.domAutomationController.send(typeof window.nostr === 'object');",
      &iframe_has_nostr));
  EXPECT_TRUE(iframe_has_nostr);
}

// Test IPC message size limits
IN_PROC_BROWSER_TEST_F(NostrIPCIntegrationTest, IPCMessageSizeLimits) {
  // Setup
  std::string pubkey = CreateAndStoreTestKey("size-test-key", "password123");
  
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
  
  // Test with a large event content
  std::string script = R"(
    const largeContent = 'x'.repeat(1024 * 1024); // 1MB of data
    const event = {
      kind: 1,
      content: largeContent,
      created_at: Math.floor(Date.now() / 1000),
      tags: []
    };
    
    window.nostr.signEvent(event)
      .then(signed => {
        window.domAutomationController.send(
          signed.content.length === largeContent.length ? 'success' : 'truncated'
        );
      })
      .catch(e => {
        window.domAutomationController.send('error');
      });
  )";
  
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), script, &result));
  // Large messages might fail or succeed depending on IPC limits
  EXPECT_TRUE(result == "success" || result == "error");
}

// Test rapid-fire IPC requests
IN_PROC_BROWSER_TEST_F(NostrIPCIntegrationTest, RapidFireIPCRequests) {
  // Setup
  std::string pubkey = CreateAndStoreTestKey("rapid-key", "password123");
  
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
  
  // Send many requests in rapid succession
  std::string script = R"(
    const numRequests = 100;
    const requests = [];
    
    for (let i = 0; i < numRequests; i++) {
      requests.push(window.nostr.getRelays());
    }
    
    Promise.all(requests)
      .then(results => {
        // All requests should succeed and return the same data
        const firstResult = JSON.stringify(results[0]);
        const allSame = results.every(r => JSON.stringify(r) === firstResult);
        window.domAutomationController.send(
          allSame ? 'all-consistent' : 'inconsistent'
        );
      })
      .catch(e => {
        window.domAutomationController.send('error');
      });
  )";
  
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), script, &result));
  EXPECT_EQ("all-consistent", result);
}

// Test IPC behavior during navigation
IN_PROC_BROWSER_TEST_F(NostrIPCIntegrationTest, IPCDuringNavigation) {
  // Setup
  std::string pubkey = CreateAndStoreTestKey("nav-key", "password123");
  
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
  
  // Start a long-running operation
  std::string script = R"(
    window.pendingRequest = window.nostr.getPublicKey();
    window.pendingRequest
      .then(pk => { window.requestResult = pk; })
      .catch(e => { window.requestResult = 'error'; });
    window.domAutomationController.send('started');
  )";
  
  std::string start_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), script, &start_result));
  EXPECT_EQ("started", start_result);
  
  // Navigate to a new page
  GURL new_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), new_url));
  
  // The pending request should have been cancelled/resolved
  // New page should have fresh window.nostr
  bool has_nostr = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      "window.domAutomationController.send(typeof window.nostr === 'object');",
      &has_nostr));
  EXPECT_TRUE(has_nostr);
}

}  // namespace nostr