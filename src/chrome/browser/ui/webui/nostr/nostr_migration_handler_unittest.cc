// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nostr/nostr_migration_handler.h"

#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/nostr/extension_migration_service.h"
#include "chrome/browser/nostr/extension_migration_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace {

class MockExtensionMigrationService : public nostr::ExtensionMigrationService {
 public:
  explicit MockExtensionMigrationService(Profile* profile) 
      : ExtensionMigrationService(profile) {}
  
  MOCK_METHOD(std::vector<nostr::DetectedExtension>, DetectInstalledExtensions, (), (override));
  MOCK_METHOD(nostr::MigrationData, ReadExtensionData, (const nostr::DetectedExtension&), (override));
  MOCK_METHOD(void, MigrateFromExtension, 
              (const nostr::DetectedExtension&, const nostr::MigrationData&, 
               MigrationProgressCallback, MigrationCompleteCallback), (override));
  MOCK_METHOD(void, DisableExtension, (const nostr::DetectedExtension&), (override));
};

class NostrMigrationHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(
        content::WebContents::Create(content::WebContents::CreateParams(profile_.get())));
    
    handler_ = std::make_unique<NostrMigrationHandler>();
    handler_->set_web_ui(web_ui_.get());
    
    // Create mock service
    mock_service_ = std::make_unique<MockExtensionMigrationService>(profile_.get());
  }

  void TearDown() override {
    handler_.reset();
    web_ui_.reset();
    profile_.reset();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<NostrMigrationHandler> handler_;
  std::unique_ptr<MockExtensionMigrationService> mock_service_;
};

TEST_F(NostrMigrationHandlerTest, HandleDetectExtensionsSuccess) {
  // Setup mock to return one extension
  nostr::DetectedExtension extension;
  extension.type = nostr::DetectedExtension::Type::kAlby;
  extension.id = "test-extension-id";
  extension.name = "Test Extension";
  extension.version = "1.0.0";
  extension.is_enabled = true;
  
  std::vector<nostr::DetectedExtension> extensions = {extension};
  
  EXPECT_CALL(*mock_service_, DetectInstalledExtensions())
      .WillOnce(Return(extensions));
  
  // Override the factory to return our mock
  nostr::ExtensionMigrationServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(),
      base::BindRepeating([](MockExtensionMigrationService* service, content::BrowserContext*) 
                          -> std::unique_ptr<KeyedService> {
        return std::unique_ptr<KeyedService>(service);
      }, mock_service_.get()));
  
  // Create arguments
  base::Value::List args;
  args.Append("test-callback-id");
  
  // Call handler
  handler_->HandleDetectExtensions(args);
  
  // Verify the callback was resolved with the expected data
  EXPECT_EQ(1U, web_ui_->call_data().size());
  const auto& call_data = web_ui_->call_data()[0];
  EXPECT_EQ("cr.webUIResponse", call_data->function_name());
  
  const base::Value::List& response_args = call_data->arg1()->GetList();
  EXPECT_EQ(3U, response_args.size());
  EXPECT_EQ("test-callback-id", response_args[0].GetString());
  EXPECT_TRUE(response_args[1].GetBool()); // success
  
  const base::Value::List& result_list = response_args[2].GetList();
  EXPECT_EQ(1U, result_list.size());
  
  const base::Value::Dict& result_dict = result_list[0].GetDict();
  EXPECT_EQ("test-extension-id", *result_dict.FindString("id"));
  EXPECT_EQ("Test Extension", *result_dict.FindString("name"));
}

TEST_F(NostrMigrationHandlerTest, HandleReadExtensionDataSuccess) {
  // Setup mock extension and data
  nostr::DetectedExtension extension;
  extension.type = nostr::DetectedExtension::Type::kAlby;
  extension.id = "test-extension-id";
  
  nostr::MigrationData data;
  data.success = true;
  data.keys.push_back({"Test Key", "test-private-key-hex", true});
  data.relay_urls.push_back("wss://relay.example.com");
  
  EXPECT_CALL(*mock_service_, ReadExtensionData(_))
      .WillOnce(Return(data));
  
  // Override the factory to return our mock
  nostr::ExtensionMigrationServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(),
      base::BindRepeating([](MockExtensionMigrationService* service, content::BrowserContext*) 
                          -> std::unique_ptr<KeyedService> {
        return std::unique_ptr<KeyedService>(service);
      }, mock_service_.get()));
  
  // Create arguments
  base::Value::List args;
  args.Append("test-callback-id");
  
  base::Value::Dict extension_dict;
  extension_dict.Set("type", static_cast<int>(extension.type));
  extension_dict.Set("id", extension.id);
  args.Append(std::move(extension_dict));
  
  // Call handler
  handler_->HandleReadExtensionData(args);
  
  // Verify the callback was resolved
  EXPECT_EQ(1U, web_ui_->call_data().size());
  const auto& call_data = web_ui_->call_data()[0];
  EXPECT_EQ("cr.webUIResponse", call_data->function_name());
  
  const base::Value::List& response_args = call_data->arg1()->GetList();
  EXPECT_EQ(3U, response_args.size());
  EXPECT_EQ("test-callback-id", response_args[0].GetString());
  EXPECT_TRUE(response_args[1].GetBool()); // success
  
  const base::Value::Dict& result_dict = response_args[2].GetDict();
  EXPECT_TRUE(result_dict.FindBool("success").value_or(false));
  
  const base::Value::List* keys_list = result_dict.FindList("keys");
  EXPECT_TRUE(keys_list);
  EXPECT_EQ(1U, keys_list->size());
  
  const base::Value::List* relays_list = result_dict.FindList("relayUrls");
  EXPECT_TRUE(relays_list);
  EXPECT_EQ(1U, relays_list->size());
}

TEST_F(NostrMigrationHandlerTest, HandlePerformMigrationInvalidData) {
  // Create arguments with missing extension field
  base::Value::List args;
  args.Append("test-callback-id");
  
  base::Value::Dict migration_dict;
  // Intentionally omit "extension" field to trigger error
  base::Value::Dict data_dict;
  data_dict.Set("success", true);
  migration_dict.Set("data", std::move(data_dict));
  
  args.Append(std::move(migration_dict));
  
  // Call handler
  handler_->HandlePerformMigration(args);
  
  // Verify the callback was rejected with specific error
  EXPECT_EQ(1U, web_ui_->call_data().size());
  const auto& call_data = web_ui_->call_data()[0];
  EXPECT_EQ("cr.webUIResponse", call_data->function_name());
  
  const base::Value::List& response_args = call_data->arg1()->GetList();
  EXPECT_EQ(3U, response_args.size());
  EXPECT_EQ("test-callback-id", response_args[0].GetString());
  EXPECT_FALSE(response_args[1].GetBool()); // failure
  
  const std::string& error_message = response_args[2].GetString();
  EXPECT_TRUE(error_message.find("missing 'extension' field") != std::string::npos);
}

TEST_F(NostrMigrationHandlerTest, HandlePerformMigrationMissingBothFields) {
  // Create arguments with missing both extension and data fields
  base::Value::List args;
  args.Append("test-callback-id");
  
  base::Value::Dict migration_dict;
  // Intentionally omit both "extension" and "data" fields
  args.Append(std::move(migration_dict));
  
  // Call handler
  handler_->HandlePerformMigration(args);
  
  // Verify the callback was rejected with specific error
  EXPECT_EQ(1U, web_ui_->call_data().size());
  const auto& call_data = web_ui_->call_data()[0];
  EXPECT_EQ("cr.webUIResponse", call_data->function_name());
  
  const base::Value::List& response_args = call_data->arg1()->GetList();
  EXPECT_EQ(3U, response_args.size());
  EXPECT_EQ("test-callback-id", response_args[0].GetString());
  EXPECT_FALSE(response_args[1].GetBool()); // failure
  
  const std::string& error_message = response_args[2].GetString();
  EXPECT_TRUE(error_message.find("missing 'extension' field") != std::string::npos);
  EXPECT_TRUE(error_message.find("missing 'data' field") != std::string::npos);
}

TEST_F(NostrMigrationHandlerTest, HandleDisableExtensionSuccess) {
  // Setup mock
  EXPECT_CALL(*mock_service_, DisableExtension(_))
      .Times(1);
  
  // Override the factory to return our mock
  nostr::ExtensionMigrationServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(),
      base::BindRepeating([](MockExtensionMigrationService* service, content::BrowserContext*) 
                          -> std::unique_ptr<KeyedService> {
        return std::unique_ptr<KeyedService>(service);
      }, mock_service_.get()));
  
  // Create arguments
  base::Value::List args;
  args.Append("test-callback-id");
  
  base::Value::Dict extension_dict;
  extension_dict.Set("type", static_cast<int>(nostr::DetectedExtension::Type::kAlby));
  extension_dict.Set("id", "test-extension-id");
  args.Append(std::move(extension_dict));
  
  // Call handler
  handler_->HandleDisableExtension(args);
  
  // Verify the callback was resolved
  EXPECT_EQ(1U, web_ui_->call_data().size());
  const auto& call_data = web_ui_->call_data()[0];
  EXPECT_EQ("cr.webUIResponse", call_data->function_name());
  
  const base::Value::List& response_args = call_data->arg1()->GetList();
  EXPECT_EQ(3U, response_args.size());
  EXPECT_EQ("test-callback-id", response_args[0].GetString());
  EXPECT_TRUE(response_args[1].GetBool()); // success
  EXPECT_TRUE(response_args[2].GetBool()); // result value
}

}  // namespace