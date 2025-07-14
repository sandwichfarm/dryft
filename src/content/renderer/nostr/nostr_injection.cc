// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/nostr/nostr_injection.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/nostr/blossom_bindings.h"
#include "content/renderer/nostr/nostr_bindings.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"

// Feature flag for Nostr support
BASE_FEATURE(kNostrSupport, "NostrSupport", base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag for Blossom support
BASE_FEATURE(kBlossomSupport, "BlossomSupport", base::FEATURE_ENABLED_BY_DEFAULT);

namespace tungsten {

// static
void NostrInjection::DidCreateScriptContext(content::RenderFrame* render_frame,
                                           v8::Local<v8::Context> context,
                                           int32_t world_id) {
  // Only inject in the main world
  if (world_id != 0) {
    VLOG(3) << "Skipping Nostr injection in isolated world: " << world_id;
    return;
  }
  
  // Check if Nostr is enabled
  if (!IsNostrEnabled()) {
    VLOG(2) << "Nostr support is disabled";
    return;
  }
  
  // Don't inject if there's no render frame
  if (!render_frame) {
    return;
  }
  
  // Get the web frame
  blink::WebLocalFrame* web_frame = render_frame->GetWebFrame();
  if (!web_frame) {
    return;
  }
  
  // Don't inject in subframes for now
  if (!web_frame->IsMainFrame()) {
    VLOG(3) << "Skipping Nostr injection in subframe";
    return;
  }
  
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context);
  
  // Get the global object
  v8::Local<v8::Object> global = context->Global();
  
  // Install window.nostr
  NostrBindings::Install(global, render_frame);
  
  // Install window.blossom if enabled
  if (base::FeatureList::IsEnabled(kBlossomSupport)) {
    BlossomBindings::Install(global, render_frame);
    VLOG(1) << "Nostr and Blossom injection completed for frame";
  } else {
    VLOG(1) << "Nostr injection completed for frame (Blossom disabled)";
  }
}

// static
bool NostrInjection::IsNostrEnabled() {
  // Check feature flag
  if (!base::FeatureList::IsEnabled(kNostrSupport)) {
    return false;
  }
  
  // Check command line flag (for development/testing)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch("disable-nostr")) {
    return false;
  }
  
  return true;
}

}  // namespace tungsten