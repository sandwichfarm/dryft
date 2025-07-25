// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nostr/nostr_migration_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/nostr/nostr_migration_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/tungsten_url_constants.h"
#include "chrome/grit/nostr_migration_resources.h"
#include "chrome/grit/nostr_migration_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

NostrMigrationUI::NostrMigrationUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  
  // Create data source
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, dryft::kChromeUINostrMigrationHost);
  
  // Add resources
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kNostrMigrationResources, kNostrMigrationResourcesSize),
      IDR_NOSTR_MIGRATION_MIGRATION_HTML);
  
  // Add strings - TODO: Move to proper localization files
  source->AddString("migrationTitle", "Import from Nostr Extensions");
  source->AddString("scanButton", "Scan for Extensions");
  source->AddString("importButton", "Import");
  source->AddString("cancelButton", "Cancel");
  source->AddString("finishButton", "Finish");
  
  // Add message handler
  web_ui->AddMessageHandler(std::make_unique<NostrMigrationHandler>());
}

NostrMigrationUI::~NostrMigrationUI() = default;