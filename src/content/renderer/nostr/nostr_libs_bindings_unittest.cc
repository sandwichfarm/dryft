// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/nostr/nostr_libs_bindings.h"

#include "gin/dictionary.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace dryft {

using NostrLibsBindingsTest = gin::V8Test;

TEST_F(NostrLibsBindingsTest, CreateLibsObject) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Context::Scope context_scope(context);

  // Create the libs object
  v8::Local<v8::Value> libs_value = NostrLibsBindings::Create(isolate);
  ASSERT_FALSE(libs_value.IsEmpty());
  ASSERT_TRUE(libs_value->IsObject());

  v8::Local<v8::Object> libs = libs_value.As<v8::Object>();

  // Check that ndk property exists and is a string
  v8::Local<v8::Value> ndk_value = 
      libs->Get(context, gin::StringToV8(isolate, "ndk")).ToLocalChecked();
  ASSERT_TRUE(ndk_value->IsString());
  EXPECT_EQ("chrome://resources/js/nostr/ndk.js", 
            gin::V8ToString(isolate, ndk_value));

  // Check nostr-tools property
  v8::Local<v8::Value> nostr_tools_value = 
      libs->Get(context, gin::StringToV8(isolate, "nostr-tools")).ToLocalChecked();
  ASSERT_TRUE(nostr_tools_value->IsString());
  EXPECT_EQ("chrome://resources/js/nostr/nostr-tools.js", 
            gin::V8ToString(isolate, nostr_tools_value));

  // Check applesauce-core property
  v8::Local<v8::Value> applesauce_core_value = 
      libs->Get(context, gin::StringToV8(isolate, "applesauce-core")).ToLocalChecked();
  ASSERT_TRUE(applesauce_core_value->IsString());
  EXPECT_EQ("chrome://resources/js/nostr/applesauce-core.js", 
            gin::V8ToString(isolate, applesauce_core_value));

  // Check alby-sdk property
  v8::Local<v8::Value> alby_sdk_value = 
      libs->Get(context, gin::StringToV8(isolate, "alby-sdk")).ToLocalChecked();
  ASSERT_TRUE(alby_sdk_value->IsString());
  EXPECT_EQ("chrome://resources/js/nostr/alby-sdk.js", 
            gin::V8ToString(isolate, alby_sdk_value));
}

TEST_F(NostrLibsBindingsTest, LibraryUrlsAreReadOnly) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Context::Scope context_scope(context);

  // Create the libs object
  v8::Local<v8::Value> libs_value = NostrLibsBindings::Create(isolate);
  v8::Local<v8::Object> libs = libs_value.As<v8::Object>();

  // Try to modify the ndk property
  v8::Local<v8::String> new_value = gin::StringToV8(isolate, "modified");
  v8::Maybe<bool> set_result = libs->Set(context, 
                                         gin::StringToV8(isolate, "ndk"), 
                                         new_value);
  
  // The set should succeed (V8 doesn't throw by default)
  ASSERT_TRUE(set_result.IsJust());

  // But the value should remain unchanged due to property configuration
  v8::Local<v8::Value> ndk_value = 
      libs->Get(context, gin::StringToV8(isolate, "ndk")).ToLocalChecked();
  EXPECT_EQ("chrome://resources/js/nostr/ndk.js", 
            gin::V8ToString(isolate, ndk_value));
}

TEST_F(NostrLibsBindingsTest, VersionsObjectExists) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Context::Scope context_scope(context);

  // Create the libs object
  v8::Local<v8::Value> libs_value = NostrLibsBindings::Create(isolate);
  v8::Local<v8::Object> libs = libs_value.As<v8::Object>();

  // Check that versions property exists and is an object
  v8::Local<v8::Value> versions_value = 
      libs->Get(context, gin::StringToV8(isolate, "versions")).ToLocalChecked();
  ASSERT_TRUE(versions_value->IsObject());

  v8::Local<v8::Object> versions = versions_value.As<v8::Object>();

  // Check NDK version
  v8::Local<v8::Value> ndk_version = 
      versions->Get(context, gin::StringToV8(isolate, "ndk")).ToLocalChecked();
  ASSERT_TRUE(ndk_version->IsString());
  EXPECT_EQ("2.0.0", gin::V8ToString(isolate, ndk_version));

  // Check nostr-tools version
  v8::Local<v8::Value> nostr_tools_version = 
      versions->Get(context, gin::StringToV8(isolate, "nostr-tools")).ToLocalChecked();
  ASSERT_TRUE(nostr_tools_version->IsString());
  EXPECT_EQ("1.17.0", gin::V8ToString(isolate, nostr_tools_version));

  // Check alby-sdk version
  v8::Local<v8::Value> alby_version = 
      versions->Get(context, gin::StringToV8(isolate, "alby-sdk")).ToLocalChecked();
  ASSERT_TRUE(alby_version->IsString());
  EXPECT_EQ("3.0.0", gin::V8ToString(isolate, alby_version));
}

TEST_F(NostrLibsBindingsTest, AllLibrariesPresent) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);
  v8::Context::Scope context_scope(context);

  // Create the libs object
  v8::Local<v8::Value> libs_value = NostrLibsBindings::Create(isolate);
  v8::Local<v8::Object> libs = libs_value.As<v8::Object>();

  // List of expected libraries
  const char* expected_libs[] = {
    "ndk",
    "nostr-tools", 
    "applesauce-core",
    "applesauce-content",
    "applesauce-lists",
    "alby-sdk"
  };

  // Check each library exists and has a chrome:// URL
  for (const char* lib_name : expected_libs) {
    v8::Local<v8::Value> lib_value = 
        libs->Get(context, gin::StringToV8(isolate, lib_name)).ToLocalChecked();
    ASSERT_TRUE(lib_value->IsString()) << "Library missing: " << lib_name;
    
    std::string url = gin::V8ToString(isolate, lib_value);
    EXPECT_TRUE(url.find("chrome://resources/js/nostr/") == 0) 
        << "Invalid URL for " << lib_name << ": " << url;
    EXPECT_TRUE(url.find(".js") != std::string::npos)
        << "URL missing .js extension for " << lib_name << ": " << url;
  }
}

}  // namespace dryft