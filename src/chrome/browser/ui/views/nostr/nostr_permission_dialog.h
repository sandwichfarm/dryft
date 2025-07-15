// Copyright 2024 The Chromium Authors and Alex313031
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NOSTR_NOSTR_PERMISSION_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_NOSTR_NOSTR_PERMISSION_DIALOG_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/nostr/nostr_messages.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "url/origin.h"

namespace views {
class Checkbox;
class Label;
class ProgressBar;
}

// Dialog that requests permission for NIP-07 operations from the user.
// Shows origin, operation details, remember option, and auto-denies after timeout.
class NostrPermissionDialog : public views::BubbleDialogDelegateView {
  METADATA_HEADER(NostrPermissionDialog, views::BubbleDialogDelegateView)

 public:
  // Callback for permission decision: (granted, remember_decision)
  using PermissionCallback = base::OnceCallback<void(bool, bool)>;

  // Creates and shows a permission dialog
  static void Show(views::View* anchor_view,
                   const NostrPermissionRequest& request,
                   PermissionCallback callback);

  NostrPermissionDialog(views::View* anchor_view,
                        const NostrPermissionRequest& request,
                        PermissionCallback callback);
  NostrPermissionDialog(const NostrPermissionDialog&) = delete;
  NostrPermissionDialog& operator=(const NostrPermissionDialog&) = delete;
  ~NostrPermissionDialog() override;

  // views::BubbleDialogDelegateView:
  std::u16string GetWindowTitle() const override;
  void OnThemeChanged() override;
  bool ShouldShowCloseButton() const override;

 private:
  void InitializeLayout();
  void CreateOriginSection();
  void CreateMethodSection();
  void CreateDetailsSection();
  void CreateRememberSection();
  void CreateTimeoutSection();
  
  // Updates the timeout progress bar and label
  void UpdateTimeoutDisplay();
  
  // Called when timeout expires - auto-denies the request
  void OnTimeout();
  
  // Called when user clicks Allow button
  void OnAccept();
  
  // Called when user clicks Deny button  
  void OnCancel();
  
  // Gets display text for the requested method
  std::u16string GetMethodDisplayText() const;
  
  // Gets display text for operation details
  std::u16string GetDetailsDisplayText() const;
  
  // Gets icon for the requested operation
  const gfx::VectorIcon& GetOperationIcon() const;

  // The permission request details
  NostrPermissionRequest request_;
  
  // Callback to invoke with user's decision
  PermissionCallback callback_;
  
  // UI components
  raw_ptr<views::Label> origin_label_ = nullptr;
  raw_ptr<views::Label> method_label_ = nullptr;
  raw_ptr<views::Label> details_label_ = nullptr;
  raw_ptr<views::Checkbox> remember_checkbox_ = nullptr;
  raw_ptr<views::Label> timeout_label_ = nullptr;
  raw_ptr<views::ProgressBar> timeout_progress_ = nullptr;
  
  // Timeout handling
  base::RepeatingTimer timeout_timer_;
  base::TimeTicks dialog_start_time_;
  static constexpr base::TimeDelta kTimeoutDuration = base::Seconds(30);
  
  // Whether the dialog has been resolved
  bool resolved_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_NOSTR_NOSTR_PERMISSION_DIALOG_H_