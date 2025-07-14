// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/nostr/nostr_accounts_bindings.h"

#include "content/renderer/nostr/nostr_bindings.h"
#include "gin/dictionary.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace tungsten {

class NostrAccountsBindingsTest : public gin::V8Test {
 protected:
  void SetUp() override {
    gin::V8Test::SetUp();
    
    // Create a mock render frame (nullptr for unit tests)
    render_frame_ = nullptr;
  }

  content::RenderFrame* render_frame_;
};

TEST_F(NostrAccountsBindingsTest, CreateAccountsObject) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Context::Scope context_scope(context);

  // Create the nostr bindings first
  v8::Local<v8::Value> nostr_value = NostrBindings::Create(isolate, render_frame_);
  ASSERT_FALSE(nostr_value.IsEmpty());
  ASSERT_TRUE(nostr_value->IsObject());

  v8::Local<v8::Object> nostr = nostr_value.As<v8::Object>();

  // Check that accounts property exists and is an object
  v8::Local<v8::Value> accounts_value = 
      nostr->Get(context, gin::StringToV8(isolate, "accounts")).ToLocalChecked();
  ASSERT_TRUE(accounts_value->IsObject());
}

TEST_F(NostrAccountsBindingsTest, AccountsMethodsExist) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Context::Scope context_scope(context);

  // Create the nostr bindings
  v8::Local<v8::Value> nostr_value = NostrBindings::Create(isolate, render_frame_);
  v8::Local<v8::Object> nostr = nostr_value.As<v8::Object>();

  // Get the accounts object
  v8::Local<v8::Value> accounts_value = 
      nostr->Get(context, gin::StringToV8(isolate, "accounts")).ToLocalChecked();
  v8::Local<v8::Object> accounts = accounts_value.As<v8::Object>();

  // Check list method exists
  v8::Local<v8::Value> list_value = 
      accounts->Get(context, gin::StringToV8(isolate, "list")).ToLocalChecked();
  ASSERT_TRUE(list_value->IsFunction());

  // Check current method exists
  v8::Local<v8::Value> current_value = 
      accounts->Get(context, gin::StringToV8(isolate, "current")).ToLocalChecked();
  ASSERT_TRUE(current_value->IsFunction());

  // Check switch method exists
  v8::Local<v8::Value> switch_value = 
      accounts->Get(context, gin::StringToV8(isolate, "switch")).ToLocalChecked();
  ASSERT_TRUE(switch_value->IsFunction());

  // Check create method exists
  v8::Local<v8::Value> create_value = 
      accounts->Get(context, gin::StringToV8(isolate, "create")).ToLocalChecked();
  ASSERT_TRUE(create_value->IsFunction());

  // Check import method exists
  v8::Local<v8::Value> import_value = 
      accounts->Get(context, gin::StringToV8(isolate, "import")).ToLocalChecked();
  ASSERT_TRUE(import_value->IsFunction());
}

TEST_F(NostrAccountsBindingsTest, MethodsReturnPromises) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Context::Scope context_scope(context);

  // Create the nostr bindings
  v8::Local<v8::Value> nostr_value = NostrBindings::Create(isolate, render_frame_);
  NostrBindings* nostr_bindings = nullptr;
  ASSERT_TRUE(gin::ConvertFromV8(isolate, nostr_value, &nostr_bindings));

  // Create accounts bindings directly
  v8::Local<v8::Value> accounts_value = 
      NostrAccountsBindings::Create(isolate, nostr_bindings);
  NostrAccountsBindings* accounts_bindings = nullptr;
  ASSERT_TRUE(gin::ConvertFromV8(isolate, accounts_value, &accounts_bindings));

  // Test list() returns a promise
  v8::Local<v8::Promise> list_promise = accounts_bindings->List(isolate);
  ASSERT_FALSE(list_promise.IsEmpty());
  ASSERT_TRUE(list_promise->IsPromise());

  // Test current() returns a promise
  v8::Local<v8::Promise> current_promise = accounts_bindings->Current(isolate);
  ASSERT_FALSE(current_promise.IsEmpty());
  ASSERT_TRUE(current_promise->IsPromise());

  // Test switch() returns a promise
  v8::Local<v8::Promise> switch_promise = 
      accounts_bindings->Switch(isolate, "test-pubkey");
  ASSERT_FALSE(switch_promise.IsEmpty());
  ASSERT_TRUE(switch_promise->IsPromise());

  // Test create() returns a promise
  v8::Local<v8::Object> create_options = v8::Object::New(isolate);
  v8::Local<v8::Promise> create_promise = 
      accounts_bindings->Create(isolate, create_options);
  ASSERT_FALSE(create_promise.IsEmpty());
  ASSERT_TRUE(create_promise->IsPromise());

  // Test import() returns a promise
  v8::Local<v8::Object> import_options = v8::Object::New(isolate);
  import_options->Set(context, 
                     gin::StringToV8(isolate, "nsec"),
                     gin::StringToV8(isolate, "nsec1test")).Check();
  v8::Local<v8::Promise> import_promise = 
      accounts_bindings->Import(isolate, import_options);
  ASSERT_FALSE(import_promise.IsEmpty());
  ASSERT_TRUE(import_promise->IsPromise());
}

TEST_F(NostrAccountsBindingsTest, BackwardCompatibility) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Context::Scope context_scope(context);

  // Create the nostr bindings
  v8::Local<v8::Value> nostr_value = NostrBindings::Create(isolate, render_frame_);
  v8::Local<v8::Object> nostr = nostr_value.As<v8::Object>();

  // Check that old methods still exist on window.nostr
  v8::Local<v8::Value> list_accounts_value = 
      nostr->Get(context, gin::StringToV8(isolate, "listAccounts")).ToLocalChecked();
  ASSERT_TRUE(list_accounts_value->IsFunction());

  v8::Local<v8::Value> get_current_value = 
      nostr->Get(context, gin::StringToV8(isolate, "getCurrentAccount")).ToLocalChecked();
  ASSERT_TRUE(get_current_value->IsFunction());

  v8::Local<v8::Value> switch_account_value = 
      nostr->Get(context, gin::StringToV8(isolate, "switchAccount")).ToLocalChecked();
  ASSERT_TRUE(switch_account_value->IsFunction());
}

}  // namespace tungsten