// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NOSTR_NOSTR_MIGRATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NOSTR_NOSTR_MIGRATION_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/nostr/extension_migration_service.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class Value;
class ListValue;
}  // namespace base

// Message handler for the Nostr migration WebUI
class NostrMigrationHandler : public content::WebUIMessageHandler {
 public:
  NostrMigrationHandler();
  ~NostrMigrationHandler() override;

  // WebUIMessageHandler implementation
  void RegisterMessages() override;

 private:
  // Message handlers
  void HandleDetectExtensions(const base::Value::List& args);
  void HandleReadExtensionData(const base::Value::List& args);
  void HandlePerformMigration(const base::Value::List& args);
  void HandleDisableExtension(const base::Value::List& args);
  
  // Callbacks
  void OnMigrationProgress(const std::string& callback_id,
                          int items_completed,
                          int total_items,
                          const std::string& current_item);
  void OnMigrationComplete(const std::string& callback_id,
                          bool success,
                          const std::string& message);
  
  // Helper to convert DetectedExtension to Value
  base::Value::Dict ExtensionToDict(const nostr::DetectedExtension& extension);
  
  // Helper to convert MigrationData to Value
  base::Value::Dict MigrationDataToDict(const nostr::MigrationData& data);
  
  // Helper to convert Value to DetectedExtension
  nostr::DetectedExtension DictToExtension(const base::Value::Dict& dict);
  
  base::WeakPtrFactory<NostrMigrationHandler> weak_factory_{this};
  
  DISALLOW_COPY_AND_ASSIGN(NostrMigrationHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NOSTR_NOSTR_MIGRATION_HANDLER_H_