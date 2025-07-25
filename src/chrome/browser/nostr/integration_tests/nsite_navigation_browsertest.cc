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

class NsiteNavigationBrowserTest : public InProcessBrowserTest,
                                  public NostrIntegrationTestBase {
 public:
  NsiteNavigationBrowserTest() = default;
  ~NsiteNavigationBrowserTest() override = default;

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
  
  std::string PublishNsiteContent(const std::string& title,
                                 const std::string& content,
                                 const std::string& theme = "default") {
    // Create and sign Nsite event
    std::string nsite_json = test::CreateTestNsiteContent(title, content, theme);
    std::string signed_event = test::SignEvent(nsite_json, "test-privkey");
    
    // Publish to local relay
    SendTestEventToLocalRelay(signed_event);
    
    // Extract event ID for navigation
    auto event = base::JSONReader::Read(signed_event);
    if (event && event->is_dict()) {
      const auto* id = event->GetDict().FindString("id");
      if (id) {
        return *id;
      }
    }
    return "";
  }
};

// Test basic Nsite navigation
IN_PROC_BROWSER_TEST_F(NsiteNavigationBrowserTest, BasicNsiteNavigation) {
  WaitForLocalRelayReady();
  
  // Publish an Nsite
  std::string nsite_id = PublishNsiteContent(
      "Test Nsite",
      "<h1>Welcome to Test Nsite</h1><p>This is a test.</p>");
  ASSERT_FALSE(nsite_id.empty());
  
  // Navigate to Nsite URL
  GURL nsite_url = test::CreateNostrURL("nsite", nsite_id);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), nsite_url, 1);
  
  // Should render the Nsite content
  std::string page_content;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(document.body.innerHTML);",
      &page_content));
  EXPECT_TRUE(page_content.find("Welcome to Test Nsite") != std::string::npos);
}

// Test Nsite with multiple pages
IN_PROC_BROWSER_TEST_F(NsiteNavigationBrowserTest, MultiPageNsite) {
  WaitForLocalRelayReady();
  
  // Publish main page
  std::string main_page = R"(
    <h1>Multi-Page Nsite</h1>
    <nav>
      <a href="/about">About</a>
      <a href="/contact">Contact</a>
    </nav>
    <p>This is the home page.</p>
  )";
  
  std::string main_id = PublishNsiteContent("Multi-Page Site", main_page);
  ASSERT_FALSE(main_id.empty());
  
  // Publish about page with same title but different path tag
  std::string about_event = test::CreateTestEvent(
      34128,  // Nsite kind
      "<h1>About Page</h1><p>This is the about page.</p>",
      {{"title", "Multi-Page Site"}, {"path", "/about"}});
  SendTestEventToLocalRelay(about_event);
  
  // Navigate to main Nsite
  GURL nsite_url = test::CreateNostrURL("nsite", main_id);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), nsite_url, 1);
  
  // Click the about link
  EXPECT_TRUE(content::ExecuteScript(
      web_contents(),
      "document.querySelector('a[href=\"/about\"]').click();"));
  
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  
  // Should show about page content
  std::string about_content;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(document.body.innerHTML);",
      &about_content));
  EXPECT_TRUE(about_content.find("About Page") != std::string::npos);
}

// Test Nsite theme application
IN_PROC_BROWSER_TEST_F(NsiteNavigationBrowserTest, NsiteThemes) {
  WaitForLocalRelayReady();
  
  // Publish Nsite with dark theme
  std::string dark_nsite = PublishNsiteContent(
      "Dark Theme Site",
      "<h1>Dark Theme Test</h1><p>This site uses a dark theme.</p>",
      "dark");
  ASSERT_FALSE(dark_nsite.empty());
  
  // Navigate to Nsite
  GURL nsite_url = test::CreateNostrURL("nsite", dark_nsite);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), nsite_url, 1);
  
  // Check if theme is applied (via CSS class or data attribute)
  bool has_dark_theme = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      R"(
        window.domAutomationController.send(
          document.body.classList.contains('theme-dark') ||
          document.documentElement.getAttribute('data-theme') === 'dark'
        );
      )",
      &has_dark_theme));
  EXPECT_TRUE(has_dark_theme);
}

// Test Nsite with embedded resources
IN_PROC_BROWSER_TEST_F(NsiteNavigationBrowserTest, NsiteWithResources) {
  WaitForLocalRelayReady();
  
  // Publish Nsite with images and styles
  std::string content_with_resources = R"(
    <h1>Nsite with Resources</h1>
    <img src="blossom://abc123/image.jpg" alt="Test Image">
    <link rel="stylesheet" href="blossom://def456/styles.css">
    <script src="blossom://ghi789/script.js"></script>
    <p>This Nsite includes external resources.</p>
  )";
  
  std::string nsite_id = PublishNsiteContent(
      "Resource Test Site",
      content_with_resources);
  ASSERT_FALSE(nsite_id.empty());
  
  // Navigate to Nsite
  GURL nsite_url = test::CreateNostrURL("nsite", nsite_id);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), nsite_url, 1);
  
  // Check if resources are present in DOM
  bool has_image = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      R"(
        window.domAutomationController.send(
          document.querySelector('img[src*="blossom://"]') !== null
        );
      )",
      &has_image));
  EXPECT_TRUE(has_image);
}

// Test Nsite navigation history
IN_PROC_BROWSER_TEST_F(NsiteNavigationBrowserTest, NavigationHistory) {
  WaitForLocalRelayReady();
  
  // Publish two different Nsites
  std::string nsite1_id = PublishNsiteContent(
      "First Site",
      "<h1>First Nsite</h1>");
  std::string nsite2_id = PublishNsiteContent(
      "Second Site",
      "<h1>Second Nsite</h1>");
  
  ASSERT_FALSE(nsite1_id.empty());
  ASSERT_FALSE(nsite2_id.empty());
  
  // Navigate to first Nsite
  GURL nsite1_url = test::CreateNostrURL("nsite", nsite1_id);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), nsite1_url, 1);
  
  // Navigate to second Nsite
  GURL nsite2_url = test::CreateNostrURL("nsite", nsite2_id);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), nsite2_url, 1);
  
  // Go back
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  
  // Should be on first Nsite
  std::string current_content;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(document.querySelector('h1').textContent);",
      &current_content));
  EXPECT_EQ("First Nsite", current_content);
  
  // Go forward
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  
  // Should be on second Nsite
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(document.querySelector('h1').textContent);",
      &current_content));
  EXPECT_EQ("Second Nsite", current_content);
}

// Test Nsite error handling
IN_PROC_BROWSER_TEST_F(NsiteNavigationBrowserTest, NsiteNotFound) {
  WaitForLocalRelayReady();
  
  // Navigate to non-existent Nsite
  GURL invalid_nsite = test::CreateNostrURL("nsite", "nonexistent123");
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), invalid_nsite, 1);
  
  // Should show error page
  std::string page_content;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(document.body.textContent);",
      &page_content));
  EXPECT_TRUE(page_content.find("not found") != std::string::npos ||
              page_content.find("404") != std::string::npos ||
              page_content.find("error") != std::string::npos);
}

// Test Nsite content security
IN_PROC_BROWSER_TEST_F(NsiteNavigationBrowserTest, NsiteContentSecurity) {
  WaitForLocalRelayReady();
  
  // Publish Nsite with potentially dangerous content
  std::string dangerous_content = R"(
    <h1>Security Test</h1>
    <script>alert('This should be blocked');</script>
    <iframe src="https://evil.com"></iframe>
    <object data="file:///etc/passwd"></object>
    <p onclick="alert('inline handler')">Click me</p>
  )";
  
  std::string nsite_id = PublishNsiteContent(
      "Security Test Site",
      dangerous_content);
  ASSERT_FALSE(nsite_id.empty());
  
  // Navigate to Nsite
  GURL nsite_url = test::CreateNostrURL("nsite", nsite_id);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), nsite_url, 1);
  
  // Check that dangerous elements are sanitized or blocked
  bool has_script_tag = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      R"(
        window.domAutomationController.send(
          document.querySelector('script') !== null
        );
      )",
      &has_script_tag));
  // Scripts should be removed or blocked
  EXPECT_FALSE(has_script_tag);
  
  // Check for CSP headers
  std::string csp_header;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      R"(
        const meta = document.querySelector('meta[http-equiv="Content-Security-Policy"]');
        window.domAutomationController.send(meta ? meta.content : 'none');
      )",
      &csp_header));
  // Should have restrictive CSP
  if (csp_header != "none") {
    EXPECT_TRUE(csp_header.find("script-src 'none'") != std::string::npos ||
                csp_header.find("script-src 'self'") != std::string::npos);
  }
}

// Test Nsite update/versioning
IN_PROC_BROWSER_TEST_F(NsiteNavigationBrowserTest, NsiteUpdates) {
  WaitForLocalRelayReady();
  
  // Publish initial version
  std::string v1_content = "<h1>Version 1</h1><p>Initial content.</p>";
  
  // Create event with d-tag for replaceable event
  std::string v1_event = test::CreateTestEvent(
      34128,
      v1_content,
      {{"title", "Versioned Site"}, {"d", "my-site"}, {"version", "1.0"}});
  SendTestEventToLocalRelay(v1_event);
  
  auto v1_parsed = base::JSONReader::Read(v1_event);
  std::string v1_id = *v1_parsed->GetDict().FindString("id");
  
  // Navigate to v1
  GURL nsite_url = test::CreateNostrURL("nsite", v1_id);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), nsite_url, 1);
  
  // Verify v1 content
  std::string current_version;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(document.querySelector('h1').textContent);",
      &current_version));
  EXPECT_EQ("Version 1", current_version);
  
  // Publish v2 with same d-tag (should replace v1)
  std::string v2_content = "<h1>Version 2</h1><p>Updated content!</p>";
  std::string v2_event = test::CreateTestEvent(
      34128,
      v2_content,
      {{"title", "Versioned Site"}, {"d", "my-site"}, {"version", "2.0"}});
  SendTestEventToLocalRelay(v2_event);
  
  // Refresh the page
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  
  // Should show v2 content
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(document.querySelector('h1').textContent);",
      &current_version));
  EXPECT_EQ("Version 2", current_version);
}

// Test Nsite JavaScript API access
IN_PROC_BROWSER_TEST_F(NsiteNavigationBrowserTest, NsiteJavaScriptAPI) {
  WaitForLocalRelayReady();
  
  // Setup key and permissions
  std::string pubkey = CreateAndStoreTestKey("nsite-js-key", "password");
  
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
  
  // Publish Nsite that uses window.nostr
  std::string interactive_content = R"(
    <h1>Interactive Nsite</h1>
    <button id="get-pubkey">Get Public Key</button>
    <div id="result"></div>
    <script>
      document.getElementById('get-pubkey').onclick = async () => {
        try {
          const pubkey = await window.nostr.getPublicKey();
          document.getElementById('result').textContent = 'Pubkey: ' + pubkey;
        } catch (e) {
          document.getElementById('result').textContent = 'Error: ' + e.message;
        }
      };
    </script>
  )";
  
  std::string nsite_id = PublishNsiteContent(
      "Interactive Site",
      interactive_content);
  ASSERT_FALSE(nsite_id.empty());
  
  // Navigate to Nsite
  GURL nsite_url = test::CreateNostrURL("nsite", nsite_id);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), nsite_url, 1);
  
  // Grant permission for Nsite origin
  GrantNIP07Permission(web_contents()->GetLastCommittedURL().DeprecatedGetOriginAsURL());
  
  // Click the button
  EXPECT_TRUE(content::ExecuteScript(
      web_contents(),
      "document.getElementById('get-pubkey').click();"));
  
  // Wait a bit for async operation
  base::PlatformThread::Sleep(base::Milliseconds(100));
  
  // Check result
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(document.getElementById('result').textContent);",
      &result));
  EXPECT_TRUE(result.find(pubkey) != std::string::npos);
}

// Test cross-Nsite navigation
IN_PROC_BROWSER_TEST_F(NsiteNavigationBrowserTest, CrossNsiteLinks) {
  WaitForLocalRelayReady();
  
  // Publish first Nsite with link to second
  std::string site1_content = R"(
    <h1>Site 1</h1>
    <a id="link-to-site2" href="nostr:nsite:SITE2_ID">Go to Site 2</a>
  )";
  
  std::string site1_id = PublishNsiteContent("Site 1", site1_content);
  
  // Publish second Nsite
  std::string site2_id = PublishNsiteContent(
      "Site 2",
      "<h1>Site 2</h1><p>You made it!</p>");
  
  // Update site1 content with actual site2 ID
  site1_content.replace(site1_content.find("SITE2_ID"), 8, site2_id);
  
  // Re-publish site1 with correct link
  std::string updated_event = test::CreateTestEvent(
      34128,
      site1_content,
      {{"title", "Site 1"}, {"d", "site1"}});
  SendTestEventToLocalRelay(updated_event);
  
  // Navigate to site1
  GURL site1_url = test::CreateNostrURL("nsite", site1_id);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), site1_url, 1);
  
  // Click link to site2
  EXPECT_TRUE(content::ExecuteScript(
      web_contents(),
      "document.getElementById('link-to-site2').click();"));
  
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  
  // Should be on site2
  std::string current_title;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(document.querySelector('h1').textContent);",
      &current_title));
  EXPECT_EQ("Site 2", current_title);
}

}  // namespace nostr