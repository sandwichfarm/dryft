// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NOSTR_MESSAGES_H_
#define CHROME_COMMON_NOSTR_MESSAGES_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/common/nostr_message_start.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"
#include "url/origin.h"

// Nostr-specific parameter types
struct NostrPermissionRequest {
  NostrPermissionRequest();
  NostrPermissionRequest(const NostrPermissionRequest& other);
  ~NostrPermissionRequest();

  url::Origin origin;
  std::string method;  // "getPublicKey", "signEvent", etc.
  base::Value::Dict details;  // Additional context (e.g., event kinds for signEvent)
  base::TimeTicks timestamp;
  bool remember_decision;
};

struct NostrEvent {
  NostrEvent();
  NostrEvent(const NostrEvent& other);
  ~NostrEvent();

  std::string id;
  std::string pubkey;
  int64_t created_at;
  int kind;
  std::vector<std::vector<std::string>> tags;
  std::string content;
  std::string sig;
};

struct NostrRelayPolicy {
  NostrRelayPolicy();
  NostrRelayPolicy(const NostrRelayPolicy& other);
  ~NostrRelayPolicy();

  // Map of relay URL to read/write permissions
  std::map<std::string, base::Value::Dict> relays;
};

struct BlossomUploadResult {
  BlossomUploadResult();
  BlossomUploadResult(const BlossomUploadResult& other);
  ~BlossomUploadResult();

  std::string hash;
  std::string url;
  int64_t size;
  std::string mime_type;
  std::vector<std::string> servers;
};

// Rate limiting info attached to messages
struct NostrRateLimitInfo {
  NostrRateLimitInfo();
  NostrRateLimitInfo(const NostrRateLimitInfo& other);
  ~NostrRateLimitInfo();

  int requests_per_minute;
  int signs_per_hour;
  base::TimeTicks window_start;
  int current_count;
};

#define IPC_MESSAGE_START NostrMsgStart

// Include parameter traits for custom types
IPC_STRUCT_TRAITS_BEGIN(NostrPermissionRequest)
  IPC_STRUCT_TRAITS_MEMBER(origin)
  IPC_STRUCT_TRAITS_MEMBER(method)
  IPC_STRUCT_TRAITS_MEMBER(details)
  IPC_STRUCT_TRAITS_MEMBER(timestamp)
  IPC_STRUCT_TRAITS_MEMBER(remember_decision)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(NostrEvent)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(pubkey)
  IPC_STRUCT_TRAITS_MEMBER(created_at)
  IPC_STRUCT_TRAITS_MEMBER(kind)
  IPC_STRUCT_TRAITS_MEMBER(tags)
  IPC_STRUCT_TRAITS_MEMBER(content)
  IPC_STRUCT_TRAITS_MEMBER(sig)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(NostrRelayPolicy)
  IPC_STRUCT_TRAITS_MEMBER(relays)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(BlossomUploadResult)
  IPC_STRUCT_TRAITS_MEMBER(hash)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(mime_type)
  IPC_STRUCT_TRAITS_MEMBER(servers)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(NostrRateLimitInfo)
  IPC_STRUCT_TRAITS_MEMBER(requests_per_minute)
  IPC_STRUCT_TRAITS_MEMBER(signs_per_hour)
  IPC_STRUCT_TRAITS_MEMBER(window_start)
  IPC_STRUCT_TRAITS_MEMBER(current_count)
IPC_STRUCT_TRAITS_END()

// ===== NIP-07 Messages =====
// Messages from renderer to browser

// Request public key
IPC_MESSAGE_ROUTED2(NostrHostMsg_GetPublicKey,
                    int /* request_id */,
                    url::Origin /* origin */)

// Sign an event
IPC_MESSAGE_ROUTED4(NostrHostMsg_SignEvent,
                    int /* request_id */,
                    url::Origin /* origin */,
                    base::Value::Dict /* unsigned_event */,
                    NostrRateLimitInfo /* rate_limit_info */)

// Get relay policy
IPC_MESSAGE_ROUTED2(NostrHostMsg_GetRelays,
                    int /* request_id */,
                    url::Origin /* origin */)

// NIP-04 encryption
IPC_MESSAGE_ROUTED4(NostrHostMsg_Nip04Encrypt,
                    int /* request_id */,
                    url::Origin /* origin */,
                    std::string /* pubkey */,
                    std::string /* plaintext */)

// NIP-04 decryption
IPC_MESSAGE_ROUTED4(NostrHostMsg_Nip04Decrypt,
                    int /* request_id */,
                    url::Origin /* origin */,
                    std::string /* pubkey */,
                    std::string /* ciphertext */)

// NIP-44 encryption (newer, preferred)
IPC_MESSAGE_ROUTED4(NostrHostMsg_Nip44Encrypt,
                    int /* request_id */,
                    url::Origin /* origin */,
                    std::string /* pubkey */,
                    std::string /* plaintext */)

// NIP-44 decryption
IPC_MESSAGE_ROUTED4(NostrHostMsg_Nip44Decrypt,
                    int /* request_id */,
                    url::Origin /* origin */,
                    std::string /* pubkey */,
                    std::string /* ciphertext */)

// ===== Permission Messages =====

// Request permission for an operation
IPC_MESSAGE_ROUTED2(NostrHostMsg_RequestPermission,
                    int /* request_id */,
                    NostrPermissionRequest /* request */)

// Check if origin has permission
IPC_MESSAGE_ROUTED3(NostrHostMsg_CheckPermission,
                    int /* request_id */,
                    url::Origin /* origin */,
                    std::string /* method */)

// ===== Local Relay Messages =====

// Query events from local relay
IPC_MESSAGE_ROUTED3(NostrHostMsg_RelayQuery,
                    int /* request_id */,
                    base::Value::Dict /* filter */,
                    int /* limit */)

// Count events in local relay
IPC_MESSAGE_ROUTED2(NostrHostMsg_RelayCount,
                    int /* request_id */,
                    base::Value::Dict /* filter */)

// Delete events from local relay
IPC_MESSAGE_ROUTED3(NostrHostMsg_RelayDelete,
                    int /* request_id */,
                    base::Value::Dict /* filter */,
                    url::Origin /* origin */)

// Subscribe to local relay events
IPC_MESSAGE_ROUTED3(NostrHostMsg_RelaySubscribe,
                    int /* subscription_id */,
                    base::Value::Dict /* filter */,
                    url::Origin /* origin */)

// Unsubscribe from local relay
IPC_MESSAGE_ROUTED1(NostrHostMsg_RelayUnsubscribe,
                    int /* subscription_id */)

// Get local relay status
IPC_MESSAGE_ROUTED1(NostrHostMsg_RelayGetStatus,
                    int /* request_id */)

// ===== Blossom Messages =====

// Upload blob to Blossom
IPC_MESSAGE_ROUTED5(NostrHostMsg_BlossomUpload,
                    int /* request_id */,
                    url::Origin /* origin */,
                    std::vector<uint8_t> /* data */,
                    std::string /* mime_type */,
                    base::Value::Dict /* metadata */)

// Get blob from Blossom
IPC_MESSAGE_ROUTED3(NostrHostMsg_BlossomGet,
                    int /* request_id */,
                    std::string /* hash */,
                    url::Origin /* origin */)

// Check if blob exists
IPC_MESSAGE_ROUTED3(NostrHostMsg_BlossomHas,
                    int /* request_id */,
                    std::string /* hash */,
                    url::Origin /* origin */)

// List Blossom servers
IPC_MESSAGE_ROUTED2(NostrHostMsg_BlossomListServers,
                    int /* request_id */,
                    url::Origin /* origin */)

// Mirror blob to servers
IPC_MESSAGE_ROUTED4(NostrHostMsg_BlossomMirror,
                    int /* request_id */,
                    std::string /* hash */,
                    std::vector<std::string> /* servers */,
                    url::Origin /* origin */)

// Create Blossom authorization event
IPC_MESSAGE_ROUTED4(NostrHostMsg_BlossomCreateAuth,
                    int /* request_id */,
                    std::string /* verb */,
                    std::vector<std::string> /* files */,
                    int64_t /* expiration */)

// ===== Account Management Messages =====

// List available accounts
IPC_MESSAGE_ROUTED2(NostrHostMsg_ListAccounts,
                    int /* request_id */,
                    url::Origin /* origin */)

// Get current account
IPC_MESSAGE_ROUTED2(NostrHostMsg_GetCurrentAccount,
                    int /* request_id */,
                    url::Origin /* origin */)

// Switch account
IPC_MESSAGE_ROUTED3(NostrHostMsg_SwitchAccount,
                    int /* request_id */,
                    std::string /* pubkey */,
                    url::Origin /* origin */)

// ===== Response Messages (Browser to Renderer) =====

// NIP-07 responses
IPC_MESSAGE_ROUTED3(NostrMsg_GetPublicKeyResponse,
                    int /* request_id */,
                    bool /* success */,
                    std::string /* pubkey_or_error */)

IPC_MESSAGE_ROUTED3(NostrMsg_SignEventResponse,
                    int /* request_id */,
                    bool /* success */,
                    base::Value::Dict /* signed_event_or_error */)

IPC_MESSAGE_ROUTED3(NostrMsg_GetRelaysResponse,
                    int /* request_id */,
                    bool /* success */,
                    NostrRelayPolicy /* relays_or_error */)

IPC_MESSAGE_ROUTED3(NostrMsg_Nip04EncryptResponse,
                    int /* request_id */,
                    bool /* success */,
                    std::string /* ciphertext_or_error */)

IPC_MESSAGE_ROUTED3(NostrMsg_Nip04DecryptResponse,
                    int /* request_id */,
                    bool /* success */,
                    std::string /* plaintext_or_error */)

IPC_MESSAGE_ROUTED3(NostrMsg_Nip44EncryptResponse,
                    int /* request_id */,
                    bool /* success */,
                    std::string /* ciphertext_or_error */)

IPC_MESSAGE_ROUTED3(NostrMsg_Nip44DecryptResponse,
                    int /* request_id */,
                    bool /* success */,
                    std::string /* plaintext_or_error */)

// Permission responses
IPC_MESSAGE_ROUTED3(NostrMsg_RequestPermissionResponse,
                    int /* request_id */,
                    bool /* granted */,
                    bool /* remember_decision */)

IPC_MESSAGE_ROUTED3(NostrMsg_CheckPermissionResponse,
                    int /* request_id */,
                    bool /* has_permission */,
                    NostrRateLimitInfo /* rate_limit_info */)

// Local relay responses
IPC_MESSAGE_ROUTED3(NostrMsg_RelayQueryResponse,
                    int /* request_id */,
                    bool /* success */,
                    std::vector<NostrEvent> /* events */)

IPC_MESSAGE_ROUTED3(NostrMsg_RelayCountResponse,
                    int /* request_id */,
                    bool /* success */,
                    int /* count */)

IPC_MESSAGE_ROUTED3(NostrMsg_RelayDeleteResponse,
                    int /* request_id */,
                    bool /* success */,
                    int /* deleted_count */)

IPC_MESSAGE_ROUTED2(NostrMsg_RelaySubscribeResponse,
                    int /* subscription_id */,
                    bool /* success */)

// Relay event notification (pushed to subscriber)
IPC_MESSAGE_ROUTED2(NostrMsg_RelayEvent,
                    int /* subscription_id */,
                    NostrEvent /* event */)

IPC_MESSAGE_ROUTED4(NostrMsg_RelayStatusResponse,
                    int /* request_id */,
                    bool /* connected */,
                    int64_t /* event_count */,
                    int64_t /* storage_used */)

// Blossom responses
IPC_MESSAGE_ROUTED3(NostrMsg_BlossomUploadResponse,
                    int /* request_id */,
                    bool /* success */,
                    BlossomUploadResult /* result_or_error */)

IPC_MESSAGE_ROUTED3(NostrMsg_BlossomGetResponse,
                    int /* request_id */,
                    bool /* success */,
                    std::vector<uint8_t> /* data */)

IPC_MESSAGE_ROUTED3(NostrMsg_BlossomHasResponse,
                    int /* request_id */,
                    bool /* success */,
                    bool /* exists */)

IPC_MESSAGE_ROUTED3(NostrMsg_BlossomListServersResponse,
                    int /* request_id */,
                    bool /* success */,
                    base::Value::List /* servers */)

IPC_MESSAGE_ROUTED3(NostrMsg_BlossomMirrorResponse,
                    int /* request_id */,
                    bool /* success */,
                    base::Value::Dict /* mirror_results */)

IPC_MESSAGE_ROUTED3(NostrMsg_BlossomCreateAuthResponse,
                    int /* request_id */,
                    bool /* success */,
                    NostrEvent /* auth_event */)

// Account management responses
IPC_MESSAGE_ROUTED3(NostrMsg_ListAccountsResponse,
                    int /* request_id */,
                    bool /* success */,
                    base::Value::List /* accounts */)

IPC_MESSAGE_ROUTED3(NostrMsg_GetCurrentAccountResponse,
                    int /* request_id */,
                    bool /* success */,
                    base::Value::Dict /* account */)

IPC_MESSAGE_ROUTED2(NostrMsg_SwitchAccountResponse,
                    int /* request_id */,
                    bool /* success */)

// ===== Notification Messages (Browser to Renderer) =====

// Notify renderer of permission changes
IPC_MESSAGE_ROUTED2(NostrMsg_PermissionChanged,
                    url::Origin /* origin */,
                    base::Value::Dict /* new_permissions */)

// Notify renderer of account switch
IPC_MESSAGE_ROUTED1(NostrMsg_AccountSwitched,
                    std::string /* new_pubkey */)

// Notify renderer of relay connection status change
IPC_MESSAGE_ROUTED1(NostrMsg_RelayConnectionChanged,
                    bool /* connected */)

// Error notification for critical failures
IPC_MESSAGE_ROUTED2(NostrMsg_Error,
                    std::string /* error_type */,
                    std::string /* error_message */)

#endif  // CHROME_COMMON_NOSTR_MESSAGES_H_