// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NOSTR_NOSTR_ACCOUNTS_BINDINGS_H_
#define CONTENT_RENDERER_NOSTR_NOSTR_ACCOUNTS_BINDINGS_H_

#include <string>

#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace content {
class RenderFrame;
}

namespace tungsten {

class NostrBindings;

// NostrAccountsBindings implements the window.nostr.accounts object that provides
// account management functionality for Nostr.
class NostrAccountsBindings : public gin::Wrappable<NostrAccountsBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  // Create the accounts bindings object
  static v8::Local<v8::Value> Create(v8::Isolate* isolate,
                                     NostrBindings* parent_bindings);

  // Account management methods
  
  // Returns a promise that resolves to an array of all configured accounts
  v8::Local<v8::Promise> List(v8::Isolate* isolate);
  
  // Returns a promise that resolves to the currently active account
  v8::Local<v8::Promise> Current(v8::Isolate* isolate);
  
  // Switches to the specified account by public key
  // Returns a promise that resolves when the switch is complete
  v8::Local<v8::Promise> Switch(v8::Isolate* isolate, const std::string& pubkey);
  
  // Creates a new account with optional settings
  // Currently returns a rejected promise - backend support not yet implemented
  v8::Local<v8::Promise> Create(v8::Isolate* isolate, v8::Local<v8::Object> options);
  
  // Imports an account from nsec or private key
  // Currently returns a rejected promise - backend support not yet implemented
  v8::Local<v8::Promise> Import(v8::Isolate* isolate, v8::Local<v8::Object> options);

 private:
  explicit NostrAccountsBindings(NostrBindings* parent_bindings);
  ~NostrAccountsBindings() override;

  // gin::Wrappable
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // Parent bindings for accessing IPC methods
  NostrBindings* parent_bindings_;
};

}  // namespace tungsten

#endif  // CONTENT_RENDERER_NOSTR_NOSTR_ACCOUNTS_BINDINGS_H_