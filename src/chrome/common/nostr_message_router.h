// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NOSTR_MESSAGE_ROUTER_H_
#define CHROME_COMMON_NOSTR_MESSAGE_ROUTER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/nostr/nostr_permission_manager.h"
#include "content/public/browser/browser_message_filter.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

// Forward declarations
struct NostrPermissionRequest;
struct NostrEvent;
struct NostrRelayPolicy;
struct BlossomUploadResult;
struct NostrRateLimitInfo;

// Browser-side message filter for handling Nostr IPC messages
class NostrMessageRouter : public content::BrowserMessageFilter {
 public:
  explicit NostrMessageRouter(content::BrowserContext* browser_context);
  ~NostrMessageRouter() override;

  // content::BrowserMessageFilter implementation
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() const override;
  
  // Message handlers - NIP-07 operations
  void OnGetPublicKey(int request_id, const url::Origin& origin);
  void OnSignEvent(int request_id, 
                   const url::Origin& origin,
                   const base::Value::Dict& unsigned_event,
                   const NostrRateLimitInfo& rate_limit_info);
  void OnGetRelays(int request_id, const url::Origin& origin);
  void OnNip04Encrypt(int request_id,
                      const url::Origin& origin,
                      const std::string& pubkey,
                      const std::string& plaintext);
  void OnNip04Decrypt(int request_id,
                      const url::Origin& origin,
                      const std::string& pubkey,
                      const std::string& ciphertext);
  void OnNip44Encrypt(int request_id,
                      const url::Origin& origin,
                      const std::string& pubkey,
                      const std::string& plaintext);
  void OnNip44Decrypt(int request_id,
                      const url::Origin& origin,
                      const std::string& pubkey,
                      const std::string& ciphertext);

  // Permission handlers
  void OnRequestPermission(int request_id,
                          const NostrPermissionRequest& request);
  void OnCheckPermission(int request_id,
                        const url::Origin& origin,
                        const std::string& method);

  // Local relay handlers
  void OnRelayQuery(int request_id,
                    const base::Value::Dict& filter,
                    int limit);
  void OnRelayCount(int request_id,
                    const base::Value::Dict& filter);
  void OnRelayDelete(int request_id,
                     const base::Value::Dict& filter,
                     const url::Origin& origin);
  void OnRelaySubscribe(int subscription_id,
                        const base::Value::Dict& filter,
                        const url::Origin& origin);
  void OnRelayUnsubscribe(int subscription_id);
  void OnRelayGetStatus(int request_id);

  // Blossom handlers
  void OnBlossomUpload(int request_id,
                       const url::Origin& origin,
                       const std::vector<uint8_t>& data,
                       const std::string& mime_type,
                       const base::Value::Dict& metadata);
  void OnBlossomGet(int request_id,
                    const std::string& hash,
                    const url::Origin& origin);
  void OnBlossomHas(int request_id,
                    const std::string& hash,
                    const url::Origin& origin);
  void OnBlossomListServers(int request_id,
                           const url::Origin& origin);
  void OnBlossomMirror(int request_id,
                       const std::string& hash,
                       const std::vector<std::string>& servers,
                       const url::Origin& origin);
  void OnBlossomCreateAuth(int request_id,
                          const std::string& verb,
                          const std::vector<std::string>& files,
                          int64_t expiration);

  // Account management handlers
  void OnListAccounts(int request_id, const url::Origin& origin);
  void OnGetCurrentAccount(int request_id, const url::Origin& origin);
  void OnSwitchAccount(int request_id,
                      const std::string& pubkey,
                      const url::Origin& origin);

 private:
  // Helper methods
  content::RenderFrameHost* GetRenderFrameHost();
  bool CheckOriginPermission(const url::Origin& origin, 
                            const std::string& method);
  std::optional<nostr::NIP07Permission::Method> StringToMethod(
      const std::string& method);
  
  // Input validation helpers
  bool ValidateEncryptionInputs(const std::string& pubkey,
                               const std::string& plaintext,
                               int request_id,
                               const std::string& operation);
  bool ValidateDecryptionInputs(const std::string& pubkey,
                               const std::string& ciphertext,
                               int request_id,
                               const std::string& operation);
  
  void SendErrorResponse(int request_id, const std::string& error);

  // Browser context for accessing profile-specific services
  raw_ptr<content::BrowserContext> browser_context_;
  
  // Weak pointer factory for async callbacks
  base::WeakPtrFactory<NostrMessageRouter> weak_factory_{this};
};

// Renderer-side message handler for receiving Nostr responses
class NostrMessageHandler {
 public:
  NostrMessageHandler();
  ~NostrMessageHandler();

  // Handle incoming messages from browser process
  bool OnMessageReceived(const IPC::Message& message);

  // Callbacks for responses
  using PublicKeyCallback = base::OnceCallback<void(bool, const std::string&)>;
  using SignEventCallback = 
      base::OnceCallback<void(bool, const base::Value::Dict&)>;
  using RelaysCallback = 
      base::OnceCallback<void(bool, const NostrRelayPolicy&)>;
  using EncryptCallback = base::OnceCallback<void(bool, const std::string&)>;
  using DecryptCallback = base::OnceCallback<void(bool, const std::string&)>;
  using PermissionCallback = base::OnceCallback<void(bool, bool)>;
  using QueryCallback = 
      base::OnceCallback<void(bool, const std::vector<NostrEvent>&)>;
  using CountCallback = base::OnceCallback<void(bool, int)>;
  using UploadCallback = 
      base::OnceCallback<void(bool, const BlossomUploadResult&)>;

  // Register callbacks for pending requests
  void RegisterPublicKeyCallback(int request_id, PublicKeyCallback callback);
  void RegisterSignEventCallback(int request_id, SignEventCallback callback);
  void RegisterRelaysCallback(int request_id, RelaysCallback callback);
  void RegisterEncryptCallback(int request_id, EncryptCallback callback);
  void RegisterDecryptCallback(int request_id, DecryptCallback callback);
  void RegisterPermissionCallback(int request_id, PermissionCallback callback);
  void RegisterQueryCallback(int request_id, QueryCallback callback);
  void RegisterCountCallback(int request_id, CountCallback callback);
  void RegisterUploadCallback(int request_id, UploadCallback callback);

 private:
  // Response handlers
  void OnGetPublicKeyResponse(int request_id,
                             bool success,
                             const std::string& pubkey_or_error);
  void OnSignEventResponse(int request_id,
                          bool success,
                          const base::Value::Dict& signed_event_or_error);
  void OnGetRelaysResponse(int request_id,
                          bool success,
                          const NostrRelayPolicy& relays_or_error);
  void OnNip04EncryptResponse(int request_id,
                             bool success,
                             const std::string& ciphertext_or_error);
  void OnNip04DecryptResponse(int request_id,
                             bool success,
                             const std::string& plaintext_or_error);
  void OnNip44EncryptResponse(int request_id,
                             bool success,
                             const std::string& ciphertext_or_error);
  void OnNip44DecryptResponse(int request_id,
                             bool success,
                             const std::string& plaintext_or_error);
  void OnRequestPermissionResponse(int request_id,
                                  bool granted,
                                  bool remember_decision);
  void OnRelayQueryResponse(int request_id,
                           bool success,
                           const std::vector<NostrEvent>& events);
  void OnRelayCountResponse(int request_id,
                           bool success,
                           int count);
  void OnBlossomUploadResponse(int request_id,
                              bool success,
                              const BlossomUploadResult& result_or_error);

  // Notification handlers
  void OnPermissionChanged(const url::Origin& origin,
                          const base::Value::Dict& new_permissions);
  void OnAccountSwitched(const std::string& new_pubkey);
  void OnRelayConnectionChanged(bool connected);
  void OnError(const std::string& error_type,
              const std::string& error_message);

  // Maps for pending callbacks
  std::map<int, PublicKeyCallback> pending_pubkey_callbacks_;
  std::map<int, SignEventCallback> pending_sign_callbacks_;
  std::map<int, RelaysCallback> pending_relays_callbacks_;
  std::map<int, EncryptCallback> pending_encrypt_callbacks_;
  std::map<int, DecryptCallback> pending_decrypt_callbacks_;
  std::map<int, PermissionCallback> pending_permission_callbacks_;
  std::map<int, QueryCallback> pending_query_callbacks_;
  std::map<int, CountCallback> pending_count_callbacks_;
  std::map<int, UploadCallback> pending_upload_callbacks_;

  // Next request ID
  int next_request_id_ = 1;
};

#endif  // CHROME_COMMON_NOSTR_MESSAGE_ROUTER_H_