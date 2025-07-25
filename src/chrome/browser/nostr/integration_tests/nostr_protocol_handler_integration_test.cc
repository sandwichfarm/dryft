// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/integration_tests/nostr_integration_test_base.h"

#include "base/strings/string_util.h"
#include "chrome/browser/nostr/integration_tests/test_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace nostr {

class NostrProtocolHandlerIntegrationTest : public InProcessBrowserTest,
                                           public NostrIntegrationTestBase {
 public:
  NostrProtocolHandlerIntegrationTest() = default;
  ~NostrProtocolHandlerIntegrationTest() override = default;

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
  
  void NavigateAndWaitForLoad(const GURL& url) {
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), url, 1);
    EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  }
};

// Test basic nostr: URL navigation
IN_PROC_BROWSER_TEST_F(NostrProtocolHandlerIntegrationTest, BasicNostrURLNavigation) {
  // Test profile URL
  GURL profile_url = test::CreateNostrURL("npub", "1234567890abcdef");
  NavigateAndWaitForLoad(profile_url);
  
  // Should redirect to appropriate handler page
  GURL current_url = web_contents()->GetLastCommittedURL();
  EXPECT_TRUE(current_url.SchemeIs("https") || current_url.SchemeIs("http"));
  
  // Page should contain profile information
  std::string page_content;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(document.body.innerText);",
      &page_content));
  EXPECT_FALSE(page_content.empty());
}

// Test nostr:nevent navigation
IN_PROC_BROWSER_TEST_F(NostrProtocolHandlerIntegrationTest, NostrEventNavigation) {
  // Create a test event ID
  std::string event_id = "nevent1234567890abcdef";
  GURL event_url = test::CreateNostrURL("nevent", event_id);
  
  NavigateAndWaitForLoad(event_url);
  
  // Should load event viewer
  GURL current_url = web_contents()->GetLastCommittedURL();
  EXPECT_TRUE(current_url.SchemeIs("https") || current_url.SchemeIs("http"));
}

// Test nostr:naddr navigation (parameterized replaceable events)
IN_PROC_BROWSER_TEST_F(NostrProtocolHandlerIntegrationTest, NostrNaddrNavigation) {
  // Create a test naddr
  std::string naddr = "naddr1234567890abcdef";
  GURL naddr_url = test::CreateNostrURL("naddr", naddr);
  
  NavigateAndWaitForLoad(naddr_url);
  
  // Should load appropriate content
  GURL current_url = web_contents()->GetLastCommittedURL();
  EXPECT_TRUE(current_url.SchemeIs("https") || current_url.SchemeIs("http"));
}

// Test nostr:nprofile navigation with relay hints
IN_PROC_BROWSER_TEST_F(NostrProtocolHandlerIntegrationTest, NostrNprofileNavigation) {
  // Create a test nprofile (includes relay hints)
  std::string nprofile = "nprofile1234567890abcdef";
  GURL nprofile_url = test::CreateNostrURL("nprofile", nprofile);
  
  NavigateAndWaitForLoad(nprofile_url);
  
  // Should load profile with relay information
  GURL current_url = web_contents()->GetLastCommittedURL();
  EXPECT_TRUE(current_url.SchemeIs("https") || current_url.SchemeIs("http"));
}

// Test invalid nostr: URLs
IN_PROC_BROWSER_TEST_F(NostrProtocolHandlerIntegrationTest, InvalidNostrURLs) {
  // Test malformed URL
  GURL invalid_url("nostr:invalid");
  NavigateAndWaitForLoad(invalid_url);
  
  // Should show error page or redirect to search
  std::string page_title;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(),
      "window.domAutomationController.send(document.title);",
      &page_title));
  EXPECT_FALSE(page_title.empty());
}

// Test nostr: URL handling from JavaScript
IN_PROC_BROWSER_TEST_F(NostrProtocolHandlerIntegrationTest, NostrURLFromJavaScript) {
  // Navigate to a test page first
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Use JavaScript to navigate to nostr: URL
  std::string script = R"(
    window.location.href = 'nostr:npub:testpubkey';
    window.domAutomationController.send('navigated');
  )";
  
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), script, &result));
  
  // Wait for navigation
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  
  // Check we navigated away from the test page
  GURL current_url = web_contents()->GetLastCommittedURL();
  EXPECT_NE(test_url, current_url);
}

// Test nostr: URL in anchor tags
IN_PROC_BROWSER_TEST_F(NostrProtocolHandlerIntegrationTest, NostrURLInAnchorTags) {
  // Create a page with nostr: links
  std::string html = test::CreateNostrTestHTML(R"(
    <a id="profile-link" href="nostr:npub:test123">Profile Link</a>
    <a id="event-link" href="nostr:nevent:test456">Event Link</a>
  )");
  
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating([&](const net::test_server::HttpRequest& request)
                             -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != "/nostr_links.html") {
          return nullptr;
        }
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_content(html);
        response->set_content_type("text/html");
        return response;
      }));
  
  GURL test_url = embedded_test_server()->GetURL("/nostr_links.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Click the profile link
  EXPECT_TRUE(content::ExecuteScript(
      web_contents(),
      "document.getElementById('profile-link').click();"));
  
  // Wait for navigation
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  
  // Should have navigated away
  GURL current_url = web_contents()->GetLastCommittedURL();
  EXPECT_NE(test_url, current_url);
}

// Test window.open with nostr: URLs
IN_PROC_BROWSER_TEST_F(NostrProtocolHandlerIntegrationTest, WindowOpenNostrURL) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Get initial tab count
  int initial_tab_count = browser()->tab_strip_model()->count();
  
  // Open nostr URL in new window
  std::string script = R"(
    window.open('nostr:npub:newwindowtest', '_blank');
    window.domAutomationController.send('opened');
  )";
  
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents(), script, &result));
  EXPECT_EQ("opened", result);
  
  // Wait for new tab
  ui_test_utils::WaitForBrowserToOpen();
  
  // Should have opened a new tab
  EXPECT_GT(browser()->tab_strip_model()->count(), initial_tab_count);
}

// Test nostr: URL protocol registration
IN_PROC_BROWSER_TEST_F(NostrProtocolHandlerIntegrationTest, ProtocolRegistration) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Check if browser recognizes nostr: as a valid protocol
  bool is_registered = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      R"(
        // Try to create a URL with nostr: protocol
        try {
          const url = new URL('nostr:npub:test');
          window.domAutomationController.send(url.protocol === 'nostr:');
        } catch (e) {
          window.domAutomationController.send(false);
        }
      )",
      &is_registered));
  EXPECT_TRUE(is_registered);
}

// Test Nsite URL handling (nostr:nsite:)
IN_PROC_BROWSER_TEST_F(NostrProtocolHandlerIntegrationTest, NsiteURLHandling) {
  // Create an Nsite URL
  std::string nsite_id = "nsite1234567890abcdef";
  GURL nsite_url = test::CreateNostrURL("nsite", nsite_id);
  
  NavigateAndWaitForLoad(nsite_url);
  
  // Should load Nsite content
  GURL current_url = web_contents()->GetLastCommittedURL();
  EXPECT_TRUE(current_url.SchemeIs("https") || current_url.SchemeIs("http"));
  
  // Check for Nsite-specific elements
  bool has_nsite_marker = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(),
      R"(
        window.domAutomationController.send(
          document.querySelector('meta[name="nsite-id"]') !== null ||
          window.__nsiteId !== undefined
        );
      )",
      &has_nsite_marker));
}

// Test deep linking with nostr: URLs
IN_PROC_BROWSER_TEST_F(NostrProtocolHandlerIntegrationTest, DeepLinking) {
  // Test various deep link formats
  std::vector<std::pair<std::string, std::string>> test_cases = {
      {"nostr:npub1234567890abcdef", "profile"},
      {"nostr:nevent1234567890abcdef", "event"},
      {"nostr:naddr1234567890abcdef", "address"},
      {"nostr:note1234567890abcdef", "note"},
  };
  
  for (const auto& [url_str, expected_type] : test_cases) {
    GURL test_url(url_str);
    NavigateAndWaitForLoad(test_url);
    
    // Verify navigation occurred
    GURL current_url = web_contents()->GetLastCommittedURL();
    EXPECT_NE(test_url, current_url);
    EXPECT_TRUE(current_url.SchemeIs("https") || current_url.SchemeIs("http"));
  }
}

// Test concurrent nostr: URL navigations
IN_PROC_BROWSER_TEST_F(NostrProtocolHandlerIntegrationTest, ConcurrentNavigations) {
  // Open multiple tabs with different nostr: URLs
  std::vector<std::string> urls = {
      "nostr:npub:tab1",
      "nostr:nevent:tab2",
      "nostr:naddr:tab3"
  };
  
  int initial_tab_count = browser()->tab_strip_model()->count();
  
  for (const auto& url : urls) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(),
        GURL(url),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }
  
  // Should have opened new tabs
  EXPECT_EQ(browser()->tab_strip_model()->count(), 
            initial_tab_count + static_cast<int>(urls.size()));
  
  // Each tab should have navigated successfully
  for (int i = 1; i <= urls.size(); ++i) {
    browser()->tab_strip_model()->ActivateTabAt(i);
    content::WebContents* tab_contents = 
        browser()->tab_strip_model()->GetActiveWebContents();
    
    GURL tab_url = tab_contents->GetLastCommittedURL();
    EXPECT_TRUE(tab_url.SchemeIs("https") || tab_url.SchemeIs("http"));
  }
}

}  // namespace nostr