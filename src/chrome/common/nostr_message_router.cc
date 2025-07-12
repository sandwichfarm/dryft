// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/nostr_message_router.h"

#include "base/logging.h"
#include "chrome/browser/nostr/nostr_service_factory.h"
#include "chrome/common/nostr_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"

using content::BrowserThread;

// NostrMessageRouter implementation
NostrMessageRouter::NostrMessageRouter(content::BrowserContext* browser_context)
    : content::BrowserMessageFilter(NostrMsgStart),
      browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

NostrMessageRouter::~NostrMessageRouter() = default;

bool NostrMessageRouter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NostrMessageRouter, message)
    // NIP-07 messages
    IPC_MESSAGE_HANDLER(NostrHostMsg_GetPublicKey, OnGetPublicKey)
    IPC_MESSAGE_HANDLER(NostrHostMsg_SignEvent, OnSignEvent)
    IPC_MESSAGE_HANDLER(NostrHostMsg_GetRelays, OnGetRelays)
    IPC_MESSAGE_HANDLER(NostrHostMsg_Nip04Encrypt, OnNip04Encrypt)
    IPC_MESSAGE_HANDLER(NostrHostMsg_Nip04Decrypt, OnNip04Decrypt)
    IPC_MESSAGE_HANDLER(NostrHostMsg_Nip44Encrypt, OnNip44Encrypt)
    IPC_MESSAGE_HANDLER(NostrHostMsg_Nip44Decrypt, OnNip44Decrypt)
    
    // Permission messages
    IPC_MESSAGE_HANDLER(NostrHostMsg_RequestPermission, OnRequestPermission)
    IPC_MESSAGE_HANDLER(NostrHostMsg_CheckPermission, OnCheckPermission)
    
    // Local relay messages
    IPC_MESSAGE_HANDLER(NostrHostMsg_RelayQuery, OnRelayQuery)
    IPC_MESSAGE_HANDLER(NostrHostMsg_RelayCount, OnRelayCount)
    IPC_MESSAGE_HANDLER(NostrHostMsg_RelayDelete, OnRelayDelete)
    IPC_MESSAGE_HANDLER(NostrHostMsg_RelaySubscribe, OnRelaySubscribe)
    IPC_MESSAGE_HANDLER(NostrHostMsg_RelayUnsubscribe, OnRelayUnsubscribe)
    IPC_MESSAGE_HANDLER(NostrHostMsg_RelayGetStatus, OnRelayGetStatus)
    
    // Blossom messages
    IPC_MESSAGE_HANDLER(NostrHostMsg_BlossomUpload, OnBlossomUpload)
    IPC_MESSAGE_HANDLER(NostrHostMsg_BlossomGet, OnBlossomGet)
    IPC_MESSAGE_HANDLER(NostrHostMsg_BlossomHas, OnBlossomHas)
    IPC_MESSAGE_HANDLER(NostrHostMsg_BlossomListServers, OnBlossomListServers)
    IPC_MESSAGE_HANDLER(NostrHostMsg_BlossomMirror, OnBlossomMirror)
    IPC_MESSAGE_HANDLER(NostrHostMsg_BlossomCreateAuth, OnBlossomCreateAuth)
    
    // Account management messages
    IPC_MESSAGE_HANDLER(NostrHostMsg_ListAccounts, OnListAccounts)
    IPC_MESSAGE_HANDLER(NostrHostMsg_GetCurrentAccount, OnGetCurrentAccount)
    IPC_MESSAGE_HANDLER(NostrHostMsg_SwitchAccount, OnSwitchAccount)
    
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  
  return handled;
}

void NostrMessageRouter::OnDestruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

void NostrMessageRouter::OnGetPublicKey(int request_id, 
                                       const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // Check permission first
  if (!CheckOriginPermission(origin, "getPublicKey")) {
    Send(new NostrMsg_GetPublicKeyResponse(
        routing_id(), request_id, false, "Permission denied"));
    return;
  }
  
  // Get the Nostr service
  auto* nostr_service = NostrServiceFactory::GetForBrowserContext(
      browser_context_);
  if (!nostr_service) {
    Send(new NostrMsg_GetPublicKeyResponse(
        routing_id(), request_id, false, "Nostr service not available"));
    return;
  }
  
  // Get the public key
  std::string pubkey = nostr_service->GetPublicKey();
  Send(new NostrMsg_GetPublicKeyResponse(
      routing_id(), request_id, true, pubkey));
}

void NostrMessageRouter::OnSignEvent(
    int request_id,
    const url::Origin& origin,
    const base::Value::Dict& unsigned_event,
    const NostrRateLimitInfo& rate_limit_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // Check permission
  if (!CheckOriginPermission(origin, "signEvent")) {
    base::Value::Dict error_dict;
    error_dict.Set("error", "Permission denied");
    Send(new NostrMsg_SignEventResponse(
        routing_id(), request_id, false, error_dict));
    return;
  }
  
  // Check rate limits
  if (rate_limit_info.current_count >= rate_limit_info.signs_per_hour) {
    base::Value::Dict error_dict;
    error_dict.Set("error", "Rate limit exceeded");
    Send(new NostrMsg_SignEventResponse(
        routing_id(), request_id, false, error_dict));
    return;
  }
  
  // Get the Nostr service and sign the event
  auto* nostr_service = NostrServiceFactory::GetForBrowserContext(
      browser_context_);
  if (!nostr_service) {
    base::Value::Dict error_dict;
    error_dict.Set("error", "Nostr service not available");
    Send(new NostrMsg_SignEventResponse(
        routing_id(), request_id, false, error_dict));
    return;
  }
  
  // Sign the event asynchronously
  nostr_service->SignEvent(
      unsigned_event,
      base::BindOnce(
          [](NostrMessageRouter* router, int request_id,
             bool success, const base::Value::Dict& signed_event) {
            router->Send(new NostrMsg_SignEventResponse(
                router->routing_id(), request_id, success, signed_event));
          },
          base::Unretained(this), request_id));
}

void NostrMessageRouter::OnGetRelays(int request_id,
                                    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // Check permission
  if (!CheckOriginPermission(origin, "getRelays")) {
    NostrRelayPolicy empty_policy;
    Send(new NostrMsg_GetRelaysResponse(
        routing_id(), request_id, false, empty_policy));
    return;
  }
  
  // Get the Nostr service
  auto* nostr_service = NostrServiceFactory::GetForBrowserContext(
      browser_context_);
  if (!nostr_service) {
    NostrRelayPolicy empty_policy;
    Send(new NostrMsg_GetRelaysResponse(
        routing_id(), request_id, false, empty_policy));
    return;
  }
  
  // Get the relay policy
  NostrRelayPolicy policy = nostr_service->GetRelayPolicy();
  Send(new NostrMsg_GetRelaysResponse(
      routing_id(), request_id, true, policy));
}

void NostrMessageRouter::OnNip04Encrypt(int request_id,
                                       const url::Origin& origin,
                                       const std::string& pubkey,
                                       const std::string& plaintext) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // Check permission
  if (!CheckOriginPermission(origin, "nip04.encrypt")) {
    Send(new NostrMsg_Nip04EncryptResponse(
        routing_id(), request_id, false, "Permission denied"));
    return;
  }
  
  // Get the Nostr service
  auto* nostr_service = NostrServiceFactory::GetForBrowserContext(
      browser_context_);
  if (!nostr_service) {
    Send(new NostrMsg_Nip04EncryptResponse(
        routing_id(), request_id, false, "Nostr service not available"));
    return;
  }
  
  // Encrypt the message
  nostr_service->Nip04Encrypt(
      pubkey, plaintext,
      base::BindOnce(
          [](NostrMessageRouter* router, int request_id,
             bool success, const std::string& ciphertext) {
            router->Send(new NostrMsg_Nip04EncryptResponse(
                router->routing_id(), request_id, success, ciphertext));
          },
          base::Unretained(this), request_id));
}

void NostrMessageRouter::OnNip04Decrypt(int request_id,
                                       const url::Origin& origin,
                                       const std::string& pubkey,
                                       const std::string& ciphertext) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // Check permission
  if (!CheckOriginPermission(origin, "nip04.decrypt")) {
    Send(new NostrMsg_Nip04DecryptResponse(
        routing_id(), request_id, false, "Permission denied"));
    return;
  }
  
  // Get the Nostr service
  auto* nostr_service = NostrServiceFactory::GetForBrowserContext(
      browser_context_);
  if (!nostr_service) {
    Send(new NostrMsg_Nip04DecryptResponse(
        routing_id(), request_id, false, "Nostr service not available"));
    return;
  }
  
  // Decrypt the message
  nostr_service->Nip04Decrypt(
      pubkey, ciphertext,
      base::BindOnce(
          [](NostrMessageRouter* router, int request_id,
             bool success, const std::string& plaintext) {
            router->Send(new NostrMsg_Nip04DecryptResponse(
                router->routing_id(), request_id, success, plaintext));
          },
          base::Unretained(this), request_id));
}

void NostrMessageRouter::OnNip44Encrypt(int request_id,
                                       const url::Origin& origin,
                                       const std::string& pubkey,
                                       const std::string& plaintext) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // Check permission
  if (!CheckOriginPermission(origin, "nip44.encrypt")) {
    Send(new NostrMsg_Nip44EncryptResponse(
        routing_id(), request_id, false, "Permission denied"));
    return;
  }
  
  // Get the Nostr service
  auto* nostr_service = NostrServiceFactory::GetForBrowserContext(
      browser_context_);
  if (!nostr_service) {
    Send(new NostrMsg_Nip44EncryptResponse(
        routing_id(), request_id, false, "Nostr service not available"));
    return;
  }
  
  // Encrypt using NIP-44 (newer standard)
  nostr_service->Nip44Encrypt(
      pubkey, plaintext,
      base::BindOnce(
          [](NostrMessageRouter* router, int request_id,
             bool success, const std::string& ciphertext) {
            router->Send(new NostrMsg_Nip44EncryptResponse(
                router->routing_id(), request_id, success, ciphertext));
          },
          base::Unretained(this), request_id));
}

void NostrMessageRouter::OnNip44Decrypt(int request_id,
                                       const url::Origin& origin,
                                       const std::string& pubkey,
                                       const std::string& ciphertext) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // Check permission
  if (!CheckOriginPermission(origin, "nip44.decrypt")) {
    Send(new NostrMsg_Nip44DecryptResponse(
        routing_id(), request_id, false, "Permission denied"));
    return;
  }
  
  // Get the Nostr service
  auto* nostr_service = NostrServiceFactory::GetForBrowserContext(
      browser_context_);
  if (!nostr_service) {
    Send(new NostrMsg_Nip44DecryptResponse(
        routing_id(), request_id, false, "Nostr service not available"));
    return;
  }
  
  // Decrypt using NIP-44
  nostr_service->Nip44Decrypt(
      pubkey, ciphertext,
      base::BindOnce(
          [](NostrMessageRouter* router, int request_id,
             bool success, const std::string& plaintext) {
            router->Send(new NostrMsg_Nip44DecryptResponse(
                router->routing_id(), request_id, success, plaintext));
          },
          base::Unretained(this), request_id));
}

void NostrMessageRouter::OnRequestPermission(
    int request_id,
    const NostrPermissionRequest& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // Get the Nostr service
  auto* nostr_service = NostrServiceFactory::GetForBrowserContext(
      browser_context_);
  if (!nostr_service) {
    Send(new NostrMsg_RequestPermissionResponse(
        routing_id(), request_id, false, false));
    return;
  }
  
  // Request permission from the user
  nostr_service->RequestPermission(
      request,
      base::BindOnce(
          [](NostrMessageRouter* router, int request_id,
             bool granted, bool remember) {
            router->Send(new NostrMsg_RequestPermissionResponse(
                router->routing_id(), request_id, granted, remember));
          },
          base::Unretained(this), request_id));
}

void NostrMessageRouter::OnCheckPermission(int request_id,
                                          const url::Origin& origin,
                                          const std::string& method) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  bool has_permission = CheckOriginPermission(origin, method);
  
  // Get current rate limit info
  NostrRateLimitInfo rate_limit_info;
  auto* nostr_service = NostrServiceFactory::GetForBrowserContext(
      browser_context_);
  if (nostr_service) {
    rate_limit_info = nostr_service->GetRateLimitInfo(origin, method);
  }
  
  Send(new NostrMsg_CheckPermissionResponse(
      routing_id(), request_id, has_permission, rate_limit_info));
}

// Additional method implementations would follow the same pattern...

bool NostrMessageRouter::CheckOriginPermission(const url::Origin& origin,
                                               const std::string& method) {
  auto* nostr_service = NostrServiceFactory::GetForBrowserContext(
      browser_context_);
  if (!nostr_service) {
    return false;
  }
  
  return nostr_service->HasPermission(origin, method);
}

void NostrMessageRouter::SendErrorResponse(int request_id,
                                          const std::string& error) {
  // This would send appropriate error response based on the original request type
  LOG(ERROR) << "Nostr message error: " << error;
}

// NostrMessageHandler implementation (renderer side)
NostrMessageHandler::NostrMessageHandler() = default;

NostrMessageHandler::~NostrMessageHandler() = default;

bool NostrMessageHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NostrMessageHandler, message)
    // Response handlers
    IPC_MESSAGE_HANDLER(NostrMsg_GetPublicKeyResponse, OnGetPublicKeyResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_SignEventResponse, OnSignEventResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_GetRelaysResponse, OnGetRelaysResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_Nip04EncryptResponse, OnNip04EncryptResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_Nip04DecryptResponse, OnNip04DecryptResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_Nip44EncryptResponse, OnNip44EncryptResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_Nip44DecryptResponse, OnNip44DecryptResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_RequestPermissionResponse, 
                       OnRequestPermissionResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_RelayQueryResponse, OnRelayQueryResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_RelayCountResponse, OnRelayCountResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_BlossomUploadResponse, 
                       OnBlossomUploadResponse)
    
    // Notification handlers
    IPC_MESSAGE_HANDLER(NostrMsg_PermissionChanged, OnPermissionChanged)
    IPC_MESSAGE_HANDLER(NostrMsg_AccountSwitched, OnAccountSwitched)
    IPC_MESSAGE_HANDLER(NostrMsg_RelayConnectionChanged, 
                       OnRelayConnectionChanged)
    IPC_MESSAGE_HANDLER(NostrMsg_Error, OnError)
    
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  
  return handled;
}

void NostrMessageHandler::OnGetPublicKeyResponse(
    int request_id,
    bool success,
    const std::string& pubkey_or_error) {
  auto it = pending_pubkey_callbacks_.find(request_id);
  if (it != pending_pubkey_callbacks_.end()) {
    std::move(it->second).Run(success, pubkey_or_error);
    pending_pubkey_callbacks_.erase(it);
  }
}

// Additional response handler implementations...

void NostrMessageHandler::RegisterPublicKeyCallback(
    int request_id,
    PublicKeyCallback callback) {
  pending_pubkey_callbacks_[request_id] = std::move(callback);
}

// Additional callback registration methods...