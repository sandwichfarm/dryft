// Copyright 2024 The Chromium Authors and Alex313031
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/nostr_status_menu_model.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/simple_menu_model.h"

class TestMenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  TestMenuDelegate() = default;
  ~TestMenuDelegate() override = default;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override { return false; }
  bool IsCommandIdEnabled(int command_id) const override { return true; }
  bool IsCommandIdVisible(int command_id) const override { return true; }
  void ExecuteCommand(int command_id, int event_flags) override {
    last_executed_command_ = command_id;
  }

  int last_executed_command() const { return last_executed_command_; }

 private:
  int last_executed_command_ = -1;
};

class NostrStatusMenuModelTest : public testing::Test {
 public:
  NostrStatusMenuModelTest() = default;
  NostrStatusMenuModelTest(const NostrStatusMenuModelTest&) = delete;
  NostrStatusMenuModelTest& operator=(const NostrStatusMenuModelTest&) = delete;
  ~NostrStatusMenuModelTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    browser_window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile_.get(), true);
    params.type = Browser::TYPE_NORMAL;
    params.window = browser_window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(params));
    
    delegate_ = std::make_unique<TestMenuDelegate>();
    menu_model_ = std::make_unique<NostrStatusMenuModel>(
        delegate_.get(), browser_.get());
  }

  void TearDown() override {
    menu_model_.reset();
    delegate_.reset();
    browser_.reset();
    browser_window_.reset();
    profile_.reset();
  }

 protected:
  NostrStatusMenuModel* menu_model() { return menu_model_.get(); }
  TestMenuDelegate* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestMenuDelegate> delegate_;
  std::unique_ptr<NostrStatusMenuModel> menu_model_;
};

TEST_F(NostrStatusMenuModelTest, MenuHasItems) {
  EXPECT_GT(menu_model()->GetItemCount(), 0);
}

TEST_F(NostrStatusMenuModelTest, MenuContainsExpectedCommands) {
  bool found_settings = false;
  bool found_connection_details = false;
  bool found_account_switcher = false;
  bool found_toggle_relay = false;
  
  for (size_t i = 0; i < menu_model()->GetItemCount(); ++i) {
    int command_id = menu_model()->GetCommandIdAt(i);
    switch (command_id) {
      case IDC_NOSTR_SETTINGS:
        found_settings = true;
        break;
      case IDC_NOSTR_CONNECTION_DETAILS:
        found_connection_details = true;
        break;
      case IDC_NOSTR_ACCOUNT_SWITCHER:
        found_account_switcher = true;
        break;
      case IDC_NOSTR_TOGGLE_RELAY:
        found_toggle_relay = true;
        break;
    }
  }
  
  EXPECT_TRUE(found_settings);
  EXPECT_TRUE(found_connection_details);
  EXPECT_TRUE(found_account_switcher);
  EXPECT_TRUE(found_toggle_relay);
}

TEST_F(NostrStatusMenuModelTest, MenuItemsAreEnabled) {
  for (size_t i = 0; i < menu_model()->GetItemCount(); ++i) {
    int command_id = menu_model()->GetCommandIdAt(i);
    if (command_id > 0) {  // Skip separators
      EXPECT_TRUE(delegate()->IsCommandIdEnabled(command_id))
          << "Command " << command_id << " should be enabled";
    }
  }
}

TEST_F(NostrStatusMenuModelTest, MenuItemsAreVisible) {
  for (size_t i = 0; i < menu_model()->GetItemCount(); ++i) {
    int command_id = menu_model()->GetCommandIdAt(i);
    if (command_id > 0) {  // Skip separators
      EXPECT_TRUE(delegate()->IsCommandIdVisible(command_id))
          << "Command " << command_id << " should be visible";
    }
  }
}