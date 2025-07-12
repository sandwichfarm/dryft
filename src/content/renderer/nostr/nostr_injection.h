// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NOSTR_NOSTR_INJECTION_H_
#define CONTENT_RENDERER_NOSTR_NOSTR_INJECTION_H_

#include "v8/include/v8-forward.h"

namespace content {
class RenderFrame;
}

namespace tungsten {

// NostrInjection handles injecting the window.nostr object into
// web page contexts at the appropriate time.
class NostrInjection {
 public:
  // Called when a new script context is created
  // This is where we inject window.nostr
  static void DidCreateScriptContext(content::RenderFrame* render_frame,
                                    v8::Local<v8::Context> context,
                                    int32_t world_id);

  // Check if Nostr injection is enabled
  static bool IsNostrEnabled();

 private:
  NostrInjection() = delete;
  ~NostrInjection() = delete;
};

}  // namespace tungsten

#endif  // CONTENT_RENDERER_NOSTR_NOSTR_INJECTION_H_