// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nostr/nostr_settings_ui.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/nostr/nostr_permission_manager.h"
#include "chrome/browser/nostr/nostr_permission_manager_factory.h"
#include "chrome/browser/nostr/nostr_service.h"
#include "chrome/browser/nostr/nostr_service_factory.h"
#include "chrome/common/tungsten_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace tungsten {

namespace {

using ::testing::_;
using ::testing::Return;

// Mock NostrService for testing
class MockNostrService : public nostr::NostrService {
 public:
  explicit MockNostrService(Profile* profile) : NostrService(profile) {}
  ~MockNostrService() override = default;

  MOCK_METHOD(std::string, GetPublicKey, (), (override));
  MOCK_METHOD(base::Value::Dict, GetCurrentAccount, (), (override));
  MOCK_METHOD(base::Value::List, ListAccounts, (), (override));
  MOCK_METHOD(bool, SwitchAccount, (const std::string&), (override));
};

// Mock NostrPermissionManager for testing
class MockNostrPermissionManager : public nostr::NostrPermissionManager {
 public:
  explicit MockNostrPermissionManager(Profile* profile) 
      : NostrPermissionManager(profile) {}
  ~MockNostrPermissionManager() override = default;

  MOCK_METHOD(std::vector<nostr::NIP07Permission>, GetAllPermissions, (), 
              (override));
  MOCK_METHOD(std::optional<nostr::NIP07Permission>, GetPermission,
              (const url::Origin&), (override));
  MOCK_METHOD(GrantResult, GrantPermission,
              (const url::Origin&, const nostr::NIP07Permission&), (override));
};

}  // namespace

class NostrSettingsUITest : public testing::Test {
 public:
  NostrSettingsUITest() = default;
  ~NostrSettingsUITest() override = default;

  void SetUp() override {
    profile_ = TestingProfile::Builder().Build();
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(nullptr);
    
    // Create the settings UI
    settings_ui_ = std::make_unique<NostrSettingsUI>(web_ui_.get());
    
    // Get the handler that was added
    auto handlers = web_ui_->GetHandlers();
    ASSERT_EQ(1U, handlers.size());
    handler_ = handlers[0];
    handler_->SetJavaScriptAllowed(true);
    handler_->RegisterMessages();
  }

  void TearDown() override {
    settings_ui_.reset();
    web_ui_.reset();
    profile_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<NostrSettingsUI> settings_ui_;
  raw_ptr<content::WebUIMessageHandler> handler_;
};

TEST_F(NostrSettingsUITest, Constructor) {
  // Verify the UI was created successfully
  EXPECT_TRUE(settings_ui_);
}

TEST_F(NostrSettingsUITest, GetNostrEnabled) {
  base::Value::List args;
  args.Append("callback-id");
  
  web_ui_->HandleReceivedMessage("getNostrEnabled", args);
  
  // Check that a response was sent
  const content::TestWebUI::CallData& call_data = 
      web_ui_->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  ASSERT_EQ(2U, call_data.arg_list().size());
  EXPECT_EQ("callback-id", call_data.arg_list()[0]->GetString());
  EXPECT_TRUE(call_data.arg_list()[1]->is_bool());
}

TEST_F(NostrSettingsUITest, SetNostrEnabled) {
  base::Value::List args;
  args.Append("callback-id");
  args.Append(true);
  
  web_ui_->HandleReceivedMessage("setNostrEnabled", args);
  
  // Check that a response was sent
  const content::TestWebUI::CallData& call_data = 
      web_ui_->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  ASSERT_EQ(2U, call_data.arg_list().size());
  EXPECT_EQ("callback-id", call_data.arg_list()[0]->GetString());
  EXPECT_TRUE(call_data.arg_list()[1]->GetBool());
}

TEST_F(NostrSettingsUITest, GetLocalRelayConfig) {
  base::Value::List args;
  args.Append("callback-id");
  
  web_ui_->HandleReceivedMessage("getLocalRelayConfig", args);
  
  // Check that a response was sent
  const content::TestWebUI::CallData& call_data = 
      web_ui_->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  ASSERT_EQ(2U, call_data.arg_list().size());
  EXPECT_EQ("callback-id", call_data.arg_list()[0]->GetString());
  ASSERT_TRUE(call_data.arg_list()[1]->is_dict());
  
  const base::Value::Dict& config = call_data.arg_list()[1]->GetDict();
  EXPECT_TRUE(config.contains("enabled"));
  EXPECT_TRUE(config.contains("port"));
  EXPECT_TRUE(config.contains("interface"));
}

TEST_F(NostrSettingsUITest, SetLocalRelayConfig) {
  base::Value::List args;
  args.Append("callback-id");
  base::Value::Dict config;
  config.Set("enabled", true);
  config.Set("port", 7777);
  config.Set("interface", "127.0.0.1");
  args.Append(std::move(config));
  
  web_ui_->HandleReceivedMessage("setLocalRelayConfig", args);
  
  // Check that a response was sent
  const content::TestWebUI::CallData& call_data = 
      web_ui_->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  ASSERT_EQ(2U, call_data.arg_list().size());
  EXPECT_EQ("callback-id", call_data.arg_list()[0]->GetString());
  EXPECT_TRUE(call_data.arg_list()[1]->GetBool());
}

}  // namespace tungsten