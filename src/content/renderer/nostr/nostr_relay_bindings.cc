// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/nostr/nostr_relay_bindings.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/common/nostr_messages.h"
#include "content/public/renderer/render_frame.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/object_template_builder.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-json.h"
#include "v8/include/v8-promise.h"

namespace tungsten {

namespace {

const char kRelayNotAvailableError[] = "Local relay is not available";
const char kInvalidFilterError[] = "Invalid filter object";

// Status cache timeout (5 seconds)
constexpr base::TimeDelta kStatusCacheTimeout = base::Seconds(5);

}  // namespace

gin::WrapperInfo NostrRelayBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
v8::Local<v8::Value> NostrRelayBindings::Create(
    v8::Isolate* isolate,
    content::RenderFrame* render_frame) {
  return gin::CreateHandle(isolate, new NostrRelayBindings(render_frame))
      .ToV8();
}

NostrRelayBindings::NostrRelayBindings(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      render_frame_(render_frame),
      next_request_id_(1),
      cached_connected_(false),
      cached_event_count_(0),
      cached_storage_used_(0) {
  // Request initial status update
  SendRelayGetStatus(GetNextRequestId());
}

NostrRelayBindings::~NostrRelayBindings() = default;

gin::ObjectTemplateBuilder NostrRelayBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<NostrRelayBindings>::GetObjectTemplateBuilder(isolate)
      .SetProperty("url", &NostrRelayBindings::GetUrl)
      .SetProperty("connected", &NostrRelayBindings::GetConnected)
      .SetProperty("eventCount", &NostrRelayBindings::GetEventCount)
      .SetProperty("storageUsed", &NostrRelayBindings::GetStorageUsed)
      .SetMethod("query", &NostrRelayBindings::Query)
      .SetMethod("count", &NostrRelayBindings::Count)
      .SetMethod("deleteEvents", &NostrRelayBindings::DeleteEvents);
}

std::string NostrRelayBindings::GetUrl() const {
  // Request status update if cache is stale
  if (base::Time::Now() - last_status_update_ > kStatusCacheTimeout) {
    const_cast<NostrRelayBindings*>(this)->SendRelayGetStatus(
        const_cast<NostrRelayBindings*>(this)->GetNextRequestId());
  }
  return cached_url_;
}

bool NostrRelayBindings::GetConnected() const {
  if (base::Time::Now() - last_status_update_ > kStatusCacheTimeout) {
    const_cast<NostrRelayBindings*>(this)->SendRelayGetStatus(
        const_cast<NostrRelayBindings*>(this)->GetNextRequestId());
  }
  return cached_connected_;
}

int64_t NostrRelayBindings::GetEventCount() const {
  if (base::Time::Now() - last_status_update_ > kStatusCacheTimeout) {
    const_cast<NostrRelayBindings*>(this)->SendRelayGetStatus(
        const_cast<NostrRelayBindings*>(this)->GetNextRequestId());
  }
  return cached_event_count_;
}

int64_t NostrRelayBindings::GetStorageUsed() const {
  if (base::Time::Now() - last_status_update_ > kStatusCacheTimeout) {
    const_cast<NostrRelayBindings*>(this)->SendRelayGetStatus(
        const_cast<NostrRelayBindings*>(this)->GetNextRequestId());
  }
  return cached_storage_used_;
}

v8::Local<v8::Promise> NostrRelayBindings::Query(
    v8::Isolate* isolate,
    v8::Local<v8::Object> filter) {
  if (!cached_connected_) {
    return CreateErrorPromise(isolate, kRelayNotAvailableError);
  }

  auto resolver = v8::Promise::Resolver::New(
      isolate->GetCurrentContext()).ToLocalChecked();
  
  int request_id = GetNextRequestId();
  pending_resolvers_[request_id].Reset(isolate, resolver);
  
  base::Value::Dict filter_dict = ConvertFilterToDict(isolate, filter);
  if (filter_dict.empty()) {
    return CreateErrorPromise(isolate, kInvalidFilterError);
  }
  
  SendRelayQuery(request_id, filter_dict);
  
  return resolver->GetPromise();
}

v8::Local<v8::Promise> NostrRelayBindings::Count(
    v8::Isolate* isolate,
    v8::Local<v8::Object> filter) {
  if (!cached_connected_) {
    return CreateErrorPromise(isolate, kRelayNotAvailableError);
  }

  auto resolver = v8::Promise::Resolver::New(
      isolate->GetCurrentContext()).ToLocalChecked();
  
  int request_id = GetNextRequestId();
  pending_resolvers_[request_id].Reset(isolate, resolver);
  
  base::Value::Dict filter_dict = ConvertFilterToDict(isolate, filter);
  if (filter_dict.empty()) {
    return CreateErrorPromise(isolate, kInvalidFilterError);
  }
  
  SendRelayCount(request_id, filter_dict);
  
  return resolver->GetPromise();
}

v8::Local<v8::Promise> NostrRelayBindings::DeleteEvents(
    v8::Isolate* isolate,
    v8::Local<v8::Object> filter) {
  if (!cached_connected_) {
    return CreateErrorPromise(isolate, kRelayNotAvailableError);
  }

  auto resolver = v8::Promise::Resolver::New(
      isolate->GetCurrentContext()).ToLocalChecked();
  
  int request_id = GetNextRequestId();
  pending_resolvers_[request_id].Reset(isolate, resolver);
  
  base::Value::Dict filter_dict = ConvertFilterToDict(isolate, filter);
  if (filter_dict.empty()) {
    return CreateErrorPromise(isolate, kInvalidFilterError);
  }
  
  SendRelayDelete(request_id, filter_dict);
  
  return resolver->GetPromise();
}

v8::Local<v8::Promise> NostrRelayBindings::CreateErrorPromise(
    v8::Isolate* isolate,
    const std::string& error) {
  auto resolver = v8::Promise::Resolver::New(
      isolate->GetCurrentContext()).ToLocalChecked();
  
  v8::Local<v8::Value> error_value = v8::Exception::Error(
      gin::StringToV8(isolate, error)).As<v8::Object>();
  
  resolver->Reject(isolate->GetCurrentContext(), error_value).ToChecked();
  
  return resolver->GetPromise();
}

void NostrRelayBindings::OnRelayQueryResponse(
    int request_id,
    bool success,
    const std::vector<NostrEvent>& events) {
  auto it = pending_resolvers_.find(request_id);
  if (it == pending_resolvers_.end())
    return;
  
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = render_frame_->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Promise::Resolver> resolver = it->second.Get(isolate);
  
  if (!success) {
    resolver->Reject(context, 
                    v8::Exception::Error(gin::StringToV8(isolate, "Query failed")))
        .ToChecked();
  } else {
    // Convert events list to V8 array
    v8::Local<v8::Array> result = v8::Array::New(isolate, events.size());
    for (size_t i = 0; i < events.size(); i++) {
      // Convert NostrEvent to V8 object
      v8::Local<v8::Object> event_obj = v8::Object::New(isolate);
      
      event_obj->Set(context, gin::StringToV8(isolate, "id"), 
                    gin::StringToV8(isolate, events[i].id)).Check();
      event_obj->Set(context, gin::StringToV8(isolate, "pubkey"), 
                    gin::StringToV8(isolate, events[i].pubkey)).Check();
      event_obj->Set(context, gin::StringToV8(isolate, "created_at"), 
                    v8::Number::New(isolate, events[i].created_at)).Check();
      event_obj->Set(context, gin::StringToV8(isolate, "kind"), 
                    v8::Number::New(isolate, events[i].kind)).Check();
      event_obj->Set(context, gin::StringToV8(isolate, "content"), 
                    gin::StringToV8(isolate, events[i].content)).Check();
      event_obj->Set(context, gin::StringToV8(isolate, "sig"), 
                    gin::StringToV8(isolate, events[i].sig)).Check();
      
      // Convert tags array
      v8::Local<v8::Array> tags_array = v8::Array::New(isolate, events[i].tags.size());
      for (size_t j = 0; j < events[i].tags.size(); j++) {
        v8::Local<v8::Array> tag = v8::Array::New(isolate, events[i].tags[j].size());
        for (size_t k = 0; k < events[i].tags[j].size(); k++) {
          tag->Set(context, k, gin::StringToV8(isolate, events[i].tags[j][k])).Check();
        }
        tags_array->Set(context, j, tag).Check();
      }
      event_obj->Set(context, gin::StringToV8(isolate, "tags"), tags_array).Check();
      
      result->Set(context, i, event_obj).ToChecked();
    }
    resolver->Resolve(context, result).ToChecked();
  }
  
  pending_resolvers_.erase(it);
}

void NostrRelayBindings::OnRelayCountResponse(
    int request_id,
    bool success,
    int count) {
  auto it = pending_resolvers_.find(request_id);
  if (it == pending_resolvers_.end())
    return;
  
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = render_frame_->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Promise::Resolver> resolver = it->second.Get(isolate);
  
  if (!success) {
    resolver->Reject(context, 
                    v8::Exception::Error(gin::StringToV8(isolate, "Count failed")))
        .ToChecked();
  } else {
    resolver->Resolve(context, v8::Number::New(isolate, count)).ToChecked();
  }
  
  pending_resolvers_.erase(it);
}

void NostrRelayBindings::OnRelayDeleteResponse(
    int request_id,
    bool success,
    int deleted_count) {
  auto it = pending_resolvers_.find(request_id);
  if (it == pending_resolvers_.end())
    return;
  
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = render_frame_->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  
  v8::Local<v8::Promise::Resolver> resolver = it->second.Get(isolate);
  
  if (!success) {
    resolver->Reject(context, 
                    v8::Exception::Error(gin::StringToV8(isolate, "Delete failed")))
        .ToChecked();
  } else {
    resolver->Resolve(context, v8::Number::New(isolate, deleted_count)).ToChecked();
  }
  
  pending_resolvers_.erase(it);
}

void NostrRelayBindings::OnRelayStatusResponse(
    int request_id,
    bool connected,
    int64_t event_count,
    int64_t storage_used) {
  // Update cached values
  cached_connected_ = connected;
  cached_event_count_ = event_count;
  cached_storage_used_ = storage_used;
  
  // Construct the URL based on connection status
  if (connected) {
    // Default local relay URL
    cached_url_ = "ws://127.0.0.1:8081";
  } else {
    cached_url_ = "";
  }
  
  last_status_update_ = base::Time::Now();
}

bool NostrRelayBindings::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NostrRelayBindings, message)
    IPC_MESSAGE_HANDLER(NostrMsg_RelayQueryResponse, OnRelayQueryResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_RelayCountResponse, OnRelayCountResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_RelayDeleteResponse, OnRelayDeleteResponse)
    IPC_MESSAGE_HANDLER(NostrMsg_RelayStatusResponse, OnRelayStatusResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  
  return handled;
}

void NostrRelayBindings::OnDestruct() {
  delete this;
}

void NostrRelayBindings::SendRelayQuery(int request_id,
                                        const base::Value::Dict& filter) {
  // Default limit of 1000 events
  int limit = 1000;
  if (auto* limit_value = filter.FindInt("limit")) {
    limit = *limit_value;
  }
  
  render_frame_->Send(new NostrHostMsg_RelayQuery(
      render_frame_->GetRoutingID(), request_id, filter.Clone(), limit));
}

void NostrRelayBindings::SendRelayCount(int request_id,
                                        const base::Value::Dict& filter) {
  render_frame_->Send(new NostrHostMsg_RelayCount(
      render_frame_->GetRoutingID(), request_id, filter.Clone()));
}

void NostrRelayBindings::SendRelayDelete(int request_id,
                                         const base::Value::Dict& filter) {
  // Get the origin for permission checking
  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();
  url::Origin origin;
  if (web_frame) {
    origin = url::Origin::Create(GURL(web_frame->GetDocument().Url()));
  }
  
  render_frame_->Send(new NostrHostMsg_RelayDelete(
      render_frame_->GetRoutingID(), request_id, filter.Clone(), origin));
}

void NostrRelayBindings::SendRelayGetStatus(int request_id) {
  render_frame_->Send(new NostrHostMsg_RelayGetStatus(
      render_frame_->GetRoutingID(), request_id));
}

int NostrRelayBindings::GetNextRequestId() {
  return next_request_id_++;
}

base::Value::Dict NostrRelayBindings::ConvertFilterToDict(
    v8::Isolate* isolate,
    v8::Local<v8::Object> filter) {
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  
  // Convert V8 object to JSON string
  v8::MaybeLocal<v8::String> maybe_json = v8::JSON::Stringify(context, filter);
  if (maybe_json.IsEmpty()) {
    return base::Value::Dict();
  }
  
  v8::Local<v8::String> json = maybe_json.ToLocalChecked();
  std::string json_str = gin::V8ToString(isolate, json);
  
  // Parse JSON string to base::Value
  auto result = base::JSONReader::Read(json_str);
  if (!result || !result->is_dict()) {
    return base::Value::Dict();
  }
  
  return std::move(result->GetDict());
}

}  // namespace tungsten