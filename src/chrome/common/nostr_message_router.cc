// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/nostr_message_router.h"

#include "base/logging.h"
#include "chrome/browser/nostr/nostr_input_validator.h"
#include "chrome/browser/nostr/nostr_permission_manager_factory.h"
#include "chrome/browser/nostr/nostr_service_factory.h"
#include "chrome/browser/nostr/nostr_operation_rate_limiter.h"
#include "chrome/browser/profiles/profile.h"
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
  
  // Check rate limit
  if (!nostr_service->CheckRateLimit(origin.GetURL(), 
                                     NostrOperationRateLimiter::OperationType::kGetPublicKey)) {
    Send(new NostrMsg_GetPublicKeyResponse(
        routing_id(), request_id, false, "Rate limit exceeded"));
    return;
  }
  
  // Record the operation
  nostr_service->RecordOperation(origin.GetURL(),
                                NostrOperationRateLimiter::OperationType::kGetPublicKey);
  
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
  
  // Get the Nostr service
  auto* nostr_service = NostrServiceFactory::GetForBrowserContext(
      browser_context_);
  if (!nostr_service) {
    base::Value::Dict error_dict;
    error_dict.Set("error", "Nostr service not available");
    Send(new NostrMsg_SignEventResponse(
        routing_id(), request_id, false, error_dict));
    return;
  }
  
  // Check rate limits using centralized rate limiter
  if (!nostr_service->CheckRateLimit(origin.GetURL(),
                                     NostrOperationRateLimiter::OperationType::kSignEvent)) {
    base::Value::Dict error_dict;
    error_dict.Set("error", "Rate limit exceeded");
    Send(new NostrMsg_SignEventResponse(
        routing_id(), request_id, false, error_dict));
    return;
  }
  
  // Record the operation
  nostr_service->RecordOperation(origin.GetURL(),
                                NostrOperationRateLimiter::OperationType::kSignEvent);
  
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
  
  // Check rate limit
  if (!nostr_service->CheckRateLimit(origin.GetURL(),
                                     NostrOperationRateLimiter::OperationType::kGetRelays)) {
    NostrRelayPolicy empty_policy;
    Send(new NostrMsg_GetRelaysResponse(
        routing_id(), request_id, false, empty_policy));
    return;
  }
  
  // Record the operation
  nostr_service->RecordOperation(origin.GetURL(),
                                NostrOperationRateLimiter::OperationType::kGetRelays);
  
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
  
  // Validate inputs
  if (!ValidateEncryptionInputs(pubkey, plaintext, request_id, "nip04.encrypt")) {
    Send(new NostrMsg_Nip04EncryptResponse(
        routing_id(), request_id, false, "Invalid input parameters"));
    return;
  }
  
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
  
  // Validate inputs
  if (!ValidateDecryptionInputs(pubkey, ciphertext, request_id, "nip04.decrypt")) {
    Send(new NostrMsg_Nip04DecryptResponse(
        routing_id(), request_id, false, "Invalid input parameters"));
    return;
  }
  
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
  
  // Validate inputs
  if (!ValidateEncryptionInputs(pubkey, plaintext, request_id, "nip44.encrypt")) {
    Send(new NostrMsg_Nip44EncryptResponse(
        routing_id(), request_id, false, "Invalid input parameters"));
    return;
  }
  
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
  
  // Validate inputs
  if (!ValidateDecryptionInputs(pubkey, ciphertext, request_id, "nip44.decrypt")) {
    Send(new NostrMsg_Nip44DecryptResponse(
        routing_id(), request_id, false, "Invalid input parameters"));
    return;
  }
  
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
  
  // Get permission manager
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  if (!profile) {
    Send(new NostrMsg_RequestPermissionResponse(
        routing_id(), request_id, false, false));
    return;
  }
  
  auto* permission_manager = 
      nostr::NostrPermissionManagerFactory::GetForProfile(profile);
  if (!permission_manager) {
    Send(new NostrMsg_RequestPermissionResponse(
        routing_id(), request_id, false, false));
    return;
  }
  
  // Convert method string to enum
  auto method_enum = StringToMethod(request.method);
  if (!method_enum) {
    LOG(ERROR) << "Unknown method in permission request: " << request.method;
    Send(new NostrMsg_RequestPermissionResponse(
        routing_id(), request_id, false, false));
    return;
  }
  
  // Check current permission status
  auto result = permission_manager->CheckPermission(request.origin, *method_enum);
  
  switch (result) {
    case nostr::NostrPermissionManager::PermissionResult::GRANTED:
      // Already granted
      Send(new NostrMsg_RequestPermissionResponse(
          routing_id(), request_id, true, false));
      return;
    case nostr::NostrPermissionManager::PermissionResult::DENIED:
      // Already denied
      Send(new NostrMsg_RequestPermissionResponse(
          routing_id(), request_id, false, false));
      return;
    case nostr::NostrPermissionManager::PermissionResult::RATE_LIMITED:
      // Rate limited
      Send(new NostrMsg_RequestPermissionResponse(
          routing_id(), request_id, false, false));
      return;
    case nostr::NostrPermissionManager::PermissionResult::EXPIRED:
    case nostr::NostrPermissionManager::PermissionResult::ASK_USER:
      // Need to show permission dialog
      // TODO: Implement permission dialog flow (Issue F-6)
      // For now, auto-grant for testing purposes
      {
        nostr::NIP07Permission permission;
        permission.origin = request.origin;
        permission.default_policy = nostr::NIP07Permission::Policy::ALLOW;
        permission.granted_until = base::Time::Now() + base::Days(30);
        permission.method_policies[*method_enum] = nostr::NIP07Permission::Policy::ALLOW;
        
        auto grant_result = permission_manager->GrantPermission(
            request.origin, permission);
        
        bool granted = (grant_result == nostr::NostrPermissionManager::GrantResult::SUCCESS);
        Send(new NostrMsg_RequestPermissionResponse(
            routing_id(), request_id, granted, request.remember_decision));
      }
      return;
  }
}

void NostrMessageRouter::OnCheckPermission(int request_id,
                                          const url::Origin& origin,
                                          const std::string& method) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // Get permission manager
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  if (!profile) {
    NostrRateLimitInfo empty_rate_limit;
    Send(new NostrMsg_CheckPermissionResponse(
        routing_id(), request_id, false, empty_rate_limit));
    return;
  }
  
  auto* permission_manager = 
      nostr::NostrPermissionManagerFactory::GetForProfile(profile);
  if (!permission_manager) {
    NostrRateLimitInfo empty_rate_limit;
    Send(new NostrMsg_CheckPermissionResponse(
        routing_id(), request_id, false, empty_rate_limit));
    return;
  }
  
  // Convert method string to enum
  auto method_enum = StringToMethod(method);
  if (!method_enum) {
    LOG(ERROR) << "Unknown method in permission check: " << method;
    NostrRateLimitInfo empty_rate_limit;
    Send(new NostrMsg_CheckPermissionResponse(
        routing_id(), request_id, false, empty_rate_limit));
    return;
  }
  
  // Check permission
  auto result = permission_manager->CheckPermission(origin, *method_enum);
  bool has_permission = (result == nostr::NostrPermissionManager::PermissionResult::GRANTED);
  
  // Get current rate limit info from permission manager
  NostrRateLimitInfo rate_limit_info;
  auto permission = permission_manager->GetPermission(origin);
  if (permission) {
    rate_limit_info.requests_per_minute = permission->rate_limits.requests_per_minute;
    rate_limit_info.signs_per_hour = permission->rate_limits.signs_per_hour;
    rate_limit_info.window_start = base::TimeTicks::Now(); // Convert from Time
    
    if (*method_enum == nostr::NIP07Permission::Method::SIGN_EVENT) {
      rate_limit_info.current_count = permission->rate_limits.current_signs_count;
    } else {
      rate_limit_info.current_count = permission->rate_limits.current_requests_count;
    }
  } else {
    // Default rate limits
    rate_limit_info.requests_per_minute = 60;
    rate_limit_info.signs_per_hour = 20;
    rate_limit_info.window_start = base::TimeTicks::Now();
    rate_limit_info.current_count = 0;
  }
  
  Send(new NostrMsg_CheckPermissionResponse(
      routing_id(), request_id, has_permission, rate_limit_info));
}

// Account Management Handlers

void NostrMessageRouter::OnListAccounts(int request_id, 
                                       const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // Check permission - accounts are sensitive information
  if (!CheckOriginPermission(origin, "getPublicKey")) {
    base::Value::List empty_list;
    Send(new NostrMsg_ListAccountsResponse(
        routing_id(), request_id, false, empty_list));
    return;
  }
  
  // Get the Nostr service
  auto* nostr_service = NostrServiceFactory::GetForBrowserContext(
      browser_context_);
  if (!nostr_service) {
    base::Value::List empty_list;
    Send(new NostrMsg_ListAccountsResponse(
        routing_id(), request_id, false, empty_list));
    return;
  }
  
  // Get accounts list
  base::Value::List accounts = nostr_service->ListAccounts();
  Send(new NostrMsg_ListAccountsResponse(
      routing_id(), request_id, true, accounts));
}

void NostrMessageRouter::OnGetCurrentAccount(int request_id,
                                            const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // Check permission
  if (!CheckOriginPermission(origin, "getPublicKey")) {
    base::Value::Dict empty_dict;
    Send(new NostrMsg_GetCurrentAccountResponse(
        routing_id(), request_id, false, empty_dict));
    return;
  }
  
  // Get the Nostr service
  auto* nostr_service = NostrServiceFactory::GetForBrowserContext(
      browser_context_);
  if (!nostr_service) {
    base::Value::Dict empty_dict;
    Send(new NostrMsg_GetCurrentAccountResponse(
        routing_id(), request_id, false, empty_dict));
    return;
  }
  
  // Get current account
  base::Value::Dict account = nostr_service->GetCurrentAccount();
  Send(new NostrMsg_GetCurrentAccountResponse(
      routing_id(), request_id, true, account));
}

void NostrMessageRouter::OnSwitchAccount(int request_id,
                                        const std::string& pubkey,
                                        const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // Check permission - account switching is a privileged operation
  if (!CheckOriginPermission(origin, "getPublicKey")) {
    Send(new NostrMsg_SwitchAccountResponse(
        routing_id(), request_id, false));
    return;
  }
  
  // Validate pubkey format
  if (!nostr::NostrInputValidator::IsValidHexKey(pubkey)) {
    LOG(ERROR) << "Invalid pubkey format for account switch: " << pubkey;
    Send(new NostrMsg_SwitchAccountResponse(
        routing_id(), request_id, false));
    return;
  }
  
  // Get the Nostr service
  auto* nostr_service = NostrServiceFactory::GetForBrowserContext(
      browser_context_);
  if (!nostr_service) {
    Send(new NostrMsg_SwitchAccountResponse(
        routing_id(), request_id, false));
    return;
  }
  
  // Attempt to switch account
  bool success = nostr_service->SwitchAccount(pubkey);
  Send(new NostrMsg_SwitchAccountResponse(
      routing_id(), request_id, success));
  
  // If successful, notify all frames about the account switch
  if (success) {
    // TODO: Broadcast to all frames in this profile
    // For now, we'll rely on the renderer to handle the response
    LOG(INFO) << "Account switched successfully";
  }
}

bool NostrMessageRouter::CheckOriginPermission(const url::Origin& origin,
                                               const std::string& method) {
  // Get permission manager
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  if (!profile) {
    return false;
  }
  
  auto* permission_manager = 
      nostr::NostrPermissionManagerFactory::GetForProfile(profile);
  if (!permission_manager) {
    return false;
  }
  
  // Convert method string to enum
  auto method_enum = StringToMethod(method);
  if (!method_enum) {
    LOG(ERROR) << "Unknown method: " << method;
    return false;
  }
  
  // Check permission
  auto result = permission_manager->CheckPermission(origin, *method_enum);
  
  switch (result) {
    case nostr::NostrPermissionManager::PermissionResult::GRANTED:
      // Update rate limit counters
      permission_manager->UpdateRateLimit(origin, *method_enum);
      return true;
    case nostr::NostrPermissionManager::PermissionResult::DENIED:
    case nostr::NostrPermissionManager::PermissionResult::RATE_LIMITED:
    case nostr::NostrPermissionManager::PermissionResult::EXPIRED:
      return false;
    case nostr::NostrPermissionManager::PermissionResult::ASK_USER:
      // This should trigger a permission dialog, but for now return false
      // TODO: Implement permission dialog flow
      return false;
  }
  
  return false;
}

// Helper to convert method string to enum
std::optional<nostr::NIP07Permission::Method> NostrMessageRouter::StringToMethod(
    const std::string& method) {
  if (method == "getPublicKey") 
    return nostr::NIP07Permission::Method::GET_PUBLIC_KEY;
  if (method == "signEvent") 
    return nostr::NIP07Permission::Method::SIGN_EVENT;
  if (method == "getRelays") 
    return nostr::NIP07Permission::Method::GET_RELAYS;
  if (method == "nip04.encrypt") 
    return nostr::NIP07Permission::Method::NIP04_ENCRYPT;
  if (method == "nip04.decrypt") 
    return nostr::NIP07Permission::Method::NIP04_DECRYPT;
  return std::nullopt;
}

bool NostrMessageRouter::ValidateEncryptionInputs(const std::string& pubkey,
                                                 const std::string& plaintext,
                                                 int request_id,
                                                 const std::string& operation) {
  // Validate public key
  if (!nostr::NostrInputValidator::IsValidHexKey(pubkey)) {
    LOG(ERROR) << "Invalid public key format for " << operation << ": " << pubkey;
    return false;
  }
  
  // Validate plaintext length
  if (plaintext.empty() || plaintext.length() > nostr::NostrInputValidator::kMaxContentLength) {
    LOG(ERROR) << "Invalid plaintext length for " << operation << ": " << plaintext.length();
    return false;
  }
  
  return true;
}

bool NostrMessageRouter::ValidateDecryptionInputs(const std::string& pubkey,
                                                  const std::string& ciphertext,
                                                  int request_id,
                                                  const std::string& operation) {
  // Validate public key
  if (!nostr::NostrInputValidator::IsValidHexKey(pubkey)) {
    LOG(ERROR) << "Invalid public key format for " << operation << ": " << pubkey;
    return false;
  }
  
  // Validate ciphertext length
  if (ciphertext.empty() || ciphertext.length() > nostr::NostrInputValidator::kMaxContentLength * 2) {
    LOG(ERROR) << "Invalid ciphertext length for " << operation << ": " << ciphertext.length();
    return false;
  }
  
  return true;
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