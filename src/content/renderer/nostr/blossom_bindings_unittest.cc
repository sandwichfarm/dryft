// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/nostr/blossom_bindings.h"

#include "base/test/task_environment.h"
#include "content/public/test/render_view_test.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-promise.h"

namespace dryft {

class BlossomBindingsTest : public content::RenderViewTest {
 protected:
  void SetUp() override {
    RenderViewTest::SetUp();
    
    // Get the V8 context
    v8::Isolate* isolate = GetMainFrame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = GetMainFrame()->GetWebFrame()->MainWorldScriptContext();
    v8::Context::Scope context_scope(context);
    
    // Install window.blossom
    v8::Local<v8::Object> global = context->Global();
    BlossomBindings::Install(global, GetMainFrame());
  }
  
  v8::Local<v8::Value> GetBlossom() {
    v8::Isolate* isolate = GetMainFrame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = GetMainFrame()->GetWebFrame()->MainWorldScriptContext();
    v8::Context::Scope context_scope(context);
    
    v8::Local<v8::Object> global = context->Global();
    v8::Local<v8::Value> blossom;
    
    if (!global->Get(context, gin::StringToV8(isolate, "blossom")).ToLocal(&blossom)) {
      return v8::Undefined(isolate);
    }
    
    return blossom;
  }
  
  bool HasMethod(v8::Local<v8::Object> obj, const std::string& method) {
    v8::Isolate* isolate = GetMainFrame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
    v8::Local<v8::Context> context = GetMainFrame()->GetWebFrame()->MainWorldScriptContext();
    
    v8::Local<v8::Value> value;
    if (!obj->Get(context, gin::StringToV8(isolate, method)).ToLocal(&value)) {
      return false;
    }
    
    return value->IsFunction();
  }
  
  bool HasProperty(v8::Local<v8::Object> obj, const std::string& property) {
    v8::Isolate* isolate = GetMainFrame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
    v8::Local<v8::Context> context = GetMainFrame()->GetWebFrame()->MainWorldScriptContext();
    
    return obj->Has(context, gin::StringToV8(isolate, property)).FromMaybe(false);
  }
};

TEST_F(BlossomBindingsTest, BlossomObjectExists) {
  v8::Local<v8::Value> blossom = GetBlossom();
  EXPECT_FALSE(blossom->IsUndefined());
  EXPECT_TRUE(blossom->IsObject());
}

TEST_F(BlossomBindingsTest, CoreMethodsExist) {
  v8::Local<v8::Value> blossom_value = GetBlossom();
  ASSERT_TRUE(blossom_value->IsObject());
  
  v8::Local<v8::Object> blossom = blossom_value.As<v8::Object>();
  
  // Check core methods
  EXPECT_TRUE(HasMethod(blossom, "upload"));
  EXPECT_TRUE(HasMethod(blossom, "uploadMultiple"));
  EXPECT_TRUE(HasMethod(blossom, "get"));
  EXPECT_TRUE(HasMethod(blossom, "getUrl"));
  EXPECT_TRUE(HasMethod(blossom, "has"));
  EXPECT_TRUE(HasMethod(blossom, "hasMultiple"));
  EXPECT_TRUE(HasMethod(blossom, "mirror"));
  EXPECT_TRUE(HasMethod(blossom, "createAuth"));
}

TEST_F(BlossomBindingsTest, PropertiesExist) {
  v8::Local<v8::Value> blossom_value = GetBlossom();
  ASSERT_TRUE(blossom_value->IsObject());
  
  v8::Local<v8::Object> blossom = blossom_value.As<v8::Object>();
  
  // Check properties
  EXPECT_TRUE(HasProperty(blossom, "servers"));
  EXPECT_TRUE(HasProperty(blossom, "local"));
}

TEST_F(BlossomBindingsTest, ServersObjectStructure) {
  v8::Isolate* isolate = GetMainFrame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetMainFrame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Value> blossom_value = GetBlossom();
  ASSERT_TRUE(blossom_value->IsObject());
  
  v8::Local<v8::Object> blossom = blossom_value.As<v8::Object>();
  v8::Local<v8::Value> servers_value;
  ASSERT_TRUE(blossom->Get(context, gin::StringToV8(isolate, "servers")).ToLocal(&servers_value));
  ASSERT_TRUE(servers_value->IsObject());
  
  v8::Local<v8::Object> servers = servers_value.As<v8::Object>();
  
  // Check server management methods
  EXPECT_TRUE(HasMethod(servers, "list"));
  EXPECT_TRUE(HasMethod(servers, "add"));
  EXPECT_TRUE(HasMethod(servers, "remove"));
  EXPECT_TRUE(HasMethod(servers, "test"));
}

TEST_F(BlossomBindingsTest, LocalObjectStructure) {
  v8::Isolate* isolate = GetMainFrame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetMainFrame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Value> blossom_value = GetBlossom();
  ASSERT_TRUE(blossom_value->IsObject());
  
  v8::Local<v8::Object> blossom = blossom_value.As<v8::Object>();
  v8::Local<v8::Value> local_value;
  ASSERT_TRUE(blossom->Get(context, gin::StringToV8(isolate, "local")).ToLocal(&local_value));
  ASSERT_TRUE(local_value->IsObject());
  
  v8::Local<v8::Object> local = local_value.As<v8::Object>();
  
  // Check local server properties
  EXPECT_TRUE(HasProperty(local, "url"));
  EXPECT_TRUE(HasProperty(local, "enabled"));
  EXPECT_TRUE(HasProperty(local, "storageUsed"));
  EXPECT_TRUE(HasProperty(local, "fileCount"));
  
  // Check local server methods
  EXPECT_TRUE(HasMethod(local, "clear"));
  EXPECT_TRUE(HasMethod(local, "prune"));
  EXPECT_TRUE(HasMethod(local, "setQuota"));
}

TEST_F(BlossomBindingsTest, GetUrlReturnsString) {
  v8::Isolate* isolate = GetMainFrame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetMainFrame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Value> blossom_value = GetBlossom();
  ASSERT_TRUE(blossom_value->IsObject());
  
  v8::Local<v8::Object> blossom = blossom_value.As<v8::Object>();
  v8::Local<v8::Value> getUrl_value;
  ASSERT_TRUE(blossom->Get(context, gin::StringToV8(isolate, "getUrl")).ToLocal(&getUrl_value));
  ASSERT_TRUE(getUrl_value->IsFunction());
  
  v8::Local<v8::Function> getUrl = getUrl_value.As<v8::Function>();
  
  // Call getUrl with a test hash
  v8::Local<v8::Value> args[] = {gin::StringToV8(isolate, "testhash123")};
  v8::Local<v8::Value> result;
  ASSERT_TRUE(getUrl->Call(context, blossom, 1, args).ToLocal(&result));
  
  EXPECT_TRUE(result->IsString());
  std::string url = gin::V8ToString(isolate, result);
  EXPECT_EQ(url, "http://localhost:8080/testhash123");
}

TEST_F(BlossomBindingsTest, UploadReturnsPromise) {
  v8::Isolate* isolate = GetMainFrame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetMainFrame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Value> blossom_value = GetBlossom();
  ASSERT_TRUE(blossom_value->IsObject());
  
  v8::Local<v8::Object> blossom = blossom_value.As<v8::Object>();
  v8::Local<v8::Value> upload_value;
  ASSERT_TRUE(blossom->Get(context, gin::StringToV8(isolate, "upload")).ToLocal(&upload_value));
  ASSERT_TRUE(upload_value->IsFunction());
  
  v8::Local<v8::Function> upload = upload_value.As<v8::Function>();
  
  // Create a test ArrayBuffer
  v8::Local<v8::ArrayBuffer> buffer = v8::ArrayBuffer::New(isolate, 10);
  
  // Call upload
  v8::Local<v8::Value> args[] = {buffer};
  v8::Local<v8::Value> result;
  ASSERT_TRUE(upload->Call(context, blossom, 1, args).ToLocal(&result));
  
  EXPECT_TRUE(result->IsPromise());
}

TEST_F(BlossomBindingsTest, GetReturnsPromise) {
  v8::Isolate* isolate = GetMainFrame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetMainFrame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Value> blossom_value = GetBlossom();
  ASSERT_TRUE(blossom_value->IsObject());
  
  v8::Local<v8::Object> blossom = blossom_value.As<v8::Object>();
  v8::Local<v8::Value> get_value;
  ASSERT_TRUE(blossom->Get(context, gin::StringToV8(isolate, "get")).ToLocal(&get_value));
  ASSERT_TRUE(get_value->IsFunction());
  
  v8::Local<v8::Function> get = get_value.As<v8::Function>();
  
  // Call get with a test hash
  v8::Local<v8::Value> args[] = {gin::StringToV8(isolate, "testhash123")};
  v8::Local<v8::Value> result;
  ASSERT_TRUE(get->Call(context, blossom, 1, args).ToLocal(&result));
  
  EXPECT_TRUE(result->IsPromise());
}

TEST_F(BlossomBindingsTest, HasReturnsPromise) {
  v8::Isolate* isolate = GetMainFrame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetMainFrame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Value> blossom_value = GetBlossom();
  ASSERT_TRUE(blossom_value->IsObject());
  
  v8::Local<v8::Object> blossom = blossom_value.As<v8::Object>();
  v8::Local<v8::Value> has_value;
  ASSERT_TRUE(blossom->Get(context, gin::StringToV8(isolate, "has")).ToLocal(&has_value));
  ASSERT_TRUE(has_value->IsFunction());
  
  v8::Local<v8::Function> has = has_value.As<v8::Function>();
  
  // Call has with a test hash
  v8::Local<v8::Value> args[] = {gin::StringToV8(isolate, "testhash123")};
  v8::Local<v8::Value> result;
  ASSERT_TRUE(has->Call(context, blossom, 1, args).ToLocal(&result));
  
  EXPECT_TRUE(result->IsPromise());
}

TEST_F(BlossomBindingsTest, ServersListReturnsPromise) {
  v8::Isolate* isolate = GetMainFrame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetMainFrame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Value> blossom_value = GetBlossom();
  ASSERT_TRUE(blossom_value->IsObject());
  
  v8::Local<v8::Object> blossom = blossom_value.As<v8::Object>();
  v8::Local<v8::Value> servers_value;
  ASSERT_TRUE(blossom->Get(context, gin::StringToV8(isolate, "servers")).ToLocal(&servers_value));
  ASSERT_TRUE(servers_value->IsObject());
  
  v8::Local<v8::Object> servers = servers_value.As<v8::Object>();
  v8::Local<v8::Value> list_value;
  ASSERT_TRUE(servers->Get(context, gin::StringToV8(isolate, "list")).ToLocal(&list_value));
  ASSERT_TRUE(list_value->IsFunction());
  
  v8::Local<v8::Function> list = list_value.As<v8::Function>();
  
  // Call list
  v8::Local<v8::Value> result;
  ASSERT_TRUE(list->Call(context, servers, 0, nullptr).ToLocal(&result));
  
  EXPECT_TRUE(result->IsPromise());
}

TEST_F(BlossomBindingsTest, LocalUrlReturnsString) {
  v8::Isolate* isolate = GetMainFrame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetMainFrame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Value> blossom_value = GetBlossom();
  ASSERT_TRUE(blossom_value->IsObject());
  
  v8::Local<v8::Object> blossom = blossom_value.As<v8::Object>();
  v8::Local<v8::Value> local_value;
  ASSERT_TRUE(blossom->Get(context, gin::StringToV8(isolate, "local")).ToLocal(&local_value));
  ASSERT_TRUE(local_value->IsObject());
  
  v8::Local<v8::Object> local = local_value.As<v8::Object>();
  v8::Local<v8::Value> url_value;
  ASSERT_TRUE(local->Get(context, gin::StringToV8(isolate, "url")).ToLocal(&url_value));
  
  EXPECT_TRUE(url_value->IsString());
  std::string url = gin::V8ToString(isolate, url_value);
  EXPECT_EQ(url, "http://localhost:8080");
}

TEST_F(BlossomBindingsTest, CreateAuthReturnsPromise) {
  v8::Isolate* isolate = GetMainFrame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetMainFrame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Value> blossom_value = GetBlossom();
  ASSERT_TRUE(blossom_value->IsObject());
  
  v8::Local<v8::Object> blossom = blossom_value.As<v8::Object>();
  v8::Local<v8::Value> createAuth_value;
  ASSERT_TRUE(blossom->Get(context, gin::StringToV8(isolate, "createAuth")).ToLocal(&createAuth_value));
  ASSERT_TRUE(createAuth_value->IsFunction());
  
  v8::Local<v8::Function> createAuth = createAuth_value.As<v8::Function>();
  
  // Create params object
  v8::Local<v8::Object> params = v8::Object::New(isolate);
  params->Set(context, gin::StringToV8(isolate, "verb"), gin::StringToV8(isolate, "upload")).Check();
  
  // Call createAuth
  v8::Local<v8::Value> args[] = {params};
  v8::Local<v8::Value> result;
  ASSERT_TRUE(createAuth->Call(context, blossom, 1, args).ToLocal(&result));
  
  EXPECT_TRUE(result->IsPromise());
}

TEST_F(BlossomBindingsTest, MirrorReturnsPromise) {
  v8::Isolate* isolate = GetMainFrame()->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = GetMainFrame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Value> blossom_value = GetBlossom();
  ASSERT_TRUE(blossom_value->IsObject());
  
  v8::Local<v8::Object> blossom = blossom_value.As<v8::Object>();
  v8::Local<v8::Value> mirror_value;
  ASSERT_TRUE(blossom->Get(context, gin::StringToV8(isolate, "mirror")).ToLocal(&mirror_value));
  ASSERT_TRUE(mirror_value->IsFunction());
  
  v8::Local<v8::Function> mirror = mirror_value.As<v8::Function>();
  
  // Call mirror with a test hash
  v8::Local<v8::Value> args[] = {gin::StringToV8(isolate, "testhash123")};
  v8::Local<v8::Value> result;
  ASSERT_TRUE(mirror->Call(context, blossom, 1, args).ToLocal(&result));
  
  EXPECT_TRUE(result->IsPromise());
}

}  // namespace dryft