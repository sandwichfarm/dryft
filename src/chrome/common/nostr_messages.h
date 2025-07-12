// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NOSTR_MESSAGES_H_
#define CHROME_COMMON_NOSTR_MESSAGES_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START NostrMsgStart

// Messages sent from the renderer to the browser for NIP-07 operations
IPC_MESSAGE_ROUTED1(NostrHostMsg_GetPublicKey,
                    int /* request_id */)

IPC_MESSAGE_ROUTED2(NostrHostMsg_SignEvent,
                    int /* request_id */,
                    base::Value::Dict /* unsigned_event */)

IPC_MESSAGE_ROUTED1(NostrHostMsg_GetRelays,
                    int /* request_id */)

IPC_MESSAGE_ROUTED3(NostrHostMsg_Nip04Encrypt,
                    int /* request_id */,
                    std::string /* pubkey */,
                    std::string /* plaintext */)

IPC_MESSAGE_ROUTED3(NostrHostMsg_Nip04Decrypt,
                    int /* request_id */,
                    std::string /* pubkey */,
                    std::string /* ciphertext */)

// Local relay messages
IPC_MESSAGE_ROUTED2(NostrHostMsg_RelayQuery,
                    int /* request_id */,
                    base::Value::Dict /* filter */)

IPC_MESSAGE_ROUTED2(NostrHostMsg_RelayCount,
                    int /* request_id */,
                    base::Value::Dict /* filter */)

IPC_MESSAGE_ROUTED2(NostrHostMsg_RelayDelete,
                    int /* request_id */,
                    base::Value::Dict /* filter */)

// Blossom messages
IPC_MESSAGE_ROUTED2(NostrHostMsg_BlossomUpload,
                    int /* request_id */,
                    std::vector<uint8_t> /* data */)

IPC_MESSAGE_ROUTED2(NostrHostMsg_BlossomGet,
                    int /* request_id */,
                    std::string /* hash */)

IPC_MESSAGE_ROUTED2(NostrHostMsg_BlossomHas,
                    int /* request_id */,
                    std::string /* hash */)

// Messages sent from the browser to the renderer
IPC_MESSAGE_ROUTED3(NostrMsg_GetPublicKeyResponse,
                    int /* request_id */,
                    bool /* success */,
                    std::string /* pubkey */)

IPC_MESSAGE_ROUTED3(NostrMsg_SignEventResponse,
                    int /* request_id */,
                    bool /* success */,
                    base::Value::Dict /* signed_event */)

IPC_MESSAGE_ROUTED3(NostrMsg_GetRelaysResponse,
                    int /* request_id */,
                    bool /* success */,
                    base::Value::Dict /* relays */)

IPC_MESSAGE_ROUTED3(NostrMsg_Nip04EncryptResponse,
                    int /* request_id */,
                    bool /* success */,
                    std::string /* ciphertext */)

IPC_MESSAGE_ROUTED3(NostrMsg_Nip04DecryptResponse,
                    int /* request_id */,
                    bool /* success */,
                    std::string /* plaintext */)

IPC_MESSAGE_ROUTED3(NostrMsg_RelayQueryResponse,
                    int /* request_id */,
                    bool /* success */,
                    base::Value::List /* events */)

IPC_MESSAGE_ROUTED3(NostrMsg_RelayCountResponse,
                    int /* request_id */,
                    bool /* success */,
                    int /* count */)

IPC_MESSAGE_ROUTED3(NostrMsg_RelayDeleteResponse,
                    int /* request_id */,
                    bool /* success */,
                    int /* deleted_count */)

IPC_MESSAGE_ROUTED3(NostrMsg_BlossomUploadResponse,
                    int /* request_id */,
                    bool /* success */,
                    base::Value::Dict /* result */)

IPC_MESSAGE_ROUTED3(NostrMsg_BlossomGetResponse,
                    int /* request_id */,
                    bool /* success */,
                    std::vector<uint8_t> /* data */)

IPC_MESSAGE_ROUTED3(NostrMsg_BlossomHasResponse,
                    int /* request_id */,
                    bool /* success */,
                    bool /* exists */)

// Permission request messages
IPC_MESSAGE_ROUTED4(NostrMsg_PermissionRequest,
                    int /* request_id */,
                    GURL /* origin */,
                    std::string /* method */,
                    base::Value::Dict /* details */)

IPC_MESSAGE_ROUTED2(NostrHostMsg_PermissionResponse,
                    int /* request_id */,
                    bool /* granted */)

#endif  // CHROME_COMMON_NOSTR_MESSAGES_H_