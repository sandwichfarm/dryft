// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/nostr/nostr_relay_bindings.h"

#include "base/test/task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"

namespace dryft {

class NostrRelayBindingsTest : public gin::V8Test {
 public:
  NostrRelayBindingsTest() = default;
  ~NostrRelayBindingsTest() override = default;

  void SetUp() override {
    gin::V8Test::SetUp();
    
    // Create a mock render frame
    render_frame_host_.reset(new content::RenderViewHostTestHarness());
    render_frame_host_->SetUp();
    render_frame_ = render_frame_host_->GetMainRenderFrameHost();
  }

  void TearDown() override {
    render_frame_ = nullptr;
    render_frame_host_->TearDown();
    render_frame_host_.reset();
    
    gin::V8Test::TearDown();
  }

 protected:
  content::RenderFrameHost* render_frame() { return render_frame_; }

 private:
  std::unique_ptr<content::RenderViewHostTestHarness> render_frame_host_;
  content::RenderFrameHost* render_frame_ = nullptr;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(NostrRelayBindingsTest, CreateRelayObject) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Context::Scope context_scope(context);
  
  // Create relay bindings
  v8::Local<v8::Value> relay_value = NostrRelayBindings::Create(
      isolate, static_cast<content::RenderFrame*>(render_frame()));
  
  ASSERT_FALSE(relay_value.IsEmpty());
  ASSERT_TRUE(relay_value->IsObject());
  
  v8::Local<v8::Object> relay = relay_value.As<v8::Object>();
  
  // Check properties exist
  EXPECT_TRUE(relay->Has(context, gin::StringToV8(isolate, "url")).FromJust());
  EXPECT_TRUE(relay->Has(context, gin::StringToV8(isolate, "connected")).FromJust());
  EXPECT_TRUE(relay->Has(context, gin::StringToV8(isolate, "eventCount")).FromJust());
  EXPECT_TRUE(relay->Has(context, gin::StringToV8(isolate, "storageUsed")).FromJust());
  
  // Check methods exist
  EXPECT_TRUE(relay->Has(context, gin::StringToV8(isolate, "query")).FromJust());
  EXPECT_TRUE(relay->Has(context, gin::StringToV8(isolate, "count")).FromJust());
  EXPECT_TRUE(relay->Has(context, gin::StringToV8(isolate, "deleteEvents")).FromJust());
}

TEST_F(NostrRelayBindingsTest, ReadOnlyProperties) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Context::Scope context_scope(context);
  
  // Create relay bindings
  v8::Local<v8::Value> relay_value = NostrRelayBindings::Create(
      isolate, static_cast<content::RenderFrame*>(render_frame()));
  v8::Local<v8::Object> relay = relay_value.As<v8::Object>();
  
  // Get initial values
  v8::Local<v8::Value> url = relay->Get(context, 
      gin::StringToV8(isolate, "url")).ToLocalChecked();
  v8::Local<v8::Value> connected = relay->Get(context, 
      gin::StringToV8(isolate, "connected")).ToLocalChecked();
  
  EXPECT_TRUE(url->IsString());
  EXPECT_TRUE(connected->IsBoolean());
  
  // Try to set properties (should fail for read-only)
  bool set_result = relay->Set(context, 
      gin::StringToV8(isolate, "url"), 
      gin::StringToV8(isolate, "ws://example.com")).FromJust();
  
  // Property should still have original value
  v8::Local<v8::Value> url_after = relay->Get(context, 
      gin::StringToV8(isolate, "url")).ToLocalChecked();
  EXPECT_TRUE(url->StrictEquals(url_after));
}

TEST_F(NostrRelayBindingsTest, QueryMethod) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Context::Scope context_scope(context);
  
  // Create relay bindings
  v8::Local<v8::Value> relay_value = NostrRelayBindings::Create(
      isolate, static_cast<content::RenderFrame*>(render_frame()));
  v8::Local<v8::Object> relay = relay_value.As<v8::Object>();
  
  // Get query method
  v8::Local<v8::Value> query_value = relay->Get(context, 
      gin::StringToV8(isolate, "query")).ToLocalChecked();
  ASSERT_TRUE(query_value->IsFunction());
  
  v8::Local<v8::Function> query = query_value.As<v8::Function>();
  
  // Create a filter object
  v8::Local<v8::Object> filter = v8::Object::New(isolate);
  filter->Set(context, gin::StringToV8(isolate, "kinds"), 
              v8::Array::New(isolate)).Check();
  filter->Set(context, gin::StringToV8(isolate, "limit"), 
              v8::Number::New(isolate, 10)).Check();
  
  // Call query method
  v8::Local<v8::Value> args[] = { filter };
  v8::Local<v8::Value> result = query->Call(context, relay, 1, args)
      .ToLocalChecked();
  
  // Should return a promise
  EXPECT_TRUE(result->IsPromise());
}

TEST_F(NostrRelayBindingsTest, CountMethod) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Context::Scope context_scope(context);
  
  // Create relay bindings
  v8::Local<v8::Value> relay_value = NostrRelayBindings::Create(
      isolate, static_cast<content::RenderFrame*>(render_frame()));
  v8::Local<v8::Object> relay = relay_value.As<v8::Object>();
  
  // Get count method
  v8::Local<v8::Value> count_value = relay->Get(context, 
      gin::StringToV8(isolate, "count")).ToLocalChecked();
  ASSERT_TRUE(count_value->IsFunction());
  
  v8::Local<v8::Function> count = count_value.As<v8::Function>();
  
  // Create a filter object
  v8::Local<v8::Object> filter = v8::Object::New(isolate);
  filter->Set(context, gin::StringToV8(isolate, "kinds"), 
              v8::Array::New(isolate)).Check();
  
  // Call count method
  v8::Local<v8::Value> args[] = { filter };
  v8::Local<v8::Value> result = count->Call(context, relay, 1, args)
      .ToLocalChecked();
  
  // Should return a promise
  EXPECT_TRUE(result->IsPromise());
}

TEST_F(NostrRelayBindingsTest, DeleteEventsMethod) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Context::Scope context_scope(context);
  
  // Create relay bindings
  v8::Local<v8::Value> relay_value = NostrRelayBindings::Create(
      isolate, static_cast<content::RenderFrame*>(render_frame()));
  v8::Local<v8::Object> relay = relay_value.As<v8::Object>();
  
  // Get deleteEvents method
  v8::Local<v8::Value> delete_value = relay->Get(context, 
      gin::StringToV8(isolate, "deleteEvents")).ToLocalChecked();
  ASSERT_TRUE(delete_value->IsFunction());
  
  v8::Local<v8::Function> deleteEvents = delete_value.As<v8::Function>();
  
  // Create a filter object
  v8::Local<v8::Object> filter = v8::Object::New(isolate);
  filter->Set(context, gin::StringToV8(isolate, "ids"), 
              v8::Array::New(isolate)).Check();
  
  // Call deleteEvents method
  v8::Local<v8::Value> args[] = { filter };
  v8::Local<v8::Value> result = deleteEvents->Call(context, relay, 1, args)
      .ToLocalChecked();
  
  // Should return a promise
  EXPECT_TRUE(result->IsPromise());
}

}  // namespace dryft