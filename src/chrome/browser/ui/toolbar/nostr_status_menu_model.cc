// Copyright 2024 The Chromium Authors and Alex313031
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/nostr_status_menu_model.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

NostrStatusMenuModel::NostrStatusMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    Browser* browser)
    : ui::SimpleMenuModel(delegate), browser_(browser) {
  Build();
}

NostrStatusMenuModel::~NostrStatusMenuModel() = default;

void NostrStatusMenuModel::Build() {
  // Status information header
  AddTitle(l10n_util::GetStringUTF16(IDS_NOSTR_STATUS_TITLE));
  
  // Connection status section
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItem(IDC_NOSTR_CONNECTION_DETAILS,
          l10n_util::GetStringUTF16(IDS_NOSTR_CONNECTION_DETAILS));
  
  // Account section
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItem(IDC_NOSTR_ACCOUNT_SWITCHER,
          l10n_util::GetStringUTF16(IDS_NOSTR_ACCOUNT_SWITCHER));
  
  // Settings section
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItem(IDC_NOSTR_TOGGLE_RELAY,
          l10n_util::GetStringUTF16(IDS_NOSTR_TOGGLE_RELAY));
  AddItem(IDC_NOSTR_SETTINGS,
          l10n_util::GetStringUTF16(IDS_NOSTR_SETTINGS));
  AddItem(IDC_NOSTR_MANAGE_KEYS,
          l10n_util::GetStringUTF16(IDS_NOSTR_MANAGE_KEYS));
}