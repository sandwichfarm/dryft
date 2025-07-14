// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_WEBUI_CONFIGS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_WEBUI_CONFIGS_H_

// Registers all Chrome WebUI configs with the WebUIConfigMap.
// This should be called early during browser initialization on the UI thread.
void RegisterChromeWebUIConfigs();

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_WEBUI_CONFIGS_H_