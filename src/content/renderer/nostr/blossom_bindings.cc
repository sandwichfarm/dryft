// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/nostr/blossom_bindings.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/nostr_messages.h"
#include "content/public/renderer/render_frame.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/function_template.h"
#include "gin/handle.h"
#include "third_party/blink/public/platform/web_blob.h"
#include "third_party/blink/public/platform/web_blob_registry.h"
#include "third_party/blink/public/web/web_array_buffer.h"
#include "third_party/blink/public/web/web_blob.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-promise.h"

namespace dryft {

gin::WrapperInfo BlossomBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

namespace {

// Convert a JavaScript File/Blob to bytes
bool ExtractBlobData(v8::Isolate* isolate,
                     v8::Local<v8::Value> value,
                     std::vector<uint8_t>* out_data,
                     std::string* out_mime_type) {
  // Check if it's a Blob or File (File inherits from Blob)
  if (value->IsObject()) {
    v8::Local<v8::Object> obj = value.As<v8::Object>();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    
    // Check if it has a type property (Blob/File)
    v8::Local<v8::String> type_key = gin::StringToV8(isolate, "type");
    if (obj->Has(context, type_key).FromMaybe(false)) {
      v8::Local<v8::Value> type_value;
      if (obj->Get(context, type_key).ToLocal(&type_value) && type_value->IsString()) {
        *out_mime_type = gin::V8ToString(isolate, type_value);
      }
    }
    
    // For now, we'll need to use FileReader API from JavaScript side
    // This is a limitation we'll need to address with proper Blob handling
    LOG(WARNING) << "Direct Blob extraction not yet implemented";
    return false;
  }
  
  // Check if it's an ArrayBuffer
  if (value->IsArrayBuffer()) {
    v8::Local<v8::ArrayBuffer> buffer = value.As<v8::ArrayBuffer>();
    auto backing_store = buffer->GetBackingStore();
    const uint8_t* data = static_cast<const uint8_t*>(backing_store->Data());
    size_t length = backing_store->ByteLength();
    out_data->assign(data, data + length);
    return true;
  }
  
  return false;
}

}  // namespace

BlossomBindings::BlossomBindings(content::RenderFrame* render_frame)
    : render_frame_(render_frame) {
  DCHECK(render_frame_);
}

BlossomBindings::~BlossomBindings() = default;

// static
void BlossomBindings::Install(v8::Local<v8::Object> global,
                              content::RenderFrame* render_frame) {
  v8::Isolate* isolate = global->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  
  // Create the Blossom bindings instance
  gin::Handle<BlossomBindings> bindings = 
      gin::CreateHandle(isolate, new BlossomBindings(render_frame));
  
  if (bindings.IsEmpty()) {
    LOG(ERROR) << "Failed to create BlossomBindings";
    return;
  }
  
  // Create the blossom object
  v8::Local<v8::Object> blossom_obj = bindings->GetWrapper(isolate).ToLocalChecked();
  
  // Install as window.blossom
  global->Set(context, gin::StringToV8(isolate, "blossom"), blossom_obj).Check();
  
  VLOG(1) << "window.blossom installed successfully";
}

gin::ObjectTemplateBuilder BlossomBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<BlossomBindings>::GetObjectTemplateBuilder(isolate)
      .SetMethod("upload", &BlossomBindings::Upload)
      .SetMethod("uploadMultiple", &BlossomBindings::UploadMultiple)
      .SetMethod("get", &BlossomBindings::Get)
      .SetMethod("getUrl", &BlossomBindings::GetUrl)
      .SetMethod("has", &BlossomBindings::Has)
      .SetMethod("hasMultiple", &BlossomBindings::HasMultiple)
      .SetProperty("servers", &BlossomBindings::GetServers)
      .SetProperty("local", &BlossomBindings::GetLocal)
      .SetMethod("mirror", &BlossomBindings::Mirror)
      .SetMethod("createAuth", &BlossomBindings::CreateAuth);
}

v8::Local<v8::Promise> BlossomBindings::Upload(v8::Isolate* isolate,
                                                v8::Local<v8::Value> file) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  std::vector<uint8_t> data;
  std::string mime_type;
  
  if (!ExtractBlobData(isolate, file, &data, &mime_type)) {
    resolver->Reject(context, 
        gin::StringToV8(isolate, "Invalid file/blob parameter")).Check();
    return resolver->GetPromise();
  }
  
  int request_id = next_request_id_++;
  auto resolver_ptr = std::make_unique<v8::Global<v8::Promise::Resolver>>(
      isolate, resolver);
  pending_promises_[request_id] = std::move(resolver_ptr);
  
  // Send IPC message to browser process
  render_frame_->Send(new NostrHostMsg_BlossomUpload(
      render_frame_->GetRoutingID(),
      request_id,
      render_frame_->GetWebFrame()->GetSecurityOrigin(),
      data,
      mime_type,
      base::Value::Dict()  // metadata
  ));
  
  return resolver->GetPromise();
}

v8::Local<v8::Promise> BlossomBindings::UploadMultiple(v8::Isolate* isolate,
                                                        v8::Local<v8::Array> files) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // For multiple uploads, we'll need to handle them sequentially or in parallel
  // For now, reject with not implemented
  resolver->Reject(context, 
      gin::StringToV8(isolate, "uploadMultiple not yet implemented")).Check();
  
  return resolver->GetPromise();
}

v8::Local<v8::Promise> BlossomBindings::Get(v8::Isolate* isolate,
                                             const std::string& hash) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  int request_id = next_request_id_++;
  auto resolver_ptr = std::make_unique<v8::Global<v8::Promise::Resolver>>(
      isolate, resolver);
  pending_promises_[request_id] = std::move(resolver_ptr);
  
  // Send IPC message to browser process
  render_frame_->Send(new NostrHostMsg_BlossomGet(
      render_frame_->GetRoutingID(),
      request_id,
      hash,
      render_frame_->GetWebFrame()->GetSecurityOrigin()
  ));
  
  return resolver->GetPromise();
}

std::string BlossomBindings::GetUrl(const std::string& hash) {
  // Return a URL that will be served by the local Blossom server
  // This assumes the local server is running on port 8080
  return "http://localhost:8080/" + hash;
}

v8::Local<v8::Promise> BlossomBindings::Has(v8::Isolate* isolate,
                                             const std::string& hash) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  int request_id = next_request_id_++;
  auto resolver_ptr = std::make_unique<v8::Global<v8::Promise::Resolver>>(
      isolate, resolver);
  pending_promises_[request_id] = std::move(resolver_ptr);
  
  // Send IPC message to browser process
  render_frame_->Send(new NostrHostMsg_BlossomHas(
      render_frame_->GetRoutingID(),
      request_id,
      hash,
      render_frame_->GetWebFrame()->GetSecurityOrigin()
  ));
  
  return resolver->GetPromise();
}

v8::Local<v8::Promise> BlossomBindings::HasMultiple(v8::Isolate* isolate,
                                                     v8::Local<v8::Array> hashes) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // For multiple checks, we'll need to handle them
  // For now, reject with not implemented
  resolver->Reject(context, 
      gin::StringToV8(isolate, "hasMultiple not yet implemented")).Check();
  
  return resolver->GetPromise();
}

v8::Local<v8::Object> BlossomBindings::GetServers(v8::Isolate* isolate) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> servers = v8::Object::New(isolate);
  
  // Create methods for server management
  servers->Set(context,
               gin::StringToV8(isolate, "list"),
               gin::CreateFunctionTemplate(isolate,
                   base::BindRepeating(&BlossomBindings::ServersList,
                                       weak_factory_.GetWeakPtr()))
                   ->GetFunction(context).ToLocalChecked()).Check();
  
  servers->Set(context,
               gin::StringToV8(isolate, "add"),
               gin::CreateFunctionTemplate(isolate,
                   base::BindRepeating(&BlossomBindings::ServersAdd,
                                       weak_factory_.GetWeakPtr()))
                   ->GetFunction(context).ToLocalChecked()).Check();
  
  servers->Set(context,
               gin::StringToV8(isolate, "remove"),
               gin::CreateFunctionTemplate(isolate,
                   base::BindRepeating(&BlossomBindings::ServersRemove,
                                       weak_factory_.GetWeakPtr()))
                   ->GetFunction(context).ToLocalChecked()).Check();
  
  servers->Set(context,
               gin::StringToV8(isolate, "test"),
               gin::CreateFunctionTemplate(isolate,
                   base::BindRepeating(&BlossomBindings::ServersTest,
                                       weak_factory_.GetWeakPtr()))
                   ->GetFunction(context).ToLocalChecked()).Check();
  
  return servers;
}

v8::Local<v8::Object> BlossomBindings::GetLocal(v8::Isolate* isolate) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> local = v8::Object::New(isolate);
  
  // Add property getters
  local->SetAccessor(context,
                     gin::StringToV8(isolate, "url"),
                     &BlossomBindings::LocalUrl).Check();
  
  local->SetAccessor(context,
                     gin::StringToV8(isolate, "enabled"),
                     &BlossomBindings::LocalEnabled).Check();
  
  local->SetAccessor(context,
                     gin::StringToV8(isolate, "storageUsed"),
                     &BlossomBindings::LocalStorageUsed).Check();
  
  local->SetAccessor(context,
                     gin::StringToV8(isolate, "fileCount"),
                     &BlossomBindings::LocalFileCount).Check();
  
  // Add methods
  local->Set(context,
             gin::StringToV8(isolate, "clear"),
             gin::CreateFunctionTemplate(isolate,
                 base::BindRepeating(&BlossomBindings::LocalClear,
                                     weak_factory_.GetWeakPtr()))
                 ->GetFunction(context).ToLocalChecked()).Check();
  
  local->Set(context,
             gin::StringToV8(isolate, "prune"),
             gin::CreateFunctionTemplate(isolate,
                 base::BindRepeating(&BlossomBindings::LocalPrune,
                                     weak_factory_.GetWeakPtr()))
                 ->GetFunction(context).ToLocalChecked()).Check();
  
  local->Set(context,
             gin::StringToV8(isolate, "setQuota"),
             gin::CreateFunctionTemplate(isolate,
                 base::BindRepeating(&BlossomBindings::LocalSetQuota,
                                     weak_factory_.GetWeakPtr()))
                 ->GetFunction(context).ToLocalChecked()).Check();
  
  return local;
}

v8::Local<v8::Promise> BlossomBindings::Mirror(v8::Isolate* isolate,
                                                const std::string& hash,
                                                v8::Local<v8::Value> servers) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  std::vector<std::string> server_list;
  
  // Extract server list if provided
  if (servers->IsArray()) {
    v8::Local<v8::Array> arr = servers.As<v8::Array>();
    for (uint32_t i = 0; i < arr->Length(); i++) {
      v8::Local<v8::Value> val;
      if (arr->Get(context, i).ToLocal(&val) && val->IsString()) {
        server_list.push_back(gin::V8ToString(isolate, val));
      }
    }
  }
  
  int request_id = next_request_id_++;
  auto resolver_ptr = std::make_unique<v8::Global<v8::Promise::Resolver>>(
      isolate, resolver);
  pending_promises_[request_id] = std::move(resolver_ptr);
  
  // Send IPC message to browser process
  render_frame_->Send(new NostrHostMsg_BlossomMirror(
      render_frame_->GetRoutingID(),
      request_id,
      hash,
      server_list,
      render_frame_->GetWebFrame()->GetSecurityOrigin()
  ));
  
  return resolver->GetPromise();
}

v8::Local<v8::Promise> BlossomBindings::CreateAuth(v8::Isolate* isolate,
                                                    v8::Local<v8::Object> params) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // Extract parameters
  std::string verb = "upload";  // default
  std::vector<std::string> files;
  int64_t expiration = 0;
  
  v8::Local<v8::Value> verb_val;
  if (params->Get(context, gin::StringToV8(isolate, "verb")).ToLocal(&verb_val) &&
      verb_val->IsString()) {
    verb = gin::V8ToString(isolate, verb_val);
  }
  
  v8::Local<v8::Value> files_val;
  if (params->Get(context, gin::StringToV8(isolate, "files")).ToLocal(&files_val) &&
      files_val->IsArray()) {
    v8::Local<v8::Array> arr = files_val.As<v8::Array>();
    for (uint32_t i = 0; i < arr->Length(); i++) {
      v8::Local<v8::Value> val;
      if (arr->Get(context, i).ToLocal(&val) && val->IsString()) {
        files.push_back(gin::V8ToString(isolate, val));
      }
    }
  }
  
  v8::Local<v8::Value> exp_val;
  if (params->Get(context, gin::StringToV8(isolate, "expiration")).ToLocal(&exp_val) &&
      exp_val->IsNumber()) {
    expiration = exp_val->IntegerValue(context).FromMaybe(0);
  }
  
  int request_id = next_request_id_++;
  auto resolver_ptr = std::make_unique<v8::Global<v8::Promise::Resolver>>(
      isolate, resolver);
  pending_promises_[request_id] = std::move(resolver_ptr);
  
  // Send IPC message to browser process
  render_frame_->Send(new NostrHostMsg_BlossomCreateAuth(
      render_frame_->GetRoutingID(),
      request_id,
      verb,
      files,
      expiration
  ));
  
  return resolver->GetPromise();
}

// Server management methods
v8::Local<v8::Promise> BlossomBindings::ServersList(v8::Isolate* isolate) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  int request_id = next_request_id_++;
  auto resolver_ptr = std::make_unique<v8::Global<v8::Promise::Resolver>>(
      isolate, resolver);
  pending_promises_[request_id] = std::move(resolver_ptr);
  
  // Send IPC message to browser process
  render_frame_->Send(new NostrHostMsg_BlossomListServers(
      render_frame_->GetRoutingID(),
      request_id,
      render_frame_->GetWebFrame()->GetSecurityOrigin()
  ));
  
  return resolver->GetPromise();
}

v8::Local<v8::Promise> BlossomBindings::ServersAdd(v8::Isolate* isolate,
                                                    const std::string& url) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // For now, resolve immediately
  // TODO: Implement actual server addition via IPC
  resolver->Resolve(context, v8::Undefined(isolate)).Check();
  
  return resolver->GetPromise();
}

v8::Local<v8::Promise> BlossomBindings::ServersRemove(v8::Isolate* isolate,
                                                       const std::string& url) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // For now, resolve immediately
  // TODO: Implement actual server removal via IPC
  resolver->Resolve(context, v8::Undefined(isolate)).Check();
  
  return resolver->GetPromise();
}

v8::Local<v8::Promise> BlossomBindings::ServersTest(v8::Isolate* isolate,
                                                     const std::string& url) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // For now, return a mock status
  // TODO: Implement actual server testing via IPC
  gin::Dictionary status(isolate);
  status.Set("online", true);
  status.Set("latency", 42);
  status.Set("version", "1.0.0");
  
  resolver->Resolve(context, status.GetHandle()).Check();
  
  return resolver->GetPromise();
}

// Local server property getters
void BlossomBindings::LocalUrl(v8::Local<v8::String> property,
                               const v8::PropertyCallbackInfo<v8::Value>& info) {
  // TODO: Get actual local server URL from browser process
  info.GetReturnValue().Set(gin::StringToV8(info.GetIsolate(), "http://localhost:8080"));
}

void BlossomBindings::LocalEnabled(v8::Local<v8::String> property,
                                   const v8::PropertyCallbackInfo<v8::Value>& info) {
  // TODO: Get actual enabled state from browser process
  info.GetReturnValue().Set(v8::Boolean::New(info.GetIsolate(), true));
}

void BlossomBindings::LocalStorageUsed(v8::Local<v8::String> property,
                                       const v8::PropertyCallbackInfo<v8::Value>& info) {
  // TODO: Get actual storage usage from browser process
  info.GetReturnValue().Set(v8::Number::New(info.GetIsolate(), 0));
}

void BlossomBindings::LocalFileCount(v8::Local<v8::String> property,
                                     const v8::PropertyCallbackInfo<v8::Value>& info) {
  // TODO: Get actual file count from browser process
  info.GetReturnValue().Set(v8::Number::New(info.GetIsolate(), 0));
}

// Local server methods
v8::Local<v8::Promise> BlossomBindings::LocalClear(v8::Isolate* isolate) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // TODO: Implement via IPC
  resolver->Resolve(context, v8::Undefined(isolate)).Check();
  
  return resolver->GetPromise();
}

v8::Local<v8::Promise> BlossomBindings::LocalPrune(v8::Isolate* isolate,
                                                    v8::Local<v8::Value> older_than) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // TODO: Implement via IPC
  resolver->Resolve(context, v8::Number::New(isolate, 0)).Check();
  
  return resolver->GetPromise();
}

v8::Local<v8::Promise> BlossomBindings::LocalSetQuota(v8::Isolate* isolate,
                                                       double bytes) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver = 
      v8::Promise::Resolver::New(context).ToLocalChecked();
  
  // TODO: Implement via IPC
  resolver->Resolve(context, v8::Undefined(isolate)).Check();
  
  return resolver->GetPromise();
}

// IPC response handlers
void BlossomBindings::OnUploadResponse(
    std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
    bool success,
    const base::Value::Dict& result) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Promise::Resolver> local_resolver = 
      resolver->Get(isolate);
  
  if (success) {
    gin::Dictionary dict(isolate);
    
    // Extract result fields
    const std::string* hash = result.FindString("hash");
    const std::string* url = result.FindString("url");
    const absl::optional<double> size = result.FindDouble("size");
    const std::string* mime_type = result.FindString("mime_type");
    const base::Value::List* servers = result.FindList("servers");
    
    if (hash) dict.Set("hash", *hash);
    if (url) dict.Set("url", *url);
    if (size) dict.Set("size", static_cast<int64_t>(*size));
    if (mime_type) dict.Set("type", *mime_type);
    
    if (servers) {
      v8::Local<v8::Array> server_array = v8::Array::New(isolate, servers->size());
      for (size_t i = 0; i < servers->size(); i++) {
        if (servers->operator[](i).is_string()) {
          server_array->Set(context, i, 
              gin::StringToV8(isolate, servers->operator[](i).GetString())).Check();
        }
      }
      dict.Set("servers", server_array);
    }
    
    local_resolver->Resolve(context, dict.GetHandle()).Check();
  } else {
    const std::string* error = result.FindString("error");
    local_resolver->Reject(context, 
        gin::StringToV8(isolate, error ? *error : "Upload failed")).Check();
  }
}

void BlossomBindings::OnGetResponse(
    std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
    bool success,
    const std::vector<uint8_t>& data) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Promise::Resolver> local_resolver = 
      resolver->Get(isolate);
  
  if (success) {
    // Create a Blob from the data
    // For now, we'll return an ArrayBuffer
    v8::Local<v8::ArrayBuffer> buffer = 
        v8::ArrayBuffer::New(isolate, data.size());
    auto backing_store = buffer->GetBackingStore();
    memcpy(backing_store->Data(), data.data(), data.size());
    
    local_resolver->Resolve(context, buffer).Check();
  } else {
    local_resolver->Reject(context, 
        gin::StringToV8(isolate, "Failed to get content")).Check();
  }
}

void BlossomBindings::OnHasResponse(
    std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
    bool success,
    bool exists) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Promise::Resolver> local_resolver = 
      resolver->Get(isolate);
  
  if (success) {
    local_resolver->Resolve(context, v8::Boolean::New(isolate, exists)).Check();
  } else {
    local_resolver->Reject(context, 
        gin::StringToV8(isolate, "Failed to check existence")).Check();
  }
}

void BlossomBindings::OnListServersResponse(
    std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
    bool success,
    const base::Value::List& servers) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Promise::Resolver> local_resolver = 
      resolver->Get(isolate);
  
  if (success) {
    v8::Local<v8::Array> array = v8::Array::New(isolate, servers.size());
    for (size_t i = 0; i < servers.size(); i++) {
      if (servers[i].is_dict()) {
        gin::Dictionary server_dict(isolate);
        const base::Value::Dict& server = servers[i].GetDict();
        
        const std::string* url = server.FindString("url");
        const absl::optional<bool> enabled = server.FindBool("enabled");
        
        if (url) server_dict.Set("url", *url);
        if (enabled) server_dict.Set("enabled", *enabled);
        
        array->Set(context, i, server_dict.GetHandle()).Check();
      }
    }
    local_resolver->Resolve(context, array).Check();
  } else {
    local_resolver->Reject(context, 
        gin::StringToV8(isolate, "Failed to list servers")).Check();
  }
}

void BlossomBindings::OnMirrorResponse(
    std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
    bool success,
    const base::Value::Dict& results) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Promise::Resolver> local_resolver = 
      resolver->Get(isolate);
  
  if (success) {
    gin::Dictionary dict(isolate);
    
    // Convert results to JavaScript object
    for (const auto& [server, result] : results) {
      if (result.is_dict()) {
        gin::Dictionary server_result(isolate);
        const base::Value::Dict& res = result.GetDict();
        
        const absl::optional<bool> success = res.FindBool("success");
        const std::string* error = res.FindString("error");
        
        if (success) server_result.Set("success", *success);
        if (error) server_result.Set("error", *error);
        
        dict.Set(server, server_result.GetHandle());
      }
    }
    
    local_resolver->Resolve(context, dict.GetHandle()).Check();
  } else {
    local_resolver->Reject(context, 
        gin::StringToV8(isolate, "Mirror operation failed")).Check();
  }
}

void BlossomBindings::OnAuthResponse(
    std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
    bool success,
    const base::Value::Dict& event) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Promise::Resolver> local_resolver = 
      resolver->Get(isolate);
  
  if (success) {
    gin::Dictionary dict(isolate);
    
    // Convert Nostr event to JavaScript object
    const std::string* id = event.FindString("id");
    const std::string* pubkey = event.FindString("pubkey");
    const absl::optional<double> created_at = event.FindDouble("created_at");
    const absl::optional<int> kind = event.FindInt("kind");
    const std::string* content = event.FindString("content");
    const std::string* sig = event.FindString("sig");
    const base::Value::List* tags = event.FindList("tags");
    
    if (id) dict.Set("id", *id);
    if (pubkey) dict.Set("pubkey", *pubkey);
    if (created_at) dict.Set("created_at", static_cast<int64_t>(*created_at));
    if (kind) dict.Set("kind", *kind);
    if (content) dict.Set("content", *content);
    if (sig) dict.Set("sig", *sig);
    
    if (tags) {
      v8::Local<v8::Array> tags_array = v8::Array::New(isolate, tags->size());
      for (size_t i = 0; i < tags->size(); i++) {
        if (tags->operator[](i).is_list()) {
          const base::Value::List& tag = tags->operator[](i).GetList();
          v8::Local<v8::Array> tag_array = v8::Array::New(isolate, tag.size());
          for (size_t j = 0; j < tag.size(); j++) {
            if (tag[j].is_string()) {
              tag_array->Set(context, j, 
                  gin::StringToV8(isolate, tag[j].GetString())).Check();
            }
          }
          tags_array->Set(context, i, tag_array).Check();
        }
      }
      dict.Set("tags", tags_array);
    }
    
    local_resolver->Resolve(context, dict.GetHandle()).Check();
  } else {
    local_resolver->Reject(context, 
        gin::StringToV8(isolate, "Failed to create auth event")).Check();
  }
}

}  // namespace dryft