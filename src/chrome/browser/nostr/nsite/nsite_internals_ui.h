// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NSITE_NSITE_INTERNALS_UI_H_
#define CHROME_BROWSER_NOSTR_NSITE_NSITE_INTERNALS_UI_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

namespace content {
class WebUI;
}

namespace nostr {

// WebUI controller for chrome://nsite-internals debug page
class NsiteInternalsUI : public content::WebUIController {
 public:
  explicit NsiteInternalsUI(content::WebUI* web_ui);
  ~NsiteInternalsUI() override;

  NsiteInternalsUI(const NsiteInternalsUI&) = delete;
  NsiteInternalsUI& operator=(const NsiteInternalsUI&) = delete;

 private:
  void HandleGetStatus(const base::Value::List& args);
  void HandleGetCacheStats(const base::Value::List& args);
  void HandleGetPerformanceMetrics(const base::Value::List& args);
  void HandleClearCache(const base::Value::List& args);
  void HandleRestartServer(const base::Value::List& args);

  base::WeakPtrFactory<NsiteInternalsUI> weak_factory_{this};
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NSITE_NSITE_INTERNALS_UI_H_