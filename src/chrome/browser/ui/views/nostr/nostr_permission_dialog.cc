// Copyright 2024 The Chromium Authors and Alex313031
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/nostr/nostr_permission_dialog.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"

namespace {

constexpr int kDialogWidth = 400;
constexpr int kProgressBarHeight = 4;

}  // namespace

// static
void NostrPermissionDialog::Show(views::View* anchor_view,
                                 const NostrPermissionRequest& request,
                                 PermissionCallback callback) {
  auto dialog = std::make_unique<NostrPermissionDialog>(
      anchor_view, request, std::move(callback));
  views::BubbleDialogDelegateView::CreateBubble(std::move(dialog))->Show();
}

NostrPermissionDialog::NostrPermissionDialog(views::View* anchor_view,
                                             const NostrPermissionRequest& request,
                                             PermissionCallback callback)
    : views::BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
      request_(request),
      callback_(std::move(callback)),
      dialog_start_time_(base::TimeTicks::Now()) {
  
  SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, l10n_util::GetStringUTF16(IDS_NOSTR_PERMISSION_ALLOW));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, l10n_util::GetStringUTF16(IDS_NOSTR_PERMISSION_DENY));
  
  SetAcceptCallback(base::BindOnce(&NostrPermissionDialog::OnAccept,
                                   base::Unretained(this)));
  SetCancelCallback(base::BindOnce(&NostrPermissionDialog::OnCancel,
                                   base::Unretained(this)));
  
  set_fixed_width(kDialogWidth);
  set_margins(ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
  
  InitializeLayout();
  
  // Start timeout timer
  timeout_timer_.Start(FROM_HERE, base::Milliseconds(100),
                      base::BindRepeating(&NostrPermissionDialog::UpdateTimeoutDisplay,
                                         base::Unretained(this)));
}

NostrPermissionDialog::~NostrPermissionDialog() {
  // Ensure callback is called if dialog is closed without user action
  if (!resolved_ && callback_) {
    std::move(callback_).Run(false, false);  // Auto-deny
  }
}

std::u16string NostrPermissionDialog::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_NOSTR_PERMISSION_DIALOG_TITLE);
}

void NostrPermissionDialog::OnThemeChanged() {
  views::BubbleDialogDelegateView::OnThemeChanged();
  
  // Update timeout progress bar color if needed
  if (timeout_progress_) {
    // Progress bar will use theme colors automatically
  }
}

bool NostrPermissionDialog::ShouldShowCloseButton() const {
  return false;  // Use Allow/Deny buttons instead
}

void NostrPermissionDialog::InitializeLayout() {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  const int column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                       0.0, views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddPaddingColumn(0.0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                                       views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                       1.0, views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  CreateOriginSection();
  CreateMethodSection();
  CreateDetailsSection();
  CreateRememberSection();
  CreateTimeoutSection();
}

void NostrPermissionDialog::CreateOriginSection() {
  auto* layout = static_cast<views::GridLayout*>(GetLayoutManager());
  
  layout->StartRow(0.0, 0);
  
  // Icon for the operation
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(ui::ImageModel::FromVectorIcon(
      GetOperationIcon(), ui::kColorIcon, 24));
  layout->AddView(std::move(icon_view));
  
  // Origin display
  origin_label_ = layout->AddView(std::make_unique<views::Label>(
      base::UTF8ToUTF16(request_.origin.Serialize())));
  origin_label_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  origin_label_->SetTextStyle(views::style::STYLE_PRIMARY);
  origin_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  
  layout->AddPaddingRow(0.0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_RELATED_CONTROL_VERTICAL));
}

void NostrPermissionDialog::CreateMethodSection() {
  auto* layout = static_cast<views::GridLayout*>(GetLayoutManager());
  
  layout->StartRow(0.0, 0);
  layout->SkipColumns(1);  // Skip icon column
  
  method_label_ = layout->AddView(std::make_unique<views::Label>(
      GetMethodDisplayText()));
  method_label_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  method_label_->SetTextStyle(views::style::STYLE_SECONDARY);
  method_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  method_label_->SetMultiLine(true);
  
  layout->AddPaddingRow(0.0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_RELATED_CONTROL_VERTICAL));
}

void NostrPermissionDialog::CreateDetailsSection() {
  auto* layout = static_cast<views::GridLayout*>(GetLayoutManager());
  
  std::u16string details_text = GetDetailsDisplayText();
  if (!details_text.empty()) {
    layout->StartRow(0.0, 0);
    layout->SkipColumns(1);  // Skip icon column
    
    details_label_ = layout->AddView(std::make_unique<views::Label>(details_text));
    details_label_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
    details_label_->SetTextStyle(views::style::STYLE_SECONDARY);
    details_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    details_label_->SetMultiLine(true);
    
    layout->AddPaddingRow(0.0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                                  views::DISTANCE_RELATED_CONTROL_VERTICAL));
  }
}

void NostrPermissionDialog::CreateRememberSection() {
  auto* layout = static_cast<views::GridLayout*>(GetLayoutManager());
  
  layout->StartRow(0.0, 0);
  layout->SkipColumns(1);  // Skip icon column
  
  remember_checkbox_ = layout->AddView(std::make_unique<views::Checkbox>(
      l10n_util::GetStringUTF16(IDS_NOSTR_PERMISSION_REMEMBER)));
  remember_checkbox_->SetChecked(false);
  
  layout->AddPaddingRow(0.0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_RELATED_CONTROL_VERTICAL));
}

void NostrPermissionDialog::CreateTimeoutSection() {
  auto* layout = static_cast<views::GridLayout*>(GetLayoutManager());
  
  layout->StartRow(0.0, 0);
  layout->SkipColumns(1);  // Skip icon column
  
  // Container for timeout UI
  auto timeout_container = std::make_unique<views::View>();
  timeout_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 4));
  
  // Timeout label
  timeout_label_ = timeout_container->AddChildView(
      std::make_unique<views::Label>(std::u16string()));
  timeout_label_->SetTextContext(views::style::CONTEXT_LABEL);
  timeout_label_->SetTextStyle(views::style::STYLE_HINT);
  timeout_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  
  // Progress bar
  timeout_progress_ = timeout_container->AddChildView(
      std::make_unique<views::ProgressBar>(kProgressBarHeight));
  timeout_progress_->SetValue(1.0);  // Start at full
  
  layout->AddView(std::move(timeout_container));
}

void NostrPermissionDialog::UpdateTimeoutDisplay() {
  if (resolved_) {
    timeout_timer_.Stop();
    return;
  }
  
  base::TimeDelta elapsed = base::TimeTicks::Now() - dialog_start_time_;
  base::TimeDelta remaining = kTimeoutDuration - elapsed;
  
  if (remaining <= base::TimeDelta()) {
    OnTimeout();
    return;
  }
  
  // Update progress bar (1.0 = full, 0.0 = empty)
  double progress = remaining.InSecondsF() / kTimeoutDuration.InSecondsF();
  timeout_progress_->SetValue(progress);
  
  // Update label
  int seconds_remaining = static_cast<int>(remaining.InSeconds()) + 1;
  timeout_label_->SetText(l10n_util::GetStringFUTF16(
      IDS_NOSTR_PERMISSION_TIMEOUT, base::NumberToString16(seconds_remaining)));
}

void NostrPermissionDialog::OnTimeout() {
  if (resolved_) {
    return;
  }
  
  resolved_ = true;
  timeout_timer_.Stop();
  
  if (callback_) {
    std::move(callback_).Run(false, false);  // Auto-deny on timeout
  }
  
  GetWidget()->Close();
}

void NostrPermissionDialog::OnAccept() {
  if (resolved_) {
    return;
  }
  
  resolved_ = true;
  timeout_timer_.Stop();
  
  bool remember = remember_checkbox_ ? remember_checkbox_->GetChecked() : false;
  
  if (callback_) {
    std::move(callback_).Run(true, remember);  // User granted permission
  }
}

void NostrPermissionDialog::OnCancel() {
  if (resolved_) {
    return;
  }
  
  resolved_ = true;
  timeout_timer_.Stop();
  
  bool remember = remember_checkbox_ ? remember_checkbox_->GetChecked() : false;
  
  if (callback_) {
    std::move(callback_).Run(false, remember);  // User denied permission
  }
}

std::u16string NostrPermissionDialog::GetMethodDisplayText() const {
  if (request_.method == "getPublicKey") {
    return l10n_util::GetStringUTF16(IDS_NOSTR_PERMISSION_GET_PUBLIC_KEY);
  } else if (request_.method == "signEvent") {
    return l10n_util::GetStringUTF16(IDS_NOSTR_PERMISSION_SIGN_EVENT);
  } else if (request_.method == "nip04_encrypt") {
    return l10n_util::GetStringUTF16(IDS_NOSTR_PERMISSION_NIP04_ENCRYPT);
  } else if (request_.method == "nip04_decrypt") {
    return l10n_util::GetStringUTF16(IDS_NOSTR_PERMISSION_NIP04_DECRYPT);
  } else if (request_.method == "nip44_encrypt") {
    return l10n_util::GetStringUTF16(IDS_NOSTR_PERMISSION_NIP44_ENCRYPT);
  } else if (request_.method == "nip44_decrypt") {
    return l10n_util::GetStringUTF16(IDS_NOSTR_PERMISSION_NIP44_DECRYPT);
  }
  
  return base::UTF8ToUTF16(request_.method);
}

std::u16string NostrPermissionDialog::GetDetailsDisplayText() const {
  // Show additional details for signing operations
  if (request_.method == "signEvent") {
    const base::Value::Dict* event = request_.details.FindDict("event");
    if (event) {
      const std::string* content = event->FindString("content");
      if (content && !content->empty()) {
        return l10n_util::GetStringFUTF16(
            IDS_NOSTR_PERMISSION_EVENT_CONTENT, 
            base::UTF8ToUTF16(*content));
      }
      
      std::optional<int> kind = event->FindInt("kind");
      if (kind.has_value()) {
        return l10n_util::GetStringFUTF16(
            IDS_NOSTR_PERMISSION_EVENT_KIND,
            base::NumberToString16(kind.value()));
      }
    }
  }
  
  return std::u16string();
}

const gfx::VectorIcon& NostrPermissionDialog::GetOperationIcon() const {
  if (request_.method == "getPublicKey") {
    return vector_icons::kAccountCircleIcon;
  } else if (request_.method == "signEvent") {
    return vector_icons::kEditIcon;
  } else if (request_.method.find("encrypt") != std::string::npos) {
    return vector_icons::kLockIcon;
  } else if (request_.method.find("decrypt") != std::string::npos) {
    return vector_icons::kLockOpenIcon;
  }
  
  return vector_icons::kSecurityIcon;
}

BEGIN_METADATA(NostrPermissionDialog)
END_METADATA