// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace chrome {

class NostrLibrariesBrowserTest : public InProcessBrowserTest {
 public:
  NostrLibrariesBrowserTest() = default;
  ~NostrLibrariesBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    
    // Ensure the test server is started
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  // Helper to navigate to a chrome:// URL and execute JavaScript
  content::EvalJsResult EvalJsAt(const GURL& url, const std::string& script) {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    return EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), script);
  }
  
  // Check if a library can be loaded via dynamic import
  bool CanLoadLibrary(const std::string& library_path) {
    GURL test_url = embedded_test_server()->GetURL("/empty.html");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
    
    std::string script = base::StringPrintf(R"(
      (async () => {
        try {
          const module = await import('chrome://resources/js/nostr/%s');
          return module && typeof module === 'object';
        } catch (e) {
          console.error('Failed to load library:', e);
          return false;
        }
      })()
    )", library_path.c_str());
    
    return EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), script)
        .ExtractBool();
  }
};

// Test that NDK library can be loaded
IN_PROC_BROWSER_TEST_F(NostrLibrariesBrowserTest, LoadNDKLibrary) {
  EXPECT_TRUE(CanLoadLibrary("ndk.js"));
}

// Test that nostr-tools library can be loaded
IN_PROC_BROWSER_TEST_F(NostrLibrariesBrowserTest, LoadNostrToolsLibrary) {
  EXPECT_TRUE(CanLoadLibrary("nostr-tools.js"));
}

// Test that applesauce library can be loaded
IN_PROC_BROWSER_TEST_F(NostrLibrariesBrowserTest, LoadApplesauceLibrary) {
  EXPECT_TRUE(CanLoadLibrary("applesauce.js"));
}

// Test that nostrify library can be loaded
IN_PROC_BROWSER_TEST_F(NostrLibrariesBrowserTest, LoadNostrifyLibrary) {
  EXPECT_TRUE(CanLoadLibrary("nostrify.js"));
}

// Test that alby-sdk library can be loaded
IN_PROC_BROWSER_TEST_F(NostrLibrariesBrowserTest, LoadAlbySDKLibrary) {
  EXPECT_TRUE(CanLoadLibrary("alby-sdk.js"));
}

// Test direct chrome:// URL access
IN_PROC_BROWSER_TEST_F(NostrLibrariesBrowserTest, DirectChromeURLAccess) {
  // Navigate directly to the NDK library URL
  GURL ndk_url("chrome://resources/js/nostr/ndk.js");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), ndk_url));
  
  // Check that we got JavaScript content
  content::WebContents* web_contents = 
      browser()->tab_strip_model()->GetActiveWebContents();
  std::string content_type;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(document.contentType);",
      &content_type));
  
  // Should be served as JavaScript
  EXPECT_EQ("application/javascript", content_type);
}

// Test that libraries expose expected APIs
IN_PROC_BROWSER_TEST_F(NostrLibrariesBrowserTest, LibraryAPIsAvailable) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Test NDK API
  std::string ndk_test = R"(
    (async () => {
      const NDK = await import('chrome://resources/js/nostr/ndk.js');
      return NDK && NDK.NDK && typeof NDK.NDK === 'function';
    })()
  )";
  EXPECT_TRUE(EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), 
                     ndk_test).ExtractBool());
  
  // Test nostr-tools API
  std::string nostr_tools_test = R"(
    (async () => {
      const tools = await import('chrome://resources/js/nostr/nostr-tools.js');
      return tools && 
             typeof tools.generatePrivateKey === 'function' &&
             typeof tools.getPublicKey === 'function';
    })()
  )";
  EXPECT_TRUE(EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), 
                     nostr_tools_test).ExtractBool());
}

// Test library version information
IN_PROC_BROWSER_TEST_F(NostrLibrariesBrowserTest, LibraryVersions) {
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Check NDK version
  std::string ndk_version_test = R"(
    (async () => {
      const NDK = await import('chrome://resources/js/nostr/ndk.js');
      return NDK.version || 'unknown';
    })()
  )";
  auto ndk_version = EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), 
                           ndk_version_test);
  EXPECT_EQ("2.0.0", ndk_version);
  
  // Check nostr-tools version
  std::string tools_version_test = R"(
    (async () => {
      const tools = await import('chrome://resources/js/nostr/nostr-tools.js');
      return tools.version || 'unknown';
    })()
  )";
  auto tools_version = EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), 
                             tools_version_test);
  EXPECT_EQ("1.17.0", tools_version);
}

// Test library caching headers
IN_PROC_BROWSER_TEST_F(NostrLibrariesBrowserTest, LibraryCaching) {
  // This test verifies that proper caching headers are set
  // The actual verification would require checking HTTP headers
  // For now, we just verify the libraries load successfully multiple times
  
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  
  // Load the same library multiple times
  for (int i = 0; i < 3; i++) {
    std::string script = R"(
      (async () => {
        const start = performance.now();
        await import('chrome://resources/js/nostr/ndk.js');
        const duration = performance.now() - start;
        return duration;
      })()
    )";
    
    auto duration = EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), 
                          script).ExtractDouble();
    
    // Subsequent loads should be faster due to caching
    if (i > 0) {
      EXPECT_LT(duration, 50.0);  // Should be under 50ms from cache
    }
  }
}

}  // namespace chrome