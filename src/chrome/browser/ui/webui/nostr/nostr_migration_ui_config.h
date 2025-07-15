// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NOSTR_NOSTR_MIGRATION_UI_CONFIG_H_
#define CHROME_BROWSER_UI_WEBUI_NOSTR_NOSTR_MIGRATION_UI_CONFIG_H_

#include "chrome/browser/ui/webui/nostr/nostr_migration_ui.h"
#include "chrome/common/tungsten_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace tungsten {

// WebUIConfig for chrome://nostr-migration
class NostrMigrationUIConfig : public content::DefaultWebUIConfig<NostrMigrationUI> {
 public:
  NostrMigrationUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                          kChromeUINostrMigrationHost) {}
};

}  // namespace tungsten

#endif  // CHROME_BROWSER_UI_WEBUI_NOSTR_NOSTR_MIGRATION_UI_CONFIG_H_