// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NOSTR_NOSTR_SETTINGS_UI_CONFIG_H_
#define CHROME_BROWSER_UI_WEBUI_NOSTR_NOSTR_SETTINGS_UI_CONFIG_H_

#include "chrome/browser/ui/webui/nostr/nostr_settings_ui.h"
#include "chrome/common/tungsten_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace tungsten {

// WebUIConfig for chrome://settings-nostr
class NostrSettingsUIConfig : public content::DefaultWebUIConfig<NostrSettingsUI> {
 public:
  NostrSettingsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                          kChromeUINostrSettingsHost) {}
};

}  // namespace tungsten

#endif  // CHROME_BROWSER_UI_WEBUI_NOSTR_NOSTR_SETTINGS_UI_CONFIG_H_