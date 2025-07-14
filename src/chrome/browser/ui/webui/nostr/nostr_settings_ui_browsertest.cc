// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/tungsten_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace tungsten {

class NostrSettingsUIBrowserTest : public InProcessBrowserTest {
 public:
  NostrSettingsUIBrowserTest() = default;
  ~NostrSettingsUIBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  void NavigateToNostrSettings() {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(kChromeUINostrSettingsURL)));
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

IN_PROC_BROWSER_TEST_F(NostrSettingsUIBrowserTest, LoadsSuccessfully) {
  NavigateToNostrSettings();
  
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  
  // Verify the page loaded
  EXPECT_EQ(GURL(kChromeUINostrSettingsURL), web_contents->GetURL());
  
  // Wait for the page to fully load
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  
  // Check that the title is set
  std::u16string title = web_contents->GetTitle();
  EXPECT_FALSE(title.empty());
}

IN_PROC_BROWSER_TEST_F(NostrSettingsUIBrowserTest, HasMainSections) {
  NavigateToNostrSettings();
  
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  
  // Check that main sections exist
  bool has_accounts = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  !!document.querySelector('#accounts-section')"
      ");",
      &has_accounts));
  EXPECT_TRUE(has_accounts);
  
  bool has_permissions = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  !!document.querySelector('#permissions-section')"
      ");",
      &has_permissions));
  EXPECT_TRUE(has_permissions);
  
  bool has_relay = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  !!document.querySelector('#local-relay-section')"
      ");",
      &has_relay));
  EXPECT_TRUE(has_relay);
}

IN_PROC_BROWSER_TEST_F(NostrSettingsUIBrowserTest, NavigationWorks) {
  NavigateToNostrSettings();
  
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  
  // Click on permissions navigation
  ASSERT_TRUE(content::ExecuteScript(
      web_contents,
      "document.querySelector('[data-section=\"permissions\"]').click();"));
  
  // Verify the permissions section is shown
  bool permissions_visible = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  document.querySelector('#permissions-section').style.display !== 'none'"
      ");",
      &permissions_visible));
  EXPECT_TRUE(permissions_visible);
  
  // Verify other sections are hidden
  bool accounts_visible = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  document.querySelector('#accounts-section').style.display !== 'none'"
      ");",
      &accounts_visible));
  EXPECT_FALSE(accounts_visible);
}

IN_PROC_BROWSER_TEST_F(NostrSettingsUIBrowserTest, RelayConfigLoads) {
  NavigateToNostrSettings();
  
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  
  // Navigate to relay section
  ASSERT_TRUE(content::ExecuteScript(
      web_contents,
      "document.querySelector('[data-section=\"local-relay\"]').click();"));
  
  // Wait a bit for async data loading
  base::RunLoop().RunUntilIdle();
  
  // Check that relay config fields exist
  bool has_enabled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  !!document.querySelector('#relay-enabled')"
      ");",
      &has_enabled));
  EXPECT_TRUE(has_enabled);
  
  bool has_port = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  !!document.querySelector('#relay-port')"
      ");",
      &has_port));
  EXPECT_TRUE(has_port);
}

}  // namespace tungsten