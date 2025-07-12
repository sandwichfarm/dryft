// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/nostr/nostr_bindings.h"

#include "base/logging.h"
#include "content/public/renderer/render_frame.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/function_template.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-promise.h"

namespace tungsten {

gin::WrapperInfo NostrBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

NostrBindings::NostrBindings(content::RenderFrame* render_frame)
    : render_frame_(render_frame) {
  DCHECK(render_frame_);
}

NostrBindings::~NostrBindings() = default;

// static
void NostrBindings::Install(v8::Local<v8::Object> global,
                           content::RenderFrame* render_frame) {
  v8::Isolate* isolate = global->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  
  // Check if we should inject window.nostr
  blink::WebLocalFrame* web_frame = render_frame->GetWebFrame();
  if (!web_frame) {
    return;
  }
  
  // Don't inject in chrome:// URLs or other privileged contexts
  GURL url(web_frame->GetDocument().Url());
  if (url.SchemeIs("chrome") || url.SchemeIs("chrome-extension") ||
      url.SchemeIs("devtools") || url.SchemeIs("chrome-search")) {
    return;
  }
  
  // Create the nostr object
  v8::Local<v8::Value> nostr_value = Create(isolate, render_frame);
  if (nostr_value.IsEmpty()) {
    LOG(ERROR) << "Failed to create window.nostr object";
    return;
  }
  
  // Install as window.nostr
  gin::Dictionary global_dict(isolate, global);
  global_dict.Set("nostr", nostr_value);
  
  VLOG(1) << "Installed window.nostr for origin: " << url.DeprecatedGetOriginAsURL();
}

// static
v8::Local<v8::Value> NostrBindings::Create(v8::Isolate* isolate,
                                           content::RenderFrame* render_frame) {
  gin::Handle<NostrBindings> handle = 
      gin::CreateHandle(isolate, new NostrBindings(render_frame));
  if (handle.IsEmpty()) {
    return v8::Local<v8::Value>();
  }
  return handle.ToV8();
}

gin::ObjectTemplateBuilder NostrBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<NostrBindings>::GetObjectTemplateBuilder(isolate)
      .SetMethod("getPublicKey", &NostrBindings::GetPublicKey)
      .SetMethod("signEvent", &NostrBindings::SignEvent)
      .SetMethod("getRelays", &NostrBindings::GetRelays)
      .SetLazyDataProperty("nip04", &NostrBindings::GetNip04Object);
}

v8::Local<v8::Promise> NostrBindings::GetPublicKey(v8::Isolate* isolate) {
  VLOG(2) << "window.nostr.getPublicKey() called";
  
  if (!IsOriginAllowed()) {
    return CreateErrorPromise(isolate, "Origin not allowed");
  }
  
  // For now, return an error as this is a stub implementation
  return CreateErrorPromise(isolate, 
      "NIP-07 not yet implemented. This is a stub implementation.");
}

v8::Local<v8::Promise> NostrBindings::SignEvent(v8::Isolate* isolate,
                                               v8::Local<v8::Object> event) {
  VLOG(2) << "window.nostr.signEvent() called";
  
  if (!IsOriginAllowed()) {
    return CreateErrorPromise(isolate, "Origin not allowed");
  }
  
  // Validate event object has required fields
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> kind_value;
  if (!event->Get(context, gin::StringToV8(isolate, "kind"))
          .ToLocal(&kind_value) || !kind_value->IsNumber()) {
    return CreateErrorPromise(isolate, "Invalid event: missing 'kind' field");
  }
  
  v8::Local<v8::Value> content_value;
  if (!event->Get(context, gin::StringToV8(isolate, "content"))
          .ToLocal(&content_value) || !content_value->IsString()) {
    return CreateErrorPromise(isolate, "Invalid event: missing 'content' field");
  }
  
  // For now, return an error as this is a stub implementation
  return CreateErrorPromise(isolate, 
      "NIP-07 not yet implemented. This is a stub implementation.");
}

v8::Local<v8::Promise> NostrBindings::GetRelays(v8::Isolate* isolate) {
  VLOG(2) << "window.nostr.getRelays() called";
  
  if (!IsOriginAllowed()) {
    return CreateErrorPromise(isolate, "Origin not allowed");
  }
  
  // For now, return an error as this is a stub implementation
  return CreateErrorPromise(isolate, 
      "NIP-07 not yet implemented. This is a stub implementation.");
}

v8::Local<v8::Object> NostrBindings::GetNip04Object(v8::Isolate* isolate) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> nip04 = v8::Object::New(isolate);
  
  // Create encrypt function
  v8::Local<v8::Function> encrypt_func = 
      gin::CreateFunctionTemplate(
          isolate,
          base::BindRepeating(&NostrBindings::Nip04Encrypt,
                            weak_factory_.GetWeakPtr()))
          ->GetFunction(context).ToLocalChecked();
  
  // Create decrypt function  
  v8::Local<v8::Function> decrypt_func =
      gin::CreateFunctionTemplate(
          isolate,
          base::BindRepeating(&NostrBindings::Nip04Decrypt,
                            weak_factory_.GetWeakPtr()))
          ->GetFunction(context).ToLocalChecked();
  
  // Set functions on nip04 object
  nip04->Set(context, gin::StringToV8(isolate, "encrypt"), encrypt_func)
      .Check();
  nip04->Set(context, gin::StringToV8(isolate, "decrypt"), decrypt_func)
      .Check();
  
  return nip04;
}

v8::Local<v8::Promise> NostrBindings::Nip04Encrypt(
    v8::Isolate* isolate,
    const std::string& pubkey,
    const std::string& plaintext) {
  VLOG(2) << "window.nostr.nip04.encrypt() called";
  
  if (!IsOriginAllowed()) {
    return CreateErrorPromise(isolate, "Origin not allowed");
  }
  
  // Validate inputs
  if (pubkey.empty() || plaintext.empty()) {
    return CreateErrorPromise(isolate, "Invalid parameters");
  }
  
  // For now, return an error as this is a stub implementation
  return CreateErrorPromise(isolate, 
      "NIP-04 encryption not yet implemented. This is a stub implementation.");
}

v8::Local<v8::Promise> NostrBindings::Nip04Decrypt(
    v8::Isolate* isolate,
    const std::string& pubkey,
    const std::string& ciphertext) {
  VLOG(2) << "window.nostr.nip04.decrypt() called";
  
  if (!IsOriginAllowed()) {
    return CreateErrorPromise(isolate, "Origin not allowed");
  }
  
  // Validate inputs
  if (pubkey.empty() || ciphertext.empty()) {
    return CreateErrorPromise(isolate, "Invalid parameters");
  }
  
  // For now, return an error as this is a stub implementation
  return CreateErrorPromise(isolate, 
      "NIP-04 decryption not yet implemented. This is a stub implementation.");
}

bool NostrBindings::IsOriginAllowed() const {
  if (!render_frame_) {
    return false;
  }
  
  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();
  if (!web_frame) {
    return false;
  }
  
  // Get the origin
  blink::WebSecurityOrigin security_origin = 
      web_frame->GetDocument().GetSecurityOrigin();
  
  // For now, allow all non-privileged origins
  // In the future, this should check user permissions
  return !security_origin.IsOpaque() && 
         security_origin.Protocol() != "chrome" &&
         security_origin.Protocol() != "chrome-extension";
}

v8::Local<v8::Promise> NostrBindings::CreateErrorPromise(
    v8::Isolate* isolate,
    const std::string& error) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  v8::Local<v8::Value> error_value = 
      v8::Exception::Error(gin::StringToV8(isolate, error));
  resolver->Reject(context, error_value).Check();
  
  return resolver->GetPromise();
}

}  // namespace tungsten