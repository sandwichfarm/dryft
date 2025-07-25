// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_webui_configs.h"

#include "chrome/browser/ui/webui/nostr/nostr_settings_ui_config.h"
#include "chrome/common/tungsten_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ChromeWebUIConfigsTest : public testing::Test {
 public:
  ChromeWebUIConfigsTest() = default;
  ~ChromeWebUIConfigsTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(ChromeWebUIConfigsTest, NostrSettingsUIConfigIsRegistered) {
  // Register configs
  RegisterChromeWebUIConfigs();
  
  // Check that NostrSettingsUIConfig is registered
  GURL nostr_settings_url(dryft::kChromeUINostrSettingsURL);
  auto& config_map = content::WebUIConfigMap::GetInstance();
  
  // GetConfig returns a WebUIConfig* if registered, nullptr otherwise
  auto* config = config_map.GetConfig(&profile_, nostr_settings_url);
  EXPECT_NE(nullptr, config);
  
  // Verify it's the right type (optional, but good practice)
  if (config) {
    // Check that the scheme and host match
    EXPECT_EQ(content::kChromeUIScheme, config->scheme());
    EXPECT_EQ(dryft::kChromeUINostrSettingsHost, config->host());
  }
}