// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/nostr/nostr_libs_bindings.h"

#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/object_template_builder.h"

namespace tungsten {

gin::WrapperInfo NostrLibsBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
v8::Local<v8::Value> NostrLibsBindings::Create(v8::Isolate* isolate) {
  return gin::CreateHandle(isolate, new NostrLibsBindings()).ToV8();
}

NostrLibsBindings::NostrLibsBindings() = default;
NostrLibsBindings::~NostrLibsBindings() = default;

gin::ObjectTemplateBuilder NostrLibsBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<NostrLibsBindings>::GetObjectTemplateBuilder(isolate)
      .SetProperty("ndk", &NostrLibsBindings::GetNdk)
      .SetProperty("nostr-tools", &NostrLibsBindings::GetNostrTools)
      .SetProperty("applesauce-core", &NostrLibsBindings::GetApplesauceCore)
      .SetProperty("applesauce-content", &NostrLibsBindings::GetApplesauceContent)
      .SetProperty("applesauce-lists", &NostrLibsBindings::GetApplesauceLists)
      .SetProperty("alby-sdk", &NostrLibsBindings::GetAlbySdk)
      .SetProperty("versions", &NostrLibsBindings::GetVersions);
}

std::string NostrLibsBindings::GetNdk() const {
  return "chrome://resources/js/nostr/ndk.js";
}

std::string NostrLibsBindings::GetNostrTools() const {
  return "chrome://resources/js/nostr/nostr-tools.js";
}

std::string NostrLibsBindings::GetApplesauceCore() const {
  return "chrome://resources/js/nostr/applesauce-core.js";
}

std::string NostrLibsBindings::GetApplesauceContent() const {
  return "chrome://resources/js/nostr/applesauce-content.js";
}

std::string NostrLibsBindings::GetApplesauceLists() const {
  return "chrome://resources/js/nostr/applesauce-lists.js";
}

std::string NostrLibsBindings::GetAlbySdk() const {
  return "chrome://resources/js/nostr/alby-sdk.js";
}

v8::Local<v8::Object> NostrLibsBindings::GetVersions(v8::Isolate* isolate) const {
  gin::Dictionary versions(isolate);
  
  // These version numbers should match what's in library_config.py
  versions.Set("ndk", "2.0.0");
  versions.Set("nostr-tools", "1.17.0");
  versions.Set("applesauce-core", "0.3.4");
  versions.Set("applesauce-content", "0.3.4");
  versions.Set("applesauce-lists", "0.3.4");
  versions.Set("alby-sdk", "3.0.0");
  
  return versions.GetHandle();
}

}  // namespace tungsten