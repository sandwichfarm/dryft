// Copyright 2024 The Chromium Authors and Alex313031
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/nostr_status_button.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/nostr/nostr_service.h"
#include "chrome/browser/nostr/nostr_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/toolbar/nostr_status_menu_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/dot_indicator.h"

namespace {

// Icon constants
const gfx::VectorIcon& GetNostrIcon(NostrStatusButton::ConnectionStatus status) {
  switch (status) {
    case NostrStatusButton::ConnectionStatus::kConnected:
      return vector_icons::kBoltIcon;
    case NostrStatusButton::ConnectionStatus::kDegraded:
      return vector_icons::kBoltIcon;
    case NostrStatusButton::ConnectionStatus::kDisconnected:
      return vector_icons::kBoltIcon;
    case NostrStatusButton::ConnectionStatus::kDisabled:
      return vector_icons::kBoltIcon;
  }
  return vector_icons::kBoltIcon;
}

SkColor GetStatusColor(NostrStatusButton::ConnectionStatus status,
                      const ui::ColorProvider* color_provider) {
  switch (status) {
    case NostrStatusButton::ConnectionStatus::kConnected:
      return SkColorSetRGB(0, 255, 0);  // Green
    case NostrStatusButton::ConnectionStatus::kDegraded:
      return SkColorSetRGB(255, 255, 0);  // Yellow
    case NostrStatusButton::ConnectionStatus::kDisconnected:
      return SkColorSetRGB(255, 0, 0);  // Red
    case NostrStatusButton::ConnectionStatus::kDisabled:
      return SkColorSetRGB(128, 128, 128);  // Gray
  }
  return SkColorSetRGB(128, 128, 128);
}

}  // namespace

NostrStatusButton::NostrStatusButton(Browser* browser)
    : ToolbarButton(base::BindRepeating(&NostrStatusButton::ButtonPressed,
                                       base::Unretained(this))),
      browser_(browser) {
  SetTooltipText(l10n_util::GetStringUTF16(IDS_NOSTR_STATUS_TOOLTIP));
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_NOSTR_STATUS_BUTTON));
  
  // Get the Nostr service
  nostr_service_ = NostrServiceFactory::GetForProfile(browser_->profile());
  
  // Create notification badge
  notification_badge_ = views::DotIndicator::Install(image_container_view());
  notification_badge_->Hide();
  
  // Set initial status
  UpdateStatus();
}

NostrStatusButton::~NostrStatusButton() = default;

void NostrStatusButton::UpdateStatus() {
  ConnectionStatus new_status = GetConnectionStatus();
  int new_notification_count = GetNotificationCount();
  
  if (new_status != current_status_) {
    current_status_ = new_status;
    UpdateIcon();
  }
  
  if (new_notification_count != notification_count_) {
    notification_count_ = new_notification_count;
    UpdateBadge();
  }
}

std::u16string NostrStatusButton::GetTooltipText(const gfx::Point& p) const {
  std::u16string base_tooltip = l10n_util::GetStringUTF16(IDS_NOSTR_STATUS_TOOLTIP);
  
  switch (current_status_) {
    case ConnectionStatus::kConnected:
      return l10n_util::GetStringUTF16(IDS_NOSTR_STATUS_CONNECTED);
    case ConnectionStatus::kDegraded:
      return l10n_util::GetStringUTF16(IDS_NOSTR_STATUS_DEGRADED);
    case ConnectionStatus::kDisconnected:
      return l10n_util::GetStringUTF16(IDS_NOSTR_STATUS_DISCONNECTED);
    case ConnectionStatus::kDisabled:
      return l10n_util::GetStringUTF16(IDS_NOSTR_STATUS_DISABLED);
  }
  
  return base_tooltip;
}

void NostrStatusButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ToolbarButton::GetAccessibleNodeData(node_data);
  node_data->SetNameExplicitlyEmpty();
  node_data->SetName(GetTooltipText(gfx::Point()));
}

bool NostrStatusButton::ShouldShowMenu() {
  return true;
}

void NostrStatusButton::ShowDropDownMenu(ui::MenuSourceType source_type) {
  auto menu_model = CreateMenuModel();
  RunMenu(std::move(menu_model), browser_, source_type);
}

bool NostrStatusButton::IsCommandIdChecked(int command_id) const {
  // TODO: Implement menu command checking
  return false;
}

bool NostrStatusButton::IsCommandIdEnabled(int command_id) const {
  // TODO: Implement menu command enabling
  return true;
}

bool NostrStatusButton::IsCommandIdVisible(int command_id) const {
  // TODO: Implement menu command visibility
  return true;
}

void NostrStatusButton::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case IDC_NOSTR_SETTINGS:
      chrome::ShowNostrSettings(browser_);
      break;
    case IDC_NOSTR_MANAGE_KEYS:
      // TODO: Implement key management dialog
      break;
    case IDC_NOSTR_CONNECTION_DETAILS:
      // TODO: Show connection status dialog
      break;
    case IDC_NOSTR_ACCOUNT_SWITCHER:
      // TODO: Show account switcher dialog
      break;
    case IDC_NOSTR_TOGGLE_RELAY:
      // TODO: Toggle local relay on/off
      break;
    default:
      NOTREACHED() << "Unknown command ID: " << command_id;
      break;
  }
}

void NostrStatusButton::UpdateIcon() {
  const gfx::VectorIcon& icon = GetNostrIcon(current_status_);
  SkColor color = GetStatusColor(current_status_, GetColorProvider());
  
  SetVectorIcon(icon);
  // TODO: Apply color to icon when color theming is available
}

void NostrStatusButton::UpdateBadge() {
  if (notification_count_ > 0) {
    // Position badge in top-right corner
    gfx::Rect badge_rect(8, 8);
    badge_rect.set_origin(
        image_container_view()->GetLocalBounds().bottom_right() -
        badge_rect.bottom_right().OffsetFromOrigin());
    notification_badge_->SetBoundsRect(badge_rect);
    notification_badge_->Show();
  } else {
    notification_badge_->Hide();
  }
}

NostrStatusButton::ConnectionStatus NostrStatusButton::GetConnectionStatus() const {
  if (!nostr_service_) {
    return ConnectionStatus::kDisabled;
  }
  
  // Check if local relay is enabled and running
  bool local_relay_enabled = nostr_service_->IsLocalRelayEnabled();
  if (!local_relay_enabled) {
    return ConnectionStatus::kDisabled;
  }
  
  // Get local relay status
  base::Value::Dict relay_status = nostr_service_->GetLocalRelayStatus();
  const std::string* status = relay_status.FindString("status");
  
  if (!status || *status == "disconnected") {
    return ConnectionStatus::kDisconnected;
  } else if (*status == "degraded") {
    return ConnectionStatus::kDegraded;
  } else if (*status == "connected") {
    return ConnectionStatus::kConnected;
  }
  
  return ConnectionStatus::kDisconnected;
}

int NostrStatusButton::GetNotificationCount() const {
  if (!nostr_service_) {
    return 0;
  }
  
  int count = 0;
  
  // Check for permission requests
  // TODO: Add permission manager integration when available
  
  // Check for system alerts
  // TODO: Add system alert checking when alert system is implemented
  
  // Check for unread events
  // TODO: Add unread event counting when event system is implemented
  
  return count;
}

void NostrStatusButton::ButtonPressed(const ui::Event& event) {
  ShowDropDownMenu(ui::MENU_SOURCE_MOUSE);
}

std::unique_ptr<NostrStatusMenuModel> NostrStatusButton::CreateMenuModel() {
  return std::make_unique<NostrStatusMenuModel>(this, browser_);
}

BEGIN_METADATA(NostrStatusButton)
END_METADATA