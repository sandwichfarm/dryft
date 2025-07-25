// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/windows/nostr_protocol_associations.h"

#include <windows.h>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

class NostrProtocolAssociationsTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));
    chrome_exe_ = L"C:\\Program Files\\Tungsten\\Application\\tungsten.exe";
  }

  // Checks if a registry key exists
  bool KeyExists(const std::wstring& key_path) {
    base::win::RegKey key;
    return key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_READ) == ERROR_SUCCESS;
  }

  // Gets a registry string value
  std::wstring GetStringValue(const std::wstring& key_path,
                              const std::wstring& value_name) {
    base::win::RegKey key;
    std::wstring value;
    if (key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_READ) == ERROR_SUCCESS) {
      key.ReadValue(value_name.c_str(), &value);
    }
    return value;
  }

  registry_util::RegistryOverrideManager registry_override_;
  std::wstring chrome_exe_;
};

TEST_F(NostrProtocolAssociationsTest, RegisterNostrProtocol) {
  EXPECT_TRUE(RegisterNostrProtocol(chrome_exe_));
  
  // Check protocol key exists
  EXPECT_TRUE(KeyExists(L"Software\\Classes\\nostr"));
  
  // Check protocol description
  EXPECT_EQ(GetStringValue(L"Software\\Classes\\nostr", L""),
            kNostrProtocolDescription);
  
  // Check URL Protocol marker
  base::win::RegKey key;
  EXPECT_EQ(key.Open(HKEY_CURRENT_USER, L"Software\\Classes\\nostr", KEY_READ),
            ERROR_SUCCESS);
  EXPECT_TRUE(key.HasValue(kUrlProtocolValue));
  
  // Check open command
  std::wstring expected_command = L"\"" + chrome_exe_ + L"\" \"%1\"";
  EXPECT_EQ(GetStringValue(L"Software\\Classes\\nostr\\shell\\open\\command", L""),
            expected_command);
  
  // Check icon
  std::wstring expected_icon = chrome_exe_ + L",1";
  EXPECT_EQ(GetStringValue(L"Software\\Classes\\nostr\\DefaultIcon", L""),
            expected_icon);
}

TEST_F(NostrProtocolAssociationsTest, RegisterNsiteFileType) {
  EXPECT_TRUE(RegisterNsiteFileType(chrome_exe_));
  
  // Check extension key
  EXPECT_TRUE(KeyExists(L"Software\\Classes\\.nsite"));
  EXPECT_EQ(GetStringValue(L"Software\\Classes\\.nsite", L""), kNsiteProgId);
  EXPECT_EQ(GetStringValue(L"Software\\Classes\\.nsite", L"Content Type"),
            kNsiteMimeType);
  
  // Check ProgID key
  EXPECT_TRUE(KeyExists(L"Software\\Classes\\ThoriumNSITE"));
  EXPECT_EQ(GetStringValue(L"Software\\Classes\\ThoriumNSITE", L""),
            kNsiteDescription);
  
  // Check open command
  std::wstring expected_command = L"\"" + chrome_exe_ + L"\" \"%1\"";
  EXPECT_EQ(GetStringValue(L"Software\\Classes\\ThoriumNSITE\\shell\\open\\command", L""),
            expected_command);
  
  // Check icon
  std::wstring expected_icon = chrome_exe_ + L",1";
  EXPECT_EQ(GetStringValue(L"Software\\Classes\\ThoriumNSITE\\DefaultIcon", L""),
            expected_icon);
}

TEST_F(NostrProtocolAssociationsTest, UnregisterNostrAssociations) {
  // First register
  EXPECT_TRUE(RegisterNostrProtocol(chrome_exe_));
  EXPECT_TRUE(RegisterNsiteFileType(chrome_exe_));
  
  // Verify they exist
  EXPECT_TRUE(KeyExists(L"Software\\Classes\\nostr"));
  EXPECT_TRUE(KeyExists(L"Software\\Classes\\.nsite"));
  EXPECT_TRUE(KeyExists(L"Software\\Classes\\ThoriumNSITE"));
  
  // Unregister
  UnregisterNostrAssociations();
  
  // Verify they're gone
  EXPECT_FALSE(KeyExists(L"Software\\Classes\\nostr"));
  EXPECT_FALSE(KeyExists(L"Software\\Classes\\.nsite"));
  EXPECT_FALSE(KeyExists(L"Software\\Classes\\ThoriumNSITE"));
}

}  // namespace installer