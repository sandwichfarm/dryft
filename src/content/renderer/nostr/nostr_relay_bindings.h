// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NOSTR_NOSTR_RELAY_BINDINGS_H_
#define CONTENT_RENDERER_NOSTR_NOSTR_RELAY_BINDINGS_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "chrome/common/nostr_messages.h"
#include "content/public/renderer/render_frame_observer.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace content {
class RenderFrame;
}

namespace tungsten {

// NostrRelayBindings implements the window.nostr.relay object that provides
// local relay access to web pages.
class NostrRelayBindings : public gin::Wrappable<NostrRelayBindings>,
                          public content::RenderFrameObserver {
 public:
  static gin::WrapperInfo kWrapperInfo;

  // Create the relay bindings object
  static v8::Local<v8::Value> Create(v8::Isolate* isolate,
                                     content::RenderFrame* render_frame);

  // Read-only properties
  std::string GetUrl() const;
  bool GetConnected() const;
  int64_t GetEventCount() const;
  int64_t GetStorageUsed() const;

  // New relay methods
  std::string GetLocalRelayURL() const;
  v8::Local<v8::Promise> GetLocalRelaySocket(v8::Isolate* isolate);
  std::string GetPubkeyRelayURL(const std::string& pubkey) const;
  
  // Query methods
  v8::Local<v8::Promise> Query(v8::Isolate* isolate,
                               v8::Local<v8::Object> filter);
  v8::Local<v8::Promise> Count(v8::Isolate* isolate,
                               v8::Local<v8::Object> filter);
  v8::Local<v8::Promise> DeleteEvents(v8::Isolate* isolate,
                                     v8::Local<v8::Object> filter);

 private:
  explicit NostrRelayBindings(content::RenderFrame* render_frame);
  ~NostrRelayBindings() override;

  // gin::Wrappable
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // Helper to create error promise
  v8::Local<v8::Promise> CreateErrorPromise(v8::Isolate* isolate,
                                           const std::string& error);

  // IPC message handling
  void OnRelayQueryResponse(int request_id, bool success, const std::vector<NostrEvent>& events);
  void OnRelayCountResponse(int request_id, bool success, int count);
  void OnRelayDeleteResponse(int request_id, bool success, int deleted_count);
  void OnRelayStatusResponse(int request_id, bool connected, int64_t event_count, int64_t storage_used);

  // content::RenderFrameObserver
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() override;

  // Send IPC messages
  void SendRelayQuery(int request_id, const base::Value::Dict& filter);
  void SendRelayCount(int request_id, const base::Value::Dict& filter);
  void SendRelayDelete(int request_id, const base::Value::Dict& filter);
  void SendRelayGetStatus(int request_id);

  // Request ID generation
  int GetNextRequestId();

  // Convert V8 filter object to base::Value::Dict
  base::Value::Dict ConvertFilterToDict(v8::Isolate* isolate,
                                        v8::Local<v8::Object> filter);

  // Associated render frame
  content::RenderFrame* render_frame_;
  
  // Pending promises for async operations
  std::map<int, v8::Global<v8::Promise::Resolver>> pending_resolvers_;
  
  // Next request ID
  int next_request_id_;

  // Cached status
  mutable std::string cached_url_;
  mutable bool cached_connected_;
  mutable int64_t cached_event_count_;
  mutable int64_t cached_storage_used_;
  mutable base::Time last_status_update_;
  
  base::WeakPtrFactory<NostrRelayBindings> weak_factory_{this};
};

}  // namespace tungsten

#endif  // CONTENT_RENDERER_NOSTR_NOSTR_RELAY_BINDINGS_H_