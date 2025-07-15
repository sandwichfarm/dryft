// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NOSTR_NOSTR_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NOSTR_NOSTR_SETTINGS_UI_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class WebUI;
}

namespace tungsten {

// WebUI message handler for Nostr settings page
class NostrSettingsHandler : public content::WebUIMessageHandler {
 public:
  NostrSettingsHandler();
  ~NostrSettingsHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // Handle JavaScript requests
  void HandleGetNostrEnabled(const base::Value::List& args);
  void HandleSetNostrEnabled(const base::Value::List& args);
  void HandleGetLocalRelayConfig(const base::Value::List& args);
  void HandleSetLocalRelayConfig(const base::Value::List& args);
  void HandleGetAccounts(const base::Value::List& args);
  void HandleGetPermissions(const base::Value::List& args);
  void HandleSetPermission(const base::Value::List& args);
  
  // Account management handlers
  void HandleGenerateKeys(const base::Value::List& args);
  void HandleCreateAccount(const base::Value::List& args);
  void HandleImportAccount(const base::Value::List& args);
  void HandleSwitchAccount(const base::Value::List& args);
  void HandleDeleteAccount(const base::Value::List& args);
  void HandleExportAccount(const base::Value::List& args);
  
  // Enhanced permission handlers
  void HandleSetPermissionFull(const base::Value::List& args);
  void HandleResetPermission(const base::Value::List& args);
  void HandleDeletePermission(const base::Value::List& args);
  void HandleBulkPermissionAction(const base::Value::List& args);
  
  // Enhanced relay handlers
  void HandleGetLocalRelayStatus(const base::Value::List& args);
  void HandleStartLocalRelay(const base::Value::List& args);
  void HandleStopLocalRelay(const base::Value::List& args);
  void HandleResetLocalRelayConfig(const base::Value::List& args);
  
  // Send response back to JavaScript
  void ResolveJavascriptCallback(const base::Value& callback_id,
                                 const base::Value& response);

  base::WeakPtrFactory<NostrSettingsHandler> weak_factory_{this};
};

// WebUI controller for chrome://settings-nostr page
class NostrSettingsUI : public content::WebUIController {
 public:
  explicit NostrSettingsUI(content::WebUI* web_ui);
  ~NostrSettingsUI() override;

  NostrSettingsUI(const NostrSettingsUI&) = delete;
  NostrSettingsUI& operator=(const NostrSettingsUI&) = delete;
};

}  // namespace tungsten

#endif  // CHROME_BROWSER_UI_WEBUI_NOSTR_NOSTR_SETTINGS_UI_H_