// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NOSTR_NOSTR_BINDINGS_H_
#define CONTENT_RENDERER_NOSTR_NOSTR_BINDINGS_H_

#include "base/memory/weak_ptr.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace content {
class RenderFrame;
}

namespace tungsten {

// NostrBindings implements the window.nostr object that provides
// NIP-07 functionality to web pages.
class NostrBindings : public gin::Wrappable<NostrBindings> {
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

  // Associated render frame
  content::RenderFrame* render_frame_;
  
  base::WeakPtrFactory<NostrBindings> weak_factory_{this};
};

}  // namespace tungsten

#endif  // CONTENT_RENDERER_NOSTR_NOSTR_BINDINGS_H_