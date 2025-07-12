// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/nostr/nostr_bindings.h"

#include "base/test/task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-promise.h"

namespace tungsten {

class NostrBindingsTest : public gin::V8Test {
 public:
  NostrBindingsTest() = default;
  ~NostrBindingsTest() override = default;

  void SetUp() override {
    gin::V8Test::SetUp();
    
    v8::Isolate* isolate = instance_->isolate();
    v8::HandleScope handle_scope(isolate);
    context_.Reset(isolate, v8::Context::New(isolate));
    
    v8::Local<v8::Context> context = 
        v8::Local<v8::Context>::New(isolate, context_);
    v8::Context::Scope context_scope(context);
    
    // Create a mock render frame
    render_frame_ = nullptr;  // In real tests, this would be a mock
  }

  void TearDown() override {
    context_.Reset();
    gin::V8Test::TearDown();
  }

 protected:
  v8::Global<v8::Context> context_;
  content::RenderFrame* render_frame_;
};

TEST_F(NostrBindingsTest, CreateNostrObject) {
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
  
  // Check that required methods exist
  v8::Local<v8::Value> get_pubkey = 
      nostr->Get(context, gin::StringToV8(isolate, "getPublicKey"))
          .ToLocalChecked();
  EXPECT_TRUE(get_pubkey->IsFunction());
  
  v8::Local<v8::Value> sign_event = 
      nostr->Get(context, gin::StringToV8(isolate, "signEvent"))
          .ToLocalChecked();
  EXPECT_TRUE(sign_event->IsFunction());
  
  v8::Local<v8::Value> get_relays = 
      nostr->Get(context, gin::StringToV8(isolate, "getRelays"))
          .ToLocalChecked();
  EXPECT_TRUE(get_relays->IsFunction());
  
  v8::Local<v8::Value> nip04 = 
      nostr->Get(context, gin::StringToV8(isolate, "nip04"))
          .ToLocalChecked();
  EXPECT_TRUE(nip04->IsObject());
}

TEST_F(NostrBindingsTest, GetPublicKeyReturnsPromise) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = 
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Context::Scope context_scope(context);
  
  // Create nostr bindings
  v8::Local<v8::Value> nostr_value = 
      NostrBindings::Create(isolate, render_frame_);
  v8::Local<v8::Object> nostr = nostr_value.As<v8::Object>();
  
  // Call getPublicKey
  v8::Local<v8::Function> get_pubkey = 
      nostr->Get(context, gin::StringToV8(isolate, "getPublicKey"))
          .ToLocalChecked()
          .As<v8::Function>();
  
  v8::Local<v8::Value> result = 
      get_pubkey->Call(context, nostr, 0, nullptr).ToLocalChecked();
  
  // Should return a promise
  EXPECT_TRUE(result->IsPromise());
  
  // The promise should be rejected with error message (stub implementation)
  v8::Local<v8::Promise> promise = result.As<v8::Promise>();
  EXPECT_EQ(v8::Promise::kRejected, promise->State());
}

TEST_F(NostrBindingsTest, SignEventValidatesInput) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = 
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Context::Scope context_scope(context);
  
  // Create nostr bindings
  v8::Local<v8::Value> nostr_value = 
      NostrBindings::Create(isolate, render_frame_);
  v8::Local<v8::Object> nostr = nostr_value.As<v8::Object>();
  
  // Get signEvent function
  v8::Local<v8::Function> sign_event = 
      nostr->Get(context, gin::StringToV8(isolate, "signEvent"))
          .ToLocalChecked()
          .As<v8::Function>();
  
  // Test with invalid event (missing fields)
  v8::Local<v8::Object> invalid_event = v8::Object::New(isolate);
  v8::Local<v8::Value> args[] = { invalid_event };
  
  v8::Local<v8::Value> result = 
      sign_event->Call(context, nostr, 1, args).ToLocalChecked();
  
  EXPECT_TRUE(result->IsPromise());
  v8::Local<v8::Promise> promise = result.As<v8::Promise>();
  EXPECT_EQ(v8::Promise::kRejected, promise->State());
}

TEST_F(NostrBindingsTest, Nip04ObjectHasMethods) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = 
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Context::Scope context_scope(context);
  
  // Create nostr bindings
  v8::Local<v8::Value> nostr_value = 
      NostrBindings::Create(isolate, render_frame_);
  v8::Local<v8::Object> nostr = nostr_value.As<v8::Object>();
  
  // Get nip04 object
  v8::Local<v8::Value> nip04_value = 
      nostr->Get(context, gin::StringToV8(isolate, "nip04"))
          .ToLocalChecked();
  ASSERT_TRUE(nip04_value->IsObject());
  
  v8::Local<v8::Object> nip04 = nip04_value.As<v8::Object>();
  
  // Check encrypt method exists
  v8::Local<v8::Value> encrypt = 
      nip04->Get(context, gin::StringToV8(isolate, "encrypt"))
          .ToLocalChecked();
  EXPECT_TRUE(encrypt->IsFunction());
  
  // Check decrypt method exists
  v8::Local<v8::Value> decrypt = 
      nip04->Get(context, gin::StringToV8(isolate, "decrypt"))
          .ToLocalChecked();
  EXPECT_TRUE(decrypt->IsFunction());
}

}  // namespace tungsten