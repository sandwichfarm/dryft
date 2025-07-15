// Copyright 2024 The Chromium Authors and Alex313031
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/nostr/nostr_permission_dialog.h"

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "chrome/browser/nostr/nostr_messages.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/widget_test.h"
#include "url/origin.h"

class NostrPermissionDialogTest : public ChromeViewsTestBase {
 public:
  NostrPermissionDialogTest() = default;
  NostrPermissionDialogTest(const NostrPermissionDialogTest&) = delete;
  NostrPermissionDialogTest& operator=(const NostrPermissionDialogTest&) = delete;
  ~NostrPermissionDialogTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    
    // Create test request
    request_.origin = url::Origin::Create(GURL("https://example.com"));
    request_.method = "getPublicKey";
    request_.timestamp = base::TimeTicks::Now();
    request_.remember_decision = false;
  }

  void TearDown() override {
    // Close any open widgets
    if (dialog_widget_) {
      dialog_widget_->CloseNow();
      dialog_widget_ = nullptr;
    }
    ChromeViewsTestBase::TearDown();
  }

 protected:
  void CreateDialog() {
    anchor_view_ = std::make_unique<views::View>();
    GetContext()->AddChildView(anchor_view_.get());
    
    callback_invoked_ = false;
    callback_granted_ = false;
    callback_remember_ = false;
    
    auto dialog = std::make_unique<NostrPermissionDialog>(
        anchor_view_.get(), request_,
        base::BindOnce(&NostrPermissionDialogTest::OnPermissionCallback,
                       base::Unretained(this)));
    
    dialog_ = dialog.get();
    dialog_widget_ = views::BubbleDialogDelegateView::CreateBubble(std::move(dialog));
    dialog_widget_->Show();
  }

  void OnPermissionCallback(bool granted, bool remember) {
    callback_invoked_ = true;
    callback_granted_ = granted;
    callback_remember_ = remember;
  }

  NostrPermissionRequest request_;
  std::unique_ptr<views::View> anchor_view_;
  NostrPermissionDialog* dialog_ = nullptr;
  views::Widget* dialog_widget_ = nullptr;
  
  bool callback_invoked_ = false;
  bool callback_granted_ = false;
  bool callback_remember_ = false;
};

TEST_F(NostrPermissionDialogTest, CreateAndShow) {
  CreateDialog();
  
  EXPECT_TRUE(dialog_);
  EXPECT_TRUE(dialog_widget_);
  EXPECT_TRUE(dialog_widget_->IsVisible());
  EXPECT_FALSE(callback_invoked_);
}

TEST_F(NostrPermissionDialogTest, WindowTitle) {
  CreateDialog();
  
  std::u16string title = dialog_->GetWindowTitle();
  EXPECT_FALSE(title.empty());
}

TEST_F(NostrPermissionDialogTest, HasExpectedButtons) {
  CreateDialog();
  
  EXPECT_TRUE(dialog_->GetOkButton());
  EXPECT_TRUE(dialog_->GetCancelButton());
  EXPECT_FALSE(dialog_->ShouldShowCloseButton());
}

TEST_F(NostrPermissionDialogTest, AcceptButtonGrantsPermission) {
  CreateDialog();
  
  // Simulate clicking the Accept button
  dialog_->Accept();
  
  EXPECT_TRUE(callback_invoked_);
  EXPECT_TRUE(callback_granted_);
}

TEST_F(NostrPermissionDialogTest, CancelButtonDeniesPermission) {
  CreateDialog();
  
  // Simulate clicking the Cancel button
  dialog_->Cancel();
  
  EXPECT_TRUE(callback_invoked_);
  EXPECT_FALSE(callback_granted_);
}

TEST_F(NostrPermissionDialogTest, CloseWithoutActionDenies) {
  CreateDialog();
  
  // Close dialog without user action
  dialog_widget_->Close();
  
  // Run pending tasks to process the close
  base::RunLoop().RunUntilIdle();
  
  EXPECT_TRUE(callback_invoked_);
  EXPECT_FALSE(callback_granted_);
}

TEST_F(NostrPermissionDialogTest, DifferentMethods) {
  // Test getPublicKey method
  request_.method = "getPublicKey";
  CreateDialog();
  EXPECT_FALSE(dialog_->GetWindowTitle().empty());
  dialog_widget_->Close();
  base::RunLoop().RunUntilIdle();
  
  // Test signEvent method
  request_.method = "signEvent";
  CreateDialog();
  EXPECT_FALSE(dialog_->GetWindowTitle().empty());
  dialog_widget_->Close();
  base::RunLoop().RunUntilIdle();
  
  // Test encryption method
  request_.method = "nip04_encrypt";
  CreateDialog();
  EXPECT_FALSE(dialog_->GetWindowTitle().empty());
}

TEST_F(NostrPermissionDialogTest, EventDetailsForSignEvent) {
  request_.method = "signEvent";
  
  // Add event details
  base::Value::Dict event;
  event.Set("kind", 1);
  event.Set("content", "Test event content");
  request_.details.Set("event", std::move(event));
  
  CreateDialog();
  
  // Verify dialog shows with event details
  EXPECT_TRUE(dialog_->GetWidget()->IsVisible());
}

// Note: Timeout testing would require advancing the test clock
// which is complex in this test setup. In practice, the timeout
// functionality would be tested in integration tests.

class NostrPermissionDialogTimeoutTest : public NostrPermissionDialogTest {
 public:
  NostrPermissionDialogTimeoutTest() : 
    task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(NostrPermissionDialogTimeoutTest, TimeoutCausesAutoDeny) {
  CreateDialog();
  
  EXPECT_FALSE(callback_invoked_);
  
  // Fast forward past the timeout duration
  task_environment_.FastForwardBy(base::Seconds(35));
  
  EXPECT_TRUE(callback_invoked_);
  EXPECT_FALSE(callback_granted_);
}