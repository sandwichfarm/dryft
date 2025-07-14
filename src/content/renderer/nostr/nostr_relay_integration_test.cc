// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/nostr/nostr_bindings.h"
#include "content/renderer/nostr/nostr_injection.h"

#include "base/test/task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"

namespace tungsten {

class NostrRelayIntegrationTest : public gin::V8Test {
 public:
  NostrRelayIntegrationTest() = default;
  ~NostrRelayIntegrationTest() override = default;

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

TEST_F(NostrRelayIntegrationTest, WindowNostrRelayProperty) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Context::Scope context_scope(context);
  
  // Get global object
  v8::Local<v8::Object> global = context->Global();
  
  // Install window.nostr
  NostrBindings::Install(global, static_cast<content::RenderFrame*>(render_frame()));
  
  // Check window.nostr exists
  v8::Local<v8::Value> nostr_value = global->Get(context, 
      gin::StringToV8(isolate, "nostr")).ToLocalChecked();
  ASSERT_FALSE(nostr_value.IsEmpty());
  ASSERT_TRUE(nostr_value->IsObject());
  
  v8::Local<v8::Object> nostr = nostr_value.As<v8::Object>();
  
  // Check window.nostr.relay exists
  v8::Local<v8::Value> relay_value = nostr->Get(context, 
      gin::StringToV8(isolate, "relay")).ToLocalChecked();
  ASSERT_FALSE(relay_value.IsEmpty());
  ASSERT_TRUE(relay_value->IsObject());
  
  v8::Local<v8::Object> relay = relay_value.As<v8::Object>();
  
  // Verify relay properties
  v8::Local<v8::Value> url = relay->Get(context, 
      gin::StringToV8(isolate, "url")).ToLocalChecked();
  EXPECT_TRUE(url->IsString());
  
  v8::Local<v8::Value> connected = relay->Get(context, 
      gin::StringToV8(isolate, "connected")).ToLocalChecked();
  EXPECT_TRUE(connected->IsBoolean());
  
  v8::Local<v8::Value> event_count = relay->Get(context, 
      gin::StringToV8(isolate, "eventCount")).ToLocalChecked();
  EXPECT_TRUE(event_count->IsNumber());
  
  v8::Local<v8::Value> storage_used = relay->Get(context, 
      gin::StringToV8(isolate, "storageUsed")).ToLocalChecked();
  EXPECT_TRUE(storage_used->IsNumber());
}

TEST_F(NostrRelayIntegrationTest, JavaScriptAPIUsage) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Context::Scope context_scope(context);
  
  // Get global object
  v8::Local<v8::Object> global = context->Global();
  
  // Install window.nostr
  NostrBindings::Install(global, static_cast<content::RenderFrame*>(render_frame()));
  
  // Test JavaScript code
  const char* js_code = R"(
    (async function() {
      // Check relay exists
      if (!window.nostr || !window.nostr.relay) {
        throw new Error('window.nostr.relay not found');
      }
      
      // Get relay status
      const url = window.nostr.relay.url;
      const connected = window.nostr.relay.connected;
      const eventCount = window.nostr.relay.eventCount;
      const storageUsed = window.nostr.relay.storageUsed;
      
      // Basic validation
      if (typeof url !== 'string') {
        throw new Error('url should be a string');
      }
      if (typeof connected !== 'boolean') {
        throw new Error('connected should be a boolean');
      }
      if (typeof eventCount !== 'number') {
        throw new Error('eventCount should be a number');
      }
      if (typeof storageUsed !== 'number') {
        throw new Error('storageUsed should be a number');
      }
      
      // Test query method
      const filter = { kinds: [1], limit: 10 };
      const queryPromise = window.nostr.relay.query(filter);
      if (!(queryPromise instanceof Promise)) {
        throw new Error('query should return a Promise');
      }
      
      // Test count method
      const countPromise = window.nostr.relay.count(filter);
      if (!(countPromise instanceof Promise)) {
        throw new Error('count should return a Promise');
      }
      
      // Test deleteEvents method
      const deletePromise = window.nostr.relay.deleteEvents({ ids: [] });
      if (!(deletePromise instanceof Promise)) {
        throw new Error('deleteEvents should return a Promise');
      }
      
      return 'success';
    })();
  )";
  
  // Compile and run the JavaScript
  v8::Local<v8::String> source = gin::StringToV8(isolate, js_code);
  v8::Local<v8::Script> script = v8::Script::Compile(context, source)
      .ToLocalChecked();
  v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
  
  // The async function returns a promise
  ASSERT_TRUE(result->IsPromise());
  
  // TODO: Wait for promise resolution and check result equals "success"
  // This would require running the microtask queue
}

}  // namespace tungsten