// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NOSTR_NOSTR_BINDINGS_H_
#define CONTENT_RENDERER_NOSTR_NOSTR_BINDINGS_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_frame_observer.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace content {
class RenderFrame;
}

namespace dryft {

// NostrBindings implements the window.nostr object that provides
// NIP-07 functionality to web pages.
class NostrBindings : public gin::Wrappable<NostrBindings>,
                     public content::RenderFrameObserver {
 public:
  static gin::WrapperInfo kWrapperInfo;

  // Install the window.nostr object into the global context
  static void Install(v8::Local<v8::Object> global,
                     content::RenderFrame* render_frame);

  // gin::Wrappable
  static v8::Local<v8::Value> Create(v8::Isolate* isolate,
                                     content::RenderFrame* render_frame);

  // NIP-07 Methods (currently stubs)
  v8::Local<v8::Promise> GetPublicKey(v8::Isolate* isolate);
  v8::Local<v8::Promise> SignEvent(v8::Isolate* isolate,
                                   v8::Local<v8::Object> event);
  v8::Local<v8::Promise> GetRelays(v8::Isolate* isolate);
  
  // NIP-04 encryption methods
  v8::Local<v8::Object> GetNip04Object(v8::Isolate* isolate);
  v8::Local<v8::Promise> Nip04Encrypt(v8::Isolate* isolate,
                                      const std::string& pubkey,
                                      const std::string& plaintext);
  v8::Local<v8::Promise> Nip04Decrypt(v8::Isolate* isolate,
                                      const std::string& pubkey,
                                      const std::string& ciphertext);

  // Account management methods
  v8::Local<v8::Promise> ListAccounts(v8::Isolate* isolate);
  v8::Local<v8::Promise> GetCurrentAccount(v8::Isolate* isolate);
  v8::Local<v8::Promise> SwitchAccount(v8::Isolate* isolate,
                                       const std::string& pubkey);

  // Local relay property
  v8::Local<v8::Object> GetRelayObject(v8::Isolate* isolate);
  
  // Library paths property
  v8::Local<v8::Object> GetLibsObject(v8::Isolate* isolate);
  
  // Accounts management property
  v8::Local<v8::Object> GetAccountsObject(v8::Isolate* isolate);

  // Make accounts bindings a friend so it can access our methods
  friend class NostrAccountsBindings;

 private:
  explicit NostrBindings(content::RenderFrame* render_frame);
  ~NostrBindings() override;

  // gin::Wrappable
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // Helper to check if origin is allowed
  bool IsOriginAllowed() const;

  // Helper to create error promise
  v8::Local<v8::Promise> CreateErrorPromise(v8::Isolate* isolate,
                                           const std::string& error);

  // IPC message handling
  void OnGetPublicKeyResponse(int request_id, bool success, const std::string& result);
  void OnSignEventResponse(int request_id, bool success, const base::Value::Dict& result);
  void OnGetRelaysResponse(int request_id, bool success, const struct NostrRelayPolicy& result);
  void OnNip04EncryptResponse(int request_id, bool success, const std::string& result);
  void OnNip04DecryptResponse(int request_id, bool success, const std::string& result);
  void OnListAccountsResponse(int request_id, bool success, const base::Value::List& result);
  void OnGetCurrentAccountResponse(int request_id, bool success, const base::Value::Dict& result);
  void OnSwitchAccountResponse(int request_id, bool success);

  // content::RenderFrameObserver
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() override;

  // Send IPC messages
  void SendGetPublicKey(int request_id);
  void SendSignEvent(int request_id, const base::Value::Dict& event);
  void SendGetRelays(int request_id);
  void SendNip04Encrypt(int request_id, const std::string& pubkey, const std::string& plaintext);
  void SendNip04Decrypt(int request_id, const std::string& pubkey, const std::string& ciphertext);
  void SendListAccounts(int request_id);
  void SendGetCurrentAccount(int request_id);
  void SendSwitchAccount(int request_id, const std::string& pubkey);

  // Get current origin
  url::Origin GetCurrentOrigin() const;

  // Request ID generation
  int GetNextRequestId();

  // Associated render frame
  content::RenderFrame* render_frame_;
  
  // Pending promises for async operations
  std::map<int, v8::Global<v8::Promise::Resolver>> pending_resolvers_;
  
  // Next request ID
  int next_request_id_;
  
  base::WeakPtrFactory<NostrBindings> weak_factory_{this};
};

}  // namespace dryft

#endif  // CONTENT_RENDERER_NOSTR_NOSTR_BINDINGS_H_