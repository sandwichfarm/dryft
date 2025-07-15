// Copyright 2024 The Chromium Authors and Alex313031
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_NOSTR_STATUS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_NOSTR_STATUS_BUTTON_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/chrome_views_export.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/dot_indicator.h"

class Browser;
class NostrService;
class NostrStatusMenuModel;

// NostrStatusButton displays the current Nostr connection status and provides
// quick access to Nostr-related features through a dropdown menu.
class NostrStatusButton : public ToolbarButton,
                          public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(NostrStatusButton, ToolbarButton)

 public:
  enum class ConnectionStatus {
    kDisabled,    // Nostr disabled by user
    kDisconnected,  // No relay connections
    kDegraded,    // Some relay connections failed
    kConnected    // All relays connected
  };

  explicit NostrStatusButton(Browser* browser);
  NostrStatusButton(const NostrStatusButton&) = delete;
  NostrStatusButton& operator=(const NostrStatusButton&) = delete;
  ~NostrStatusButton() override;

  // Updates the button appearance based on current Nostr status
  void UpdateStatus();

  // ToolbarButton:
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool ShouldShowMenu() override;
  void ShowDropDownMenu(ui::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // Updates the button icon based on connection status
  void UpdateIcon();
  
  // Updates the notification badge
  void UpdateBadge();
  
  // Gets the current connection status
  ConnectionStatus GetConnectionStatus() const;
  
  // Gets the count of unread notifications
  int GetNotificationCount() const;

  // Called when the button is pressed
  void ButtonPressed(const ui::Event& event);

  // Creates the dropdown menu model
  std::unique_ptr<NostrStatusMenuModel> CreateMenuModel();

  raw_ptr<Browser> browser_;
  raw_ptr<NostrService> nostr_service_;
  
  // Current connection status
  ConnectionStatus current_status_ = ConnectionStatus::kDisabled;
  
  // Notification badge indicator
  std::unique_ptr<views::DotIndicator> notification_badge_;
  
  // Current notification count
  int notification_count_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_NOSTR_STATUS_BUTTON_H_