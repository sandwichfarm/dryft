// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_webui_configs.h"

#include <memory>

#include "build/build_config.h"
#include "content/public/browser/webui_config_map.h"

#if BUILDFLAG(ENABLE_NOSTR)
#include "chrome/browser/ui/webui/nostr/nostr_settings_ui_config.h"
#include "chrome/browser/ui/webui/nostr/nostr_migration_ui_config.h"
#endif

void RegisterChromeWebUIConfigs() {
  auto& map = content::WebUIConfigMap::GetInstance();

#if BUILDFLAG(ENABLE_NOSTR)
  // Register Nostr-related WebUI pages
  map.AddWebUIConfig(std::make_unique<tungsten::NostrSettingsUIConfig>());
  map.AddWebUIConfig(std::make_unique<tungsten::NostrMigrationUIConfig>());
#endif

  // Add other WebUIConfig registrations here as needed
}