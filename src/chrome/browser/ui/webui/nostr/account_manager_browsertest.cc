// Copyright 2024 The dryft Authors
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

namespace dryft {

class AccountManagerBrowserTest : public InProcessBrowserTest {
 public:
  AccountManagerBrowserTest() = default;
  ~AccountManagerBrowserTest() override = default;

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
  
  bool WaitForElement(const std::string& selector) {
    content::WebContents* web_contents = GetActiveWebContents();
    return content::ExecJs(web_contents,
        base::StringPrintf(
            "new Promise((resolve) => {"
            "  const checkElement = () => {"
            "    const element = document.querySelector('%s');"
            "    if (element) {"
            "      resolve(true);"
            "    } else {"
            "      setTimeout(checkElement, 100);"
            "    }"
            "  };"
            "  checkElement();"
            "});", selector.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(AccountManagerBrowserTest, AccountSectionLoads) {
  NavigateToNostrSettings();
  
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  
  // Check that account section exists
  bool has_account_section = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  !!document.querySelector('#accounts')"
      ");",
      &has_account_section));
  EXPECT_TRUE(has_account_section);
  
  // Check for add account button
  bool has_add_button = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  !!document.querySelector('#add-account-button')"
      ");",
      &has_add_button));
  EXPECT_TRUE(has_add_button);
  
  // Check for import button
  bool has_import_button = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  !!document.querySelector('#import-account-button')"
      ");",
      &has_import_button));
  EXPECT_TRUE(has_import_button);
}

IN_PROC_BROWSER_TEST_F(AccountManagerBrowserTest, CreateAccountDialogOpens) {
  NavigateToNostrSettings();
  
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  
  // Click add account button
  ASSERT_TRUE(content::ExecuteScript(
      web_contents,
      "document.querySelector('#add-account-button').click();"));
  
  // Wait for dialog
  ASSERT_TRUE(WaitForElement("#account-creation-dialog[open]"));
  
  // Check that wizard steps are shown
  bool has_wizard_steps = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  !!document.querySelector('.wizard-steps')"
      ");",
      &has_wizard_steps));
  EXPECT_TRUE(has_wizard_steps);
  
  // Check first step is active
  bool first_step_active = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  document.querySelector('.step[data-step=\"1\"]').classList.contains('active')"
      ");",
      &first_step_active));
  EXPECT_TRUE(first_step_active);
}

IN_PROC_BROWSER_TEST_F(AccountManagerBrowserTest, ImportAccountDialogOpens) {
  NavigateToNostrSettings();
  
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  
  // Click import account button
  ASSERT_TRUE(content::ExecuteScript(
      web_contents,
      "document.querySelector('#import-account-button').click();"));
  
  // Wait for dialog
  ASSERT_TRUE(WaitForElement("#import-account-dialog[open]"));
  
  // Check that import tabs are shown
  bool has_import_tabs = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  !!document.querySelector('.import-tabs')"
      ");",
      &has_import_tabs));
  EXPECT_TRUE(has_import_tabs);
  
  // Check text tab is active by default
  bool text_tab_active = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  document.querySelector('.tab-button[data-tab=\"text\"]').classList.contains('active')"
      ");",
      &text_tab_active));
  EXPECT_TRUE(text_tab_active);
}

IN_PROC_BROWSER_TEST_F(AccountManagerBrowserTest, WizardNavigation) {
  NavigateToNostrSettings();
  
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  
  // Open create account dialog
  ASSERT_TRUE(content::ExecuteScript(
      web_contents,
      "document.querySelector('#add-account-button').click();"));
  
  ASSERT_TRUE(WaitForElement("#account-creation-dialog[open]"));
  
  // Mock key generation
  ASSERT_TRUE(content::ExecuteScript(
      web_contents,
      "window.AccountManager.generatedKeys = {"
      "  pubkey: 'test_pubkey',"
      "  privkey: 'test_privkey',"
      "  npub: 'npub1test',"
      "  nsec: 'nsec1test'"
      "};"));
  
  // Click next button
  ASSERT_TRUE(content::ExecuteScript(
      web_contents,
      "document.querySelector('#wizard-next').click();"));
  
  // Check second step is active
  bool second_step_active = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  document.querySelector('.step[data-step=\"2\"]').classList.contains('active')"
      ");",
      &second_step_active));
  EXPECT_TRUE(second_step_active);
  
  // Fill in profile data
  ASSERT_TRUE(content::ExecuteScript(
      web_contents,
      "document.querySelector('#account-name').value = 'Test User';"));
  
  // Click next again
  ASSERT_TRUE(content::ExecuteScript(
      web_contents,
      "document.querySelector('#wizard-next').click();"));
  
  // Check third step is active
  bool third_step_active = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  document.querySelector('.step[data-step=\"3\"]').classList.contains('active')"
      ");",
      &third_step_active));
  EXPECT_TRUE(third_step_active);
  
  // Check finish button is shown
  bool finish_shown = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  !document.querySelector('#wizard-finish').hidden"
      ");",
      &finish_shown));
  EXPECT_TRUE(finish_shown);
}

IN_PROC_BROWSER_TEST_F(AccountManagerBrowserTest, ImportTabSwitching) {
  NavigateToNostrSettings();
  
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  
  // Open import dialog
  ASSERT_TRUE(content::ExecuteScript(
      web_contents,
      "document.querySelector('#import-account-button').click();"));
  
  ASSERT_TRUE(WaitForElement("#import-account-dialog[open]"));
  
  // Click file tab
  ASSERT_TRUE(content::ExecuteScript(
      web_contents,
      "document.querySelector('.tab-button[data-tab=\"file\"]').click();"));
  
  // Check file tab is active
  bool file_tab_active = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  document.querySelector('.tab-button[data-tab=\"file\"]').classList.contains('active')"
      ");",
      &file_tab_active));
  EXPECT_TRUE(file_tab_active);
  
  // Check file panel is shown
  bool file_panel_shown = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  !document.querySelector('#import-file').hidden"
      ");",
      &file_panel_shown));
  EXPECT_TRUE(file_panel_shown);
  
  // Click QR tab
  ASSERT_TRUE(content::ExecuteScript(
      web_contents,
      "document.querySelector('.tab-button[data-tab=\"qr\"]').click();"));
  
  // Check QR panel is shown
  bool qr_panel_shown = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  !document.querySelector('#import-qr').hidden"
      ");",
      &qr_panel_shown));
  EXPECT_TRUE(qr_panel_shown);
}

IN_PROC_BROWSER_TEST_F(AccountManagerBrowserTest, AccountListDisplay) {
  NavigateToNostrSettings();
  
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  
  // Mock account data
  ASSERT_TRUE(content::ExecuteScript(
      web_contents,
      "window.AccountManager.displayAccounts(["
      "  {"
      "    pubkey: 'test_pubkey_1',"
      "    name: 'Test Account 1',"
      "    isDefault: true,"
      "    picture: '',"
      "    nip05: 'test@example.com'"
      "  },"
      "  {"
      "    pubkey: 'test_pubkey_2',"
      "    name: 'Test Account 2',"
      "    isDefault: false"
      "  }"
      "]);"));
  
  // Check current account is displayed
  bool current_account_shown = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  !document.querySelector('#current-account').hidden"
      ");",
      &current_account_shown));
  EXPECT_TRUE(current_account_shown);
  
  // Check account items are created
  int account_count = 0;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      web_contents,
      "window.domAutomationController.send("
      "  document.querySelectorAll('.account-item').length"
      ");",
      &account_count));
  EXPECT_EQ(2, account_count);
  
  // Check active badge is shown
  bool has_active_badge = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  !!document.querySelector('.default-badge')"
      ");",
      &has_active_badge));
  EXPECT_TRUE(has_active_badge);
}

}  // namespace dryft