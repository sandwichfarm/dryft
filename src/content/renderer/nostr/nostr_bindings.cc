// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/nostr/nostr_bindings.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/common/nostr_messages.h"
#include "ipc/ipc_message_macros.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/nostr/nostr_relay_bindings.h"
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
    : content::RenderFrameObserver(render_frame),
      render_frame_(render_frame), 
      next_request_id_(1) {
  DCHECK(render_frame_);
}

NostrBindings::~NostrBindings() = default;

void NostrBindings::OnDestruct() {
  // Clear any pending resolvers
  pending_resolvers_.clear();
  render_frame_ = nullptr;
}

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
  
  // The NostrBindings instance is now owned by V8 and will receive IPC messages
  // as a RenderFrameObserver until the frame is destroyed
  
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
      .SetLazyDataProperty("nip04", &NostrBindings::GetNip04Object)
      .SetLazyDataProperty("relay", &NostrBindings::GetRelayObject)
      // Account management methods (non-standard but useful)
      .SetMethod("listAccounts", &NostrBindings::ListAccounts)
      .SetMethod("getCurrentAccount", &NostrBindings::GetCurrentAccount)
      .SetMethod("switchAccount", &NostrBindings::SwitchAccount);
}

v8::Local<v8::Promise> NostrBindings::GetPublicKey(v8::Isolate* isolate) {
  VLOG(2) << "window.nostr.getPublicKey() called";
  
  if (!IsOriginAllowed()) {
    return CreateErrorPromise(isolate, "Origin not allowed");
  }
  
  // Create promise for async operation
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // Generate request ID and store resolver
  int request_id = GetNextRequestId();
  pending_resolvers_[request_id] = v8::Global<v8::Promise::Resolver>(isolate, resolver);
  
  // Send IPC message
  SendGetPublicKey(request_id);
  
  return resolver->GetPromise();
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
  
  // Convert V8 object to base::Value::Dict
  base::Value::Dict event_dict;
  
  // Convert required fields
  event_dict.Set("kind", kind_value.As<v8::Number>()->Value());
  event_dict.Set("content", gin::V8ToString(isolate, content_value));
  
  // Convert optional fields if present
  v8::Local<v8::Value> tags_value;
  if (event->Get(context, gin::StringToV8(isolate, "tags")).ToLocal(&tags_value) && tags_value->IsArray()) {
    // TODO: Convert tags array properly
    event_dict.Set("tags", base::Value::List());
  } else {
    event_dict.Set("tags", base::Value::List());
  }
  
  v8::Local<v8::Value> created_at_value;
  if (event->Get(context, gin::StringToV8(isolate, "created_at")).ToLocal(&created_at_value) && created_at_value->IsNumber()) {
    event_dict.Set("created_at", created_at_value.As<v8::Number>()->Value());
  }
  
  // Create promise for async operation
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // Generate request ID and store resolver
  int request_id = GetNextRequestId();
  pending_resolvers_[request_id] = v8::Global<v8::Promise::Resolver>(isolate, resolver);
  
  // Send IPC message
  SendSignEvent(request_id, event_dict);
  
  return resolver->GetPromise();
}

v8::Local<v8::Promise> NostrBindings::GetRelays(v8::Isolate* isolate) {
  VLOG(2) << "window.nostr.getRelays() called";
  
  if (!IsOriginAllowed()) {
    return CreateErrorPromise(isolate, "Origin not allowed");
  }
  
  // Create promise for async operation
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // Generate request ID and store resolver
  int request_id = GetNextRequestId();
  pending_resolvers_[request_id] = v8::Global<v8::Promise::Resolver>(isolate, resolver);
  
  // Send IPC message
  SendGetRelays(request_id);
  
  return resolver->GetPromise();
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
  
  // Create promise for async operation
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // Generate request ID and store resolver
  int request_id = GetNextRequestId();
  pending_resolvers_[request_id] = v8::Global<v8::Promise::Resolver>(isolate, resolver);
  
  // Send IPC message
  SendNip04Encrypt(request_id, pubkey, plaintext);
  
  return resolver->GetPromise();
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
  
  // Create promise for async operation
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // Generate request ID and store resolver
  int request_id = GetNextRequestId();
  pending_resolvers_[request_id] = v8::Global<v8::Promise::Resolver>(isolate, resolver);
  
  // Send IPC message
  SendNip04Decrypt(request_id, pubkey, ciphertext);
  
  return resolver->GetPromise();
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

// Account Management Methods

v8::Local<v8::Promise> NostrBindings::ListAccounts(v8::Isolate* isolate) {
  VLOG(2) << "window.nostr.listAccounts() called";
  
  if (!IsOriginAllowed()) {
    return CreateErrorPromise(isolate, "Origin not allowed");
  }
  
  // Create promise for async operation
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // Generate request ID and store resolver
  int request_id = GetNextRequestId();
  pending_resolvers_[request_id] = v8::Global<v8::Promise::Resolver>(isolate, resolver);
  
  // Send IPC message
  SendListAccounts(request_id);
  
  return resolver->GetPromise();
}

v8::Local<v8::Promise> NostrBindings::GetCurrentAccount(v8::Isolate* isolate) {
  VLOG(2) << "window.nostr.getCurrentAccount() called";
  
  if (!IsOriginAllowed()) {
    return CreateErrorPromise(isolate, "Origin not allowed");
  }
  
  // Create promise for async operation
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // Generate request ID and store resolver
  int request_id = GetNextRequestId();
  pending_resolvers_[request_id] = v8::Global<v8::Promise::Resolver>(isolate, resolver);
  
  // Send IPC message
  SendGetCurrentAccount(request_id);
  
  return resolver->GetPromise();
}

v8::Local<v8::Promise> NostrBindings::SwitchAccount(v8::Isolate* isolate,
                                                   const std::string& pubkey) {
  VLOG(2) << "window.nostr.switchAccount() called";
  
  if (!IsOriginAllowed()) {
    return CreateErrorPromise(isolate, "Origin not allowed");
  }
  
  // Validate pubkey format
  if (pubkey.length() != 64) {
    return CreateErrorPromise(isolate, "Invalid pubkey format");
  }
  
  // Create promise for async operation
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // Generate request ID and store resolver
  int request_id = GetNextRequestId();
  pending_resolvers_[request_id] = v8::Global<v8::Promise::Resolver>(isolate, resolver);
  
  // Send IPC message
  SendSwitchAccount(request_id, pubkey);
  
  return resolver->GetPromise();
}

v8::Local<v8::Object> NostrBindings::GetRelayObject(v8::Isolate* isolate) {
  // Create the relay bindings object
  v8::Local<v8::Value> relay_value = NostrRelayBindings::Create(isolate, render_frame_);
  if (relay_value.IsEmpty()) {
    LOG(ERROR) << "Failed to create window.nostr.relay object";
    return v8::Object::New(isolate);
  }
  
  return relay_value.As<v8::Object>();
}

// IPC Message Sending Methods

void NostrBindings::SendGetPublicKey(int request_id) {
  if (!render_frame_) return;
  
  url::Origin origin = GetCurrentOrigin();
  render_frame_->Send(new NostrHostMsg_GetPublicKey(
      render_frame_->GetRoutingID(), request_id, origin));
}

void NostrBindings::SendSignEvent(int request_id, const base::Value::Dict& event) {
  if (!render_frame_) return;
  
  url::Origin origin = GetCurrentOrigin();
  NostrRateLimitInfo rate_limit_info;  // Empty for now
  
  render_frame_->Send(new NostrHostMsg_SignEvent(
      render_frame_->GetRoutingID(), request_id, origin, event, rate_limit_info));
}

void NostrBindings::SendGetRelays(int request_id) {
  if (!render_frame_) return;
  
  url::Origin origin = GetCurrentOrigin();
  render_frame_->Send(new NostrHostMsg_GetRelays(
      render_frame_->GetRoutingID(), request_id, origin));
}

void NostrBindings::SendNip04Encrypt(int request_id, const std::string& pubkey, 
                                    const std::string& plaintext) {
  if (!render_frame_) return;
  
  url::Origin origin = GetCurrentOrigin();
  render_frame_->Send(new NostrHostMsg_Nip04Encrypt(
      render_frame_->GetRoutingID(), request_id, origin, pubkey, plaintext));
}

void NostrBindings::SendNip04Decrypt(int request_id, const std::string& pubkey, 
                                    const std::string& ciphertext) {
  if (!render_frame_) return;
  
  url::Origin origin = GetCurrentOrigin();
  render_frame_->Send(new NostrHostMsg_Nip04Decrypt(
      render_frame_->GetRoutingID(), request_id, origin, pubkey, ciphertext));
}

void NostrBindings::SendListAccounts(int request_id) {
  if (!render_frame_) return;
  
  url::Origin origin = GetCurrentOrigin();
  render_frame_->Send(new NostrHostMsg_ListAccounts(
      render_frame_->GetRoutingID(), request_id, origin));
}

void NostrBindings::SendGetCurrentAccount(int request_id) {
  if (!render_frame_) return;
  
  url::Origin origin = GetCurrentOrigin();
  render_frame_->Send(new NostrHostMsg_GetCurrentAccount(
      render_frame_->GetRoutingID(), request_id, origin));
}

void NostrBindings::SendSwitchAccount(int request_id, const std::string& pubkey) {
  if (!render_frame_) return;
  
  url::Origin origin = GetCurrentOrigin();
  render_frame_->Send(new NostrHostMsg_SwitchAccount(
      render_frame_->GetRoutingID(), request_id, pubkey, origin));
}

// IPC Response Handlers

void NostrBindings::OnGetPublicKeyResponse(int request_id, bool success, 
                                          const std::string& result) {
  auto it = pending_resolvers_.find(request_id);
  if (it == pending_resolvers_.end()) {
    return;  // Request not found or already resolved
  }
  
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  
  v8::Local<v8::Promise::Resolver> resolver = it->second.Get(isolate);
  
  if (success) {
    v8::Local<v8::Value> result_value = gin::StringToV8(isolate, result);
    resolver->Resolve(context, result_value).Check();
  } else {
    v8::Local<v8::Value> error_value = 
        v8::Exception::Error(gin::StringToV8(isolate, result));
    resolver->Reject(context, error_value).Check();
  }
  
  pending_resolvers_.erase(it);
}

void NostrBindings::OnSignEventResponse(int request_id, bool success, 
                                       const base::Value::Dict& result) {
  auto it = pending_resolvers_.find(request_id);
  if (it == pending_resolvers_.end()) {
    return;
  }
  
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  
  v8::Local<v8::Promise::Resolver> resolver = it->second.Get(isolate);
  
  if (success) {
    // Convert base::Value::Dict to V8 object
    v8::Local<v8::Object> result_obj = v8::Object::New(isolate);
    
    for (const auto& [key, value] : result) {
      v8::Local<v8::String> v8_key = gin::StringToV8(isolate, key);
      v8::Local<v8::Value> v8_value;
      
      if (value.is_string()) {
        v8_value = gin::StringToV8(isolate, value.GetString());
      } else if (value.is_int()) {
        v8_value = v8::Number::New(isolate, value.GetInt());
      } else if (value.is_list()) {
        // TODO: Convert list properly
        v8_value = v8::Array::New(isolate);
      } else {
        v8_value = v8::Null(isolate);
      }
      
      result_obj->Set(context, v8_key, v8_value).Check();
    }
    
    resolver->Resolve(context, result_obj).Check();
  } else {
    std::string error_msg = "Signing failed";
    auto* error_str = result.FindString("error");
    if (error_str) {
      error_msg = *error_str;
    }
    
    v8::Local<v8::Value> error_value = 
        v8::Exception::Error(gin::StringToV8(isolate, error_msg));
    resolver->Reject(context, error_value).Check();
  }
  
  pending_resolvers_.erase(it);
}

void NostrBindings::OnGetRelaysResponse(int request_id, bool success, 
                                       const NostrRelayPolicy& result) {
  auto it = pending_resolvers_.find(request_id);
  if (it == pending_resolvers_.end()) {
    return;
  }
  
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  
  v8::Local<v8::Promise::Resolver> resolver = it->second.Get(isolate);
  
  if (success) {
    // Convert NostrRelayPolicy to V8 object
    v8::Local<v8::Object> result_obj = v8::Object::New(isolate);
    
    for (const auto& [url, config] : result.relays) {
      v8::Local<v8::String> v8_url = gin::StringToV8(isolate, url);
      v8::Local<v8::Object> config_obj = v8::Object::New(isolate);
      
      for (const auto& [key, value] : config) {
        v8::Local<v8::String> v8_key = gin::StringToV8(isolate, key);
        v8::Local<v8::Value> v8_value;
        
        if (value.is_bool()) {
          v8_value = v8::Boolean::New(isolate, value.GetBool());
        } else if (value.is_string()) {
          v8_value = gin::StringToV8(isolate, value.GetString());
        } else {
          v8_value = v8::Null(isolate);
        }
        
        config_obj->Set(context, v8_key, v8_value).Check();
      }
      
      result_obj->Set(context, v8_url, config_obj).Check();
    }
    
    resolver->Resolve(context, result_obj).Check();
  } else {
    v8::Local<v8::Value> error_value = 
        v8::Exception::Error(gin::StringToV8(isolate, "Failed to get relays"));
    resolver->Reject(context, error_value).Check();
  }
  
  pending_resolvers_.erase(it);
}

void NostrBindings::OnNip04EncryptResponse(int request_id, bool success, 
                                          const std::string& result) {
  auto it = pending_resolvers_.find(request_id);
  if (it == pending_resolvers_.end()) {
    return;
  }
  
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  
  v8::Local<v8::Promise::Resolver> resolver = it->second.Get(isolate);
  
  if (success) {
    v8::Local<v8::Value> result_value = gin::StringToV8(isolate, result);
    resolver->Resolve(context, result_value).Check();
  } else {
    v8::Local<v8::Value> error_value = 
        v8::Exception::Error(gin::StringToV8(isolate, result));
    resolver->Reject(context, error_value).Check();
  }
  
  pending_resolvers_.erase(it);
}

void NostrBindings::OnNip04DecryptResponse(int request_id, bool success, 
                                          const std::string& result) {
  auto it = pending_resolvers_.find(request_id);
  if (it == pending_resolvers_.end()) {
    return;
  }
  
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  
  v8::Local<v8::Promise::Resolver> resolver = it->second.Get(isolate);
  
  if (success) {
    v8::Local<v8::Value> result_value = gin::StringToV8(isolate, result);
    resolver->Resolve(context, result_value).Check();
  } else {
    v8::Local<v8::Value> error_value = 
        v8::Exception::Error(gin::StringToV8(isolate, result));
    resolver->Reject(context, error_value).Check();
  }
  
  pending_resolvers_.erase(it);
}

void NostrBindings::OnListAccountsResponse(int request_id, bool success,
                                         const base::Value::List& result) {
  auto it = pending_resolvers_.find(request_id);
  if (it == pending_resolvers_.end()) {
    return;
  }
  
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  
  v8::Local<v8::Promise::Resolver> resolver = it->second.Get(isolate);
  
  if (success) {
    // Convert base::Value::List to V8 array
    v8::Local<v8::Array> result_array = v8::Array::New(isolate, result.size());
    
    for (size_t i = 0; i < result.size(); ++i) {
      const auto& account = result[i];
      v8::Local<v8::Object> account_obj = v8::Object::New(isolate);
      
      if (account.is_dict()) {
        const auto& account_dict = account.GetDict();
        
        for (const auto& [key, value] : account_dict) {
          v8::Local<v8::String> v8_key = gin::StringToV8(isolate, key);
          v8::Local<v8::Value> v8_value;
          
          if (value.is_string()) {
            v8_value = gin::StringToV8(isolate, value.GetString());
          } else if (value.is_bool()) {
            v8_value = v8::Boolean::New(isolate, value.GetBool());
          } else if (value.is_double()) {
            v8_value = v8::Number::New(isolate, value.GetDouble());
          } else if (value.is_list()) {
            // Convert relay list
            const auto& relays = value.GetList();
            v8::Local<v8::Array> relays_array = v8::Array::New(isolate, relays.size());
            for (size_t j = 0; j < relays.size(); ++j) {
              if (relays[j].is_string()) {
                relays_array->Set(context, j, 
                                gin::StringToV8(isolate, relays[j].GetString())).Check();
              }
            }
            v8_value = relays_array;
          } else {
            v8_value = v8::Null(isolate);
          }
          
          account_obj->Set(context, v8_key, v8_value).Check();
        }
      }
      
      result_array->Set(context, i, account_obj).Check();
    }
    
    resolver->Resolve(context, result_array).Check();
  } else {
    v8::Local<v8::Value> error_value = 
        v8::Exception::Error(gin::StringToV8(isolate, "Failed to list accounts"));
    resolver->Reject(context, error_value).Check();
  }
  
  pending_resolvers_.erase(it);
}

void NostrBindings::OnGetCurrentAccountResponse(int request_id, bool success,
                                               const base::Value::Dict& result) {
  auto it = pending_resolvers_.find(request_id);
  if (it == pending_resolvers_.end()) {
    return;
  }
  
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  
  v8::Local<v8::Promise::Resolver> resolver = it->second.Get(isolate);
  
  if (success) {
    // Convert base::Value::Dict to V8 object
    v8::Local<v8::Object> result_obj = v8::Object::New(isolate);
    
    for (const auto& [key, value] : result) {
      v8::Local<v8::String> v8_key = gin::StringToV8(isolate, key);
      v8::Local<v8::Value> v8_value;
      
      if (value.is_string()) {
        v8_value = gin::StringToV8(isolate, value.GetString());
      } else if (value.is_bool()) {
        v8_value = v8::Boolean::New(isolate, value.GetBool());
      } else if (value.is_double()) {
        v8_value = v8::Number::New(isolate, value.GetDouble());
      } else if (value.is_list()) {
        // Convert relay list
        const auto& relays = value.GetList();
        v8::Local<v8::Array> relays_array = v8::Array::New(isolate, relays.size());
        for (size_t i = 0; i < relays.size(); ++i) {
          if (relays[i].is_string()) {
            relays_array->Set(context, i, 
                            gin::StringToV8(isolate, relays[i].GetString())).Check();
          }
        }
        v8_value = relays_array;
      } else {
        v8_value = v8::Null(isolate);
      }
      
      result_obj->Set(context, v8_key, v8_value).Check();
    }
    
    resolver->Resolve(context, result_obj).Check();
  } else {
    v8::Local<v8::Value> error_value = 
        v8::Exception::Error(gin::StringToV8(isolate, "Failed to get current account"));
    resolver->Reject(context, error_value).Check();
  }
  
  pending_resolvers_.erase(it);
}

void NostrBindings::OnSwitchAccountResponse(int request_id, bool success) {
  auto it = pending_resolvers_.find(request_id);
  if (it == pending_resolvers_.end()) {
    return;
  }
  
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  
  v8::Local<v8::Promise::Resolver> resolver = it->second.Get(isolate);
  
  if (success) {
    resolver->Resolve(context, v8::Boolean::New(isolate, true)).Check();
  } else {
    v8::Local<v8::Value> error_value = 
        v8::Exception::Error(gin::StringToV8(isolate, "Failed to switch account"));
    resolver->Reject(context, error_value).Check();
  }
  
  pending_resolvers_.erase(it);
}

// RenderFrameObserver Implementation

bool NostrBindings::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NostrBindings, message)
    IPC_MESSAGE_HANDLER(NostrMsg_GetPublicKeyResponse, OnGetPublicKeyResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_SignEventResponse, OnSignEventResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_GetRelaysResponse, OnGetRelaysResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_Nip04EncryptResponse, OnNip04EncryptResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_Nip04DecryptResponse, OnNip04DecryptResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_ListAccountsResponse, OnListAccountsResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_GetCurrentAccountResponse, OnGetCurrentAccountResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_SwitchAccountResponse, OnSwitchAccountResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  
  return handled;
}

// Helper Methods

url::Origin NostrBindings::GetCurrentOrigin() const {
  if (!render_frame_) {
    return url::Origin();
  }
  
  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();
  if (!web_frame) {
    return url::Origin();
  }
  
  blink::WebSecurityOrigin security_origin = 
      web_frame->GetDocument().GetSecurityOrigin();
  
  return url::Origin::Create(GURL(security_origin.ToString()));
}

int NostrBindings::GetNextRequestId() {
  return next_request_id_++;
}

}  // namespace tungsten