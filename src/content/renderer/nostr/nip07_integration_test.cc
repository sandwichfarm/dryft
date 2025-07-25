// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/nostr/nostr_bindings.h"

#include "base/test/task_environment.h"
#include "chrome/common/nostr_messages.h"
#include "content/public/test/test_renderer_host.h"
#include "gin/converter.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-promise.h"

namespace dryft {

class Nip07IntegrationTest : public gin::V8Test {
 public:
  Nip07IntegrationTest() = default;
  ~Nip07IntegrationTest() override = default;

  void SetUp() override {
    gin::V8Test::SetUp();
    
    v8::Isolate* isolate = instance_->isolate();
    v8::HandleScope handle_scope(isolate);
    context_.Reset(isolate, v8::Context::New(isolate));
    
    v8::Local<v8::Context> context = 
        v8::Local<v8::Context>::New(isolate, context_);
    v8::Context::Scope context_scope(context);
    
    // Create a mock render frame for testing
    render_frame_ = nullptr;  // In real integration, this would be a mock
  }

  void TearDown() override {
    context_.Reset();
    gin::V8Test::TearDown();
  }

 protected:
  v8::Global<v8::Context> context_;
  content::RenderFrame* render_frame_;
};

// Test that the complete NIP-07 API is available
TEST_F(Nip07IntegrationTest, CompleteNip07ApiAvailable) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = 
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Context::Scope context_scope(context);
  
  // Create nostr bindings
  v8::Local<v8::Value> nostr_value = 
      NostrBindings::Create(isolate, render_frame_);
  ASSERT_FALSE(nostr_value.IsEmpty());
  ASSERT_TRUE(nostr_value->IsObject());
  
  v8::Local<v8::Object> nostr = nostr_value.As<v8::Object>();
  
  // Test all NIP-07 required methods exist
  
  // getPublicKey
  v8::Local<v8::Value> get_pubkey = 
      nostr->Get(context, gin::StringToV8(isolate, "getPublicKey"))
          .ToLocalChecked();
  EXPECT_TRUE(get_pubkey->IsFunction());
  
  // signEvent
  v8::Local<v8::Value> sign_event = 
      nostr->Get(context, gin::StringToV8(isolate, "signEvent"))
          .ToLocalChecked();
  EXPECT_TRUE(sign_event->IsFunction());
  
  // getRelays
  v8::Local<v8::Value> get_relays = 
      nostr->Get(context, gin::StringToV8(isolate, "getRelays"))
          .ToLocalChecked();
  EXPECT_TRUE(get_relays->IsFunction());
  
  // nip04 object
  v8::Local<v8::Value> nip04_value = 
      nostr->Get(context, gin::StringToV8(isolate, "nip04"))
          .ToLocalChecked();
  EXPECT_TRUE(nip04_value->IsObject());
  
  // nip04.encrypt
  v8::Local<v8::Object> nip04 = nip04_value.As<v8::Object>();
  v8::Local<v8::Value> encrypt = 
      nip04->Get(context, gin::StringToV8(isolate, "encrypt"))
          .ToLocalChecked();
  EXPECT_TRUE(encrypt->IsFunction());
  
  // nip04.decrypt
  v8::Local<v8::Value> decrypt = 
      nip04->Get(context, gin::StringToV8(isolate, "decrypt"))
          .ToLocalChecked();
  EXPECT_TRUE(decrypt->IsFunction());
}

// Test that methods return promises as required by NIP-07
TEST_F(Nip07IntegrationTest, MethodsReturnPromises) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = 
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Value> nostr_value = 
      NostrBindings::Create(isolate, render_frame_);
  v8::Local<v8::Object> nostr = nostr_value.As<v8::Object>();
  
  // Test getPublicKey returns promise
  v8::Local<v8::Function> get_pubkey = 
      nostr->Get(context, gin::StringToV8(isolate, "getPublicKey"))
          .ToLocalChecked()
          .As<v8::Function>();
  
  v8::Local<v8::Value> pubkey_result = 
      get_pubkey->Call(context, nostr, 0, nullptr).ToLocalChecked();
  EXPECT_TRUE(pubkey_result->IsPromise());
  
  // Test getRelays returns promise
  v8::Local<v8::Function> get_relays = 
      nostr->Get(context, gin::StringToV8(isolate, "getRelays"))
          .ToLocalChecked()
          .As<v8::Function>();
  
  v8::Local<v8::Value> relays_result = 
      get_relays->Call(context, nostr, 0, nullptr).ToLocalChecked();
  EXPECT_TRUE(relays_result->IsPromise());
}

// Test signEvent input validation
TEST_F(Nip07IntegrationTest, SignEventValidation) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = 
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Value> nostr_value = 
      NostrBindings::Create(isolate, render_frame_);
  v8::Local<v8::Object> nostr = nostr_value.As<v8::Object>();
  
  v8::Local<v8::Function> sign_event = 
      nostr->Get(context, gin::StringToV8(isolate, "signEvent"))
          .ToLocalChecked()
          .As<v8::Function>();
  
  // Test with valid event structure
  v8::Local<v8::Object> valid_event = v8::Object::New(isolate);
  valid_event->Set(context, gin::StringToV8(isolate, "kind"), 
                   v8::Number::New(isolate, 1)).Check();
  valid_event->Set(context, gin::StringToV8(isolate, "content"), 
                   gin::StringToV8(isolate, "Hello Nostr!")).Check();
  
  v8::Local<v8::Value> valid_args[] = { valid_event };
  v8::Local<v8::Value> valid_result = 
      sign_event->Call(context, nostr, 1, valid_args).ToLocalChecked();
  EXPECT_TRUE(valid_result->IsPromise());
  
  // Test with invalid event (missing fields)
  v8::Local<v8::Object> invalid_event = v8::Object::New(isolate);
  v8::Local<v8::Value> invalid_args[] = { invalid_event };
  
  v8::Local<v8::Value> invalid_result = 
      sign_event->Call(context, nostr, 1, invalid_args).ToLocalChecked();
  EXPECT_TRUE(invalid_result->IsPromise());
  
  // Invalid event should result in rejected promise
  v8::Local<v8::Promise> invalid_promise = invalid_result.As<v8::Promise>();
  EXPECT_EQ(v8::Promise::kRejected, invalid_promise->State());
}

// Test NIP-04 encrypt/decrypt parameter validation
TEST_F(Nip07IntegrationTest, Nip04Validation) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = 
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Value> nostr_value = 
      NostrBindings::Create(isolate, render_frame_);
  v8::Local<v8::Object> nostr = nostr_value.As<v8::Object>();
  
  v8::Local<v8::Object> nip04 = 
      nostr->Get(context, gin::StringToV8(isolate, "nip04"))
          .ToLocalChecked()
          .As<v8::Object>();
  
  v8::Local<v8::Function> encrypt = 
      nip04->Get(context, gin::StringToV8(isolate, "encrypt"))
          .ToLocalChecked()
          .As<v8::Function>();
  
  // Test with valid parameters
  v8::Local<v8::Value> valid_pubkey = 
      gin::StringToV8(isolate, "02a1d7d8c77b3a9c8e9f6d5c4b3a2918576f4e3d2c1b0a9876543210fedcba09");
  v8::Local<v8::Value> valid_plaintext = 
      gin::StringToV8(isolate, "Secret message");
  
  v8::Local<v8::Value> valid_encrypt_args[] = { valid_pubkey, valid_plaintext };
  v8::Local<v8::Value> encrypt_result = 
      encrypt->Call(context, nip04, 2, valid_encrypt_args).ToLocalChecked();
  EXPECT_TRUE(encrypt_result->IsPromise());
  
  // Test with empty parameters (should be rejected)
  v8::Local<v8::Value> empty_args[] = { 
      gin::StringToV8(isolate, ""), 
      gin::StringToV8(isolate, "") 
  };
  v8::Local<v8::Value> empty_result = 
      encrypt->Call(context, nip04, 2, empty_args).ToLocalChecked();
  EXPECT_TRUE(empty_result->IsPromise());
  
  v8::Local<v8::Promise> empty_promise = empty_result.As<v8::Promise>();
  EXPECT_EQ(v8::Promise::kRejected, empty_promise->State());
}

}  // namespace dryft