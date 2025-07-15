// Copyright 2024 The Chromium Authors and Alex313031
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_NOSTR_STATUS_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_NOSTR_STATUS_MENU_MODEL_H_

#include "ui/base/models/simple_menu_model.h"

class Browser;

namespace ui {
class SimpleMenuModel;
}

// Menu commands for the Nostr status button dropdown
enum NostrStatusMenuCommands {
  IDC_NOSTR_SETTINGS = 1000,
  IDC_NOSTR_MANAGE_KEYS,
  IDC_NOSTR_CONNECTION_DETAILS,
  IDC_NOSTR_TOGGLE_RELAY,
  IDC_NOSTR_ACCOUNT_SWITCHER,
};

// Menu model for the Nostr status button dropdown menu
class NostrStatusMenuModel : public ui::SimpleMenuModel {
 public:
  NostrStatusMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                       Browser* browser);
  NostrStatusMenuModel(const NostrStatusMenuModel&) = delete;
  NostrStatusMenuModel& operator=(const NostrStatusMenuModel&) = delete;
  ~NostrStatusMenuModel() override;

 private:
  void Build();
  
  Browser* browser_;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_NOSTR_STATUS_MENU_MODEL_H_