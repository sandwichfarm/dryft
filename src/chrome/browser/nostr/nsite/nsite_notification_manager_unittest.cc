// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_notification_manager.h"

#include "base/test/task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {

class NsiteNotificationManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    notification_manager_ = std::make_unique<NsiteNotificationManager>();
    web_contents_ = web_contents_factory_.CreateWebContents(&browser_context_);
  }

  void TearDown() override {
    notification_manager_.reset();
  }

  base::test::TaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  content::TestWebContentsFactory web_contents_factory_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<NsiteNotificationManager> notification_manager_;
};

TEST_F(NsiteNotificationManagerTest, ShowUpdateNotification) {
  const std::string npub = "npub1test123";
  const std::string path = "/index.html";
  
  // Should not crash when showing notification
  notification_manager_->ShowUpdateNotification(web_contents_.get(), npub, path);
  
  task_environment_.RunUntilIdle();
  
  SUCCEED();  // Test passes if no crash occurs
}

TEST_F(NsiteNotificationManagerTest, DismissNotification) {
  const std::string npub = "npub1test123";
  
  // Initially not dismissed
  EXPECT_FALSE(notification_manager_->IsNotificationDismissed(npub));
  
  // Dismiss the notification
  notification_manager_->DismissNotification(npub);
  
  // Should now be dismissed
  EXPECT_TRUE(notification_manager_->IsNotificationDismissed(npub));
}

TEST_F(NsiteNotificationManagerTest, DismissedNotificationNotShown) {
  const std::string npub = "npub1test123";
  const std::string path = "/index.html";
  
  // Dismiss first
  notification_manager_->DismissNotification(npub);
  EXPECT_TRUE(notification_manager_->IsNotificationDismissed(npub));
  
  // Subsequent show should be ignored (logged as skipped)
  notification_manager_->ShowUpdateNotification(web_contents_.get(), npub, path);
  
  task_environment_.RunUntilIdle();
  
  SUCCEED();  // Test passes if no crash occurs
}

TEST_F(NsiteNotificationManagerTest, ClearDismissedNotifications) {
  const std::string npub1 = "npub1test123";
  const std::string npub2 = "npub1test456";
  
  // Dismiss both
  notification_manager_->DismissNotification(npub1);
  notification_manager_->DismissNotification(npub2);
  
  EXPECT_TRUE(notification_manager_->IsNotificationDismissed(npub1));
  EXPECT_TRUE(notification_manager_->IsNotificationDismissed(npub2));
  
  // Clear all dismissed notifications
  notification_manager_->ClearDismissedNotifications();
  
  EXPECT_FALSE(notification_manager_->IsNotificationDismissed(npub1));
  EXPECT_FALSE(notification_manager_->IsNotificationDismissed(npub2));
}

TEST_F(NsiteNotificationManagerTest, MultipleNsites) {
  const std::string npub1 = "npub1test123";
  const std::string npub2 = "npub1test456";
  const std::string path = "/index.html";
  
  // Show notifications for different nsites
  notification_manager_->ShowUpdateNotification(web_contents_.get(), npub1, path);
  notification_manager_->ShowUpdateNotification(web_contents_.get(), npub2, path);
  
  task_environment_.RunUntilIdle();
  
  // Dismiss one
  notification_manager_->DismissNotification(npub1);
  
  EXPECT_TRUE(notification_manager_->IsNotificationDismissed(npub1));
  EXPECT_FALSE(notification_manager_->IsNotificationDismissed(npub2));
  
  SUCCEED();
}

TEST_F(NsiteNotificationManagerTest, JavaScriptGeneration) {
  const std::string npub = "npub1test123";
  const std::string path = "/index.html";
  
  // Test that notification script can be generated without crashing
  notification_manager_->ShowUpdateNotification(web_contents_.get(), npub, path);
  
  task_environment_.RunUntilIdle();
  
  SUCCEED();  // Test passes if no crash occurs
}

}  // namespace nostr