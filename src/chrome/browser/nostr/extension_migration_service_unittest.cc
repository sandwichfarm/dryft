// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/extension_migration_service.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {

class ExtensionMigrationServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_ = std::make_unique<TestingProfile>();
    service_ = std::make_unique<ExtensionMigrationService>(profile_.get());
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ExtensionMigrationService> service_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ExtensionMigrationServiceTest, DetectInstalledExtensionsEmptyDirectory) {
  auto extensions = service_->DetectInstalledExtensions();
  EXPECT_TRUE(extensions.empty());
}

TEST_F(ExtensionMigrationServiceTest, DetectExtensionTypeFromId) {
  EXPECT_EQ(ExtensionMigrationService::GetExtensionTypeFromId("genadeldhhhjdfjjkbjcidgklfjgacjjp"),
            DetectedExtension::Type::kAlby);
  EXPECT_EQ(ExtensionMigrationService::GetExtensionTypeFromId("ldekhppnaggiejfmfafamhcgfgklgfnj"),
            DetectedExtension::Type::kNostr);
  EXPECT_EQ(ExtensionMigrationService::GetExtensionTypeFromId("ebegadccbenicbmjjnjbgbmhfokeidhj"),
            DetectedExtension::Type::kNostrConnect);
  EXPECT_EQ(ExtensionMigrationService::GetExtensionTypeFromId("bgpkdjpdkfkjjjdpbblkbmhbefhallmj"),
            DetectedExtension::Type::kFlamingo);
  EXPECT_EQ(ExtensionMigrationService::GetExtensionTypeFromId("unknown-extension-id"),
            DetectedExtension::Type::kUnknown);
}

TEST_F(ExtensionMigrationServiceTest, ReadExtensionDataInvalidPath) {
  DetectedExtension extension;
  extension.type = DetectedExtension::Type::kAlby;
  extension.id = "test-id";
  extension.storage_path = base::FilePath("/nonexistent/path");
  
  auto data = service_->ReadExtensionData(extension);
  EXPECT_FALSE(data.success);
  EXPECT_FALSE(data.error_message.empty());
}

TEST_F(ExtensionMigrationServiceTest, DisableExtensionInvalidPath) {
  DetectedExtension extension;
  extension.type = DetectedExtension::Type::kAlby;
  extension.id = "test-id";
  extension.storage_path = base::FilePath("/nonexistent/path");
  
  // Should not crash or throw
  service_->DisableExtension(extension);
}

}  // namespace nostr