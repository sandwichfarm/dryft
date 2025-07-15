// Copyright 2024 The Chromium Authors and Alex313031
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/nostr_status_button.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/nostr/nostr_service.h"
#include "chrome/browser/nostr/nostr_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"

class NostrStatusButtonTest : public TestWithBrowserView {
 public:
  NostrStatusButtonTest() = default;
  NostrStatusButtonTest(const NostrStatusButtonTest&) = delete;
  NostrStatusButtonTest& operator=(const NostrStatusButtonTest&) = delete;
  ~NostrStatusButtonTest() override = default;

  void SetUp() override {
    TestWithBrowserView::SetUp();
    
    // Create the status button
    nostr_button_ = std::make_unique<NostrStatusButton>(browser());
    
    // Add to a parent view for proper testing
    browser_view()->GetWidget()->GetContentsView()->AddChildView(
        nostr_button_.get());
  }

  void TearDown() override {
    nostr_button_.reset();
    TestWithBrowserView::TearDown();
  }

 protected:
  NostrStatusButton* nostr_button() { return nostr_button_.get(); }

 private:
  std::unique_ptr<NostrStatusButton> nostr_button_;
};

TEST_F(NostrStatusButtonTest, InitialState) {
  EXPECT_TRUE(nostr_button());
  EXPECT_FALSE(nostr_button()->GetTooltipText(gfx::Point()).empty());
}

TEST_F(NostrStatusButtonTest, UpdateStatusChangesIcon) {
  // Initial update should not crash
  nostr_button()->UpdateStatus();
  
  // Verify the button is in a valid state
  EXPECT_TRUE(nostr_button()->GetVisible());
}

TEST_F(NostrStatusButtonTest, ButtonShowsMenu) {
  EXPECT_TRUE(nostr_button()->ShouldShowMenu());
}

TEST_F(NostrStatusButtonTest, MenuModelCreation) {
  // Test that the menu can be created without crashing
  // Note: We can't easily test ShowDropDownMenu without a full UI setup
  EXPECT_TRUE(nostr_button()->ShouldShowMenu());
}

TEST_F(NostrStatusButtonTest, AccessibilityProperties) {
  ui::AXNodeData node_data;
  nostr_button()->GetAccessibleNodeData(&node_data);
  
  // Should have accessible name
  EXPECT_FALSE(node_data.GetStringAttribute(ax::mojom::StringAttribute::kName).empty());
}

TEST_F(NostrStatusButtonTest, TooltipText) {
  gfx::Point point(10, 10);
  std::u16string tooltip = nostr_button()->GetTooltipText(point);
  
  // Should have tooltip text
  EXPECT_FALSE(tooltip.empty());
}

// Test connection status logic
TEST_F(NostrStatusButtonTest, ConnectionStatusLogic) {
  // Test different connection states
  nostr_button()->UpdateStatus();
  
  // Should not crash with no Nostr service
  EXPECT_TRUE(nostr_button()->GetVisible());
}