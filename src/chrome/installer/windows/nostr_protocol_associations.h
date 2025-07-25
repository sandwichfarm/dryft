// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_WINDOWS_NOSTR_PROTOCOL_ASSOCIATIONS_H_
#define CHROME_INSTALLER_WINDOWS_NOSTR_PROTOCOL_ASSOCIATIONS_H_

#include <string>

namespace installer {

// Protocol scheme for Nostr URLs
const wchar_t kNostrProtocol[] = L"nostr";

// File extension for Nostr Site archives
const wchar_t kNsiteExtension[] = L".nsite";

// ProgID for Nsite files
const wchar_t kNsiteProgId[] = L"ThoriumNSITE";

// Description for Nsite files
const wchar_t kNsiteDescription[] = L"Nostr Site Archive";

// MIME type for Nsite files
const wchar_t kNsiteMimeType[] = L"application/x-nsite";

// Registry value indicating a URL protocol
const wchar_t kUrlProtocolValue[] = L"URL Protocol";

// Protocol description
const wchar_t kNostrProtocolDescription[] = L"URL:Nostr Protocol";

// Default icon index in the executable
const wchar_t kDefaultIconIndex[] = L",1";

// Registers the nostr:// protocol handler
bool RegisterNostrProtocol(const std::wstring& chrome_exe);

// Registers the .nsite file association
bool RegisterNsiteFileType(const std::wstring& chrome_exe);

// Unregisters Nostr protocol and file associations
void UnregisterNostrAssociations();

}  // namespace installer

#endif  // CHROME_INSTALLER_WINDOWS_NOSTR_PROTOCOL_ASSOCIATIONS_H_