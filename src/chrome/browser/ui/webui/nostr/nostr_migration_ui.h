// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NOSTR_NOSTR_MIGRATION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NOSTR_NOSTR_MIGRATION_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace content {
class WebUI;
}

// WebUI controller for chrome://nostr-migration
class NostrMigrationUI : public content::WebUIController {
 public:
  explicit NostrMigrationUI(content::WebUI* web_ui);
  ~NostrMigrationUI() override;

  NostrMigrationUI(const NostrMigrationUI&) = delete;
  NostrMigrationUI& operator=(const NostrMigrationUI&) = delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NOSTR_NOSTR_MIGRATION_UI_H_