// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/windows/nostr_protocol_associations.h"

#include <windows.h>
#include <shlobj.h>

#include "base/strings/string_util.h"
#include "base/win/registry.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"

namespace installer {

namespace {

// Registry paths
const wchar_t kClassesRoot[] = L"Software\\Classes";
const wchar_t kShellOpenCommand[] = L"shell\\open\\command";
const wchar_t kDefaultIcon[] = L"DefaultIcon";

// Creates registry entries for a URL protocol
bool CreateProtocolEntries(WorkItemList* list,
                          const std::wstring& protocol,
                          const std::wstring& description,
                          const std::wstring& command) {
  std::wstring protocol_key = std::wstring(kClassesRoot) + L"\\" + protocol;
  
  // Create protocol key and set description
  list->AddCreateRegKeyWorkItem(HKEY_CURRENT_USER, protocol_key, KEY_WOW64_32KEY);
  list->AddSetRegValueWorkItem(HKEY_CURRENT_USER, protocol_key, KEY_WOW64_32KEY,
                               L"", description, true);
  
  // Mark as URL protocol
  list->AddSetRegValueWorkItem(HKEY_CURRENT_USER, protocol_key, KEY_WOW64_32KEY,
                               kUrlProtocolValue, L"", true);
  
  // Set default icon
  std::wstring icon_key = protocol_key + L"\\" + kDefaultIcon;
  list->AddCreateRegKeyWorkItem(HKEY_CURRENT_USER, icon_key, KEY_WOW64_32KEY);
  list->AddSetRegValueWorkItem(HKEY_CURRENT_USER, icon_key, KEY_WOW64_32KEY,
                               L"", command + kDefaultIconIndex, true);
  
  // Set open command
  std::wstring command_key = protocol_key + L"\\" + kShellOpenCommand;
  list->AddCreateRegKeyWorkItem(HKEY_CURRENT_USER, command_key, KEY_WOW64_32KEY);
  list->AddSetRegValueWorkItem(HKEY_CURRENT_USER, command_key, KEY_WOW64_32KEY,
                               L"", L"\"" + command + L"\" \"%1\"", true);
  
  return true;
}

// Creates registry entries for a file type association
bool CreateFileTypeEntries(WorkItemList* list,
                          const std::wstring& extension,
                          const std::wstring& prog_id,
                          const std::wstring& description,
                          const std::wstring& mime_type,
                          const std::wstring& command) {
  // Register extension
  std::wstring ext_key = std::wstring(kClassesRoot) + L"\\" + extension;
  list->AddCreateRegKeyWorkItem(HKEY_CURRENT_USER, ext_key, KEY_WOW64_32KEY);
  list->AddSetRegValueWorkItem(HKEY_CURRENT_USER, ext_key, KEY_WOW64_32KEY,
                               L"", prog_id, true);
  list->AddSetRegValueWorkItem(HKEY_CURRENT_USER, ext_key, KEY_WOW64_32KEY,
                               L"Content Type", mime_type, true);
  
  // Register ProgID
  std::wstring prog_id_key = std::wstring(kClassesRoot) + L"\\" + prog_id;
  list->AddCreateRegKeyWorkItem(HKEY_CURRENT_USER, prog_id_key, KEY_WOW64_32KEY);
  list->AddSetRegValueWorkItem(HKEY_CURRENT_USER, prog_id_key, KEY_WOW64_32KEY,
                               L"", description, true);
  
  // Set default icon
  std::wstring icon_key = prog_id_key + L"\\" + kDefaultIcon;
  list->AddCreateRegKeyWorkItem(HKEY_CURRENT_USER, icon_key, KEY_WOW64_32KEY);
  list->AddSetRegValueWorkItem(HKEY_CURRENT_USER, icon_key, KEY_WOW64_32KEY,
                               L"", command + kDefaultIconIndex, true);
  
  // Set open command
  std::wstring command_key = prog_id_key + L"\\" + kShellOpenCommand;
  list->AddCreateRegKeyWorkItem(HKEY_CURRENT_USER, command_key, KEY_WOW64_32KEY);
  list->AddSetRegValueWorkItem(HKEY_CURRENT_USER, command_key, KEY_WOW64_32KEY,
                               L"", L"\"" + command + L"\" \"%1\"", true);
  
  return true;
}

}  // namespace

bool RegisterNostrProtocol(const std::wstring& chrome_exe) {
  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());
  
  if (!CreateProtocolEntries(list.get(), kNostrProtocol, 
                            kNostrProtocolDescription, chrome_exe)) {
    return false;
  }
  
  if (!list->Do()) {
    list->Rollback();
    return false;
  }
  
  // Notify shell of changes
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
  
  return true;
}

bool RegisterNsiteFileType(const std::wstring& chrome_exe) {
  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());
  
  if (!CreateFileTypeEntries(list.get(), kNsiteExtension, kNsiteProgId,
                            kNsiteDescription, kNsiteMimeType, chrome_exe)) {
    return false;
  }
  
  if (!list->Do()) {
    list->Rollback();
    return false;
  }
  
  // Notify shell of changes
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
  
  return true;
}

void UnregisterNostrAssociations() {
  // Remove nostr protocol (use DeleteTree to remove all subkeys)
  std::wstring nostr_key = std::wstring(kClassesRoot) + L"\\" + kNostrProtocol;
  base::win::RegKey::DeleteTree(HKEY_CURRENT_USER, nostr_key, KEY_WOW64_32KEY | KEY_WRITE);
  
  // Remove .nsite extension
  std::wstring nsite_ext_key = std::wstring(kClassesRoot) + L"\\" + kNsiteExtension;
  base::win::RegKey::DeleteTree(HKEY_CURRENT_USER, nsite_ext_key, KEY_WOW64_32KEY | KEY_WRITE);
  
  // Remove Nsite ProgID
  std::wstring nsite_prog_key = std::wstring(kClassesRoot) + L"\\" + kNsiteProgId;
  base::win::RegKey::DeleteTree(HKEY_CURRENT_USER, nsite_prog_key, KEY_WOW64_32KEY | KEY_WRITE);
  
  // Notify shell of changes
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

}  // namespace installer