// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/nostr/nostr_accounts_bindings.h"

#include "base/logging.h"
#include "content/renderer/nostr/nostr_bindings.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/object_template_builder.h"

namespace tungsten {

gin::WrapperInfo NostrAccountsBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
v8::Local<v8::Value> NostrAccountsBindings::Create(v8::Isolate* isolate,
                                                   NostrBindings* parent_bindings) {
  return gin::CreateHandle(isolate, new NostrAccountsBindings(parent_bindings)).ToV8();
}

NostrAccountsBindings::NostrAccountsBindings(NostrBindings* parent_bindings)
    : parent_bindings_(parent_bindings) {
  DCHECK(parent_bindings_);
}

NostrAccountsBindings::~NostrAccountsBindings() = default;

gin::ObjectTemplateBuilder NostrAccountsBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<NostrAccountsBindings>::GetObjectTemplateBuilder(isolate)
      .SetMethod("list", &NostrAccountsBindings::List)
      .SetMethod("current", &NostrAccountsBindings::Current)
      .SetMethod("switch", &NostrAccountsBindings::Switch)
      .SetMethod("create", &NostrAccountsBindings::Create)
      .SetMethod("import", &NostrAccountsBindings::Import);
}

v8::Local<v8::Promise> NostrAccountsBindings::List(v8::Isolate* isolate) {
  VLOG(2) << "window.nostr.accounts.list() called";
  
  // Delegate to parent bindings which already implements this
  return parent_bindings_->ListAccounts(isolate);
}

v8::Local<v8::Promise> NostrAccountsBindings::Current(v8::Isolate* isolate) {
  VLOG(2) << "window.nostr.accounts.current() called";
  
  // Delegate to parent bindings which already implements this
  return parent_bindings_->GetCurrentAccount(isolate);
}

v8::Local<v8::Promise> NostrAccountsBindings::Switch(v8::Isolate* isolate,
                                                     const std::string& pubkey) {
  VLOG(2) << "window.nostr.accounts.switch() called with pubkey: " << pubkey;
  
  // Delegate to parent bindings which already implements this
  return parent_bindings_->SwitchAccount(isolate, pubkey);
}

v8::Local<v8::Promise> NostrAccountsBindings::Create(v8::Isolate* isolate,
                                                     v8::Local<v8::Object> options) {
  VLOG(2) << "window.nostr.accounts.create() called";
  
  // Extract options if provided
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  std::string name;
  
  if (!options.IsEmpty() && options->IsObject()) {
    v8::Local<v8::Value> name_value;
    if (options->Get(context, gin::StringToV8(isolate, "name")).ToLocal(&name_value) && 
        name_value->IsString()) {
      name = gin::V8ToString(isolate, name_value);
    }
  }
  
  // For now, return a rejected promise as the backend doesn't implement create yet
  // TODO: Implement this when NostrService supports account creation
  return parent_bindings_->CreateErrorPromise(isolate, 
      "Account creation not yet implemented. Use browser settings to create accounts.");
}

v8::Local<v8::Promise> NostrAccountsBindings::Import(v8::Isolate* isolate,
                                                     v8::Local<v8::Object> options) {
  VLOG(2) << "window.nostr.accounts.import() called";
  
  if (options.IsEmpty() || !options->IsObject()) {
    return parent_bindings_->CreateErrorPromise(isolate, "Import options required");
  }
  
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  
  // Extract nsec or private key
  v8::Local<v8::Value> nsec_value;
  std::string private_key;
  
  if (options->Get(context, gin::StringToV8(isolate, "nsec")).ToLocal(&nsec_value) && 
      nsec_value->IsString()) {
    private_key = gin::V8ToString(isolate, nsec_value);
  } else {
    v8::Local<v8::Value> key_value;
    if (options->Get(context, gin::StringToV8(isolate, "privateKey")).ToLocal(&key_value) && 
        key_value->IsString()) {
      private_key = gin::V8ToString(isolate, key_value);
    }
  }
  
  if (private_key.empty()) {
    return parent_bindings_->CreateErrorPromise(isolate, 
        "Import requires either 'nsec' or 'privateKey' option");
  }
  
  // Extract optional name
  v8::Local<v8::Value> name_value;
  std::string name;
  if (options->Get(context, gin::StringToV8(isolate, "name")).ToLocal(&name_value) && 
      name_value->IsString()) {
    name = gin::V8ToString(isolate, name_value);
  }
  
  // For now, return a rejected promise as the backend doesn't implement import yet
  // TODO: Implement this when NostrService supports account import
  return parent_bindings_->CreateErrorPromise(isolate, 
      "Account import not yet implemented. Use browser settings to import accounts.");
}

}  // namespace tungsten