// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NOSTR_NOSTR_LIBS_BINDINGS_H_
#define CONTENT_RENDERER_NOSTR_NOSTR_LIBS_BINDINGS_H_

#include <string>

#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace content {
class RenderFrame;
}

namespace tungsten {

// NostrLibsBindings implements the window.nostr.libs object that provides
// paths to bundled Nostr JavaScript libraries.
class NostrLibsBindings : public gin::Wrappable<NostrLibsBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  // Create the libs bindings object
  static v8::Local<v8::Value> Create(v8::Isolate* isolate);

  // Library path getters
  std::string GetNdk() const;
  std::string GetNostrTools() const;
  std::string GetApplesauceCore() const;
  std::string GetApplesauceContent() const;
  std::string GetApplesauceLists() const;
  std::string GetAlbySdk() const;

 private:
  NostrLibsBindings();
  ~NostrLibsBindings() override;

  // gin::Wrappable
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
};

}  // namespace tungsten

#endif  // CONTENT_RENDERER_NOSTR_NOSTR_LIBS_BINDINGS_H_