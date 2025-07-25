// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NOSTR_BLOSSOM_BINDINGS_H_
#define CONTENT_RENDERER_NOSTR_BLOSSOM_BINDINGS_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "v8/include/v8-forward.h"

namespace content {
class RenderFrame;
}

namespace dryft {

// BlossomBindings implements the window.blossom API, providing JavaScript
// access to Blossom content-addressed storage functionality including
// upload, download, server management, and mirroring capabilities.
class BlossomBindings : public gin::Wrappable<BlossomBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  // Install window.blossom on the global object
  static void Install(v8::Local<v8::Object> global,
                      content::RenderFrame* render_frame);

  // gin::Wrappable
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

 private:
  explicit BlossomBindings(content::RenderFrame* render_frame);
  ~BlossomBindings() override;

  // Core upload/download methods
  v8::Local<v8::Promise> Upload(v8::Isolate* isolate,
                                v8::Local<v8::Value> file);
  v8::Local<v8::Promise> UploadMultiple(v8::Isolate* isolate,
                                        v8::Local<v8::Array> files);
  v8::Local<v8::Promise> Get(v8::Isolate* isolate,
                              const std::string& hash);
  std::string GetUrl(const std::string& hash);

  // Availability check methods
  v8::Local<v8::Promise> Has(v8::Isolate* isolate,
                              const std::string& hash);
  v8::Local<v8::Promise> HasMultiple(v8::Isolate* isolate,
                                      v8::Local<v8::Array> hashes);

  // Server management - returns an object with list/add/remove/test methods
  v8::Local<v8::Object> GetServers(v8::Isolate* isolate);

  // Local server access - returns an object with url/enabled/storageUsed getters
  v8::Local<v8::Object> GetLocal(v8::Isolate* isolate);

  // Mirror content across servers
  v8::Local<v8::Promise> Mirror(v8::Isolate* isolate,
                                 const std::string& hash,
                                 v8::Local<v8::Value> servers);

  // Authorization helper
  v8::Local<v8::Promise> CreateAuth(v8::Isolate* isolate,
                                     v8::Local<v8::Object> params);

  // Helper methods for server management object
  v8::Local<v8::Promise> ServersList(v8::Isolate* isolate);
  v8::Local<v8::Promise> ServersAdd(v8::Isolate* isolate,
                                     const std::string& url);
  v8::Local<v8::Promise> ServersRemove(v8::Isolate* isolate,
                                        const std::string& url);
  v8::Local<v8::Promise> ServersTest(v8::Isolate* isolate,
                                      const std::string& url);

  // Helper methods for local server object
  void LocalUrl(v8::Local<v8::String> property,
                const v8::PropertyCallbackInfo<v8::Value>& info);
  void LocalEnabled(v8::Local<v8::String> property,
                    const v8::PropertyCallbackInfo<v8::Value>& info);
  void LocalStorageUsed(v8::Local<v8::String> property,
                        const v8::PropertyCallbackInfo<v8::Value>& info);
  void LocalFileCount(v8::Local<v8::String> property,
                      const v8::PropertyCallbackInfo<v8::Value>& info);
  v8::Local<v8::Promise> LocalClear(v8::Isolate* isolate);
  v8::Local<v8::Promise> LocalPrune(v8::Isolate* isolate,
                                     v8::Local<v8::Value> older_than);
  v8::Local<v8::Promise> LocalSetQuota(v8::Isolate* isolate,
                                        double bytes);

  // IPC callback handlers
  void OnUploadResponse(std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
                        bool success,
                        const base::Value::Dict& result);
  void OnGetResponse(std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
                     bool success,
                     const std::vector<uint8_t>& data);
  void OnHasResponse(std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
                     bool success,
                     bool exists);
  void OnListServersResponse(std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
                             bool success,
                             const base::Value::List& servers);
  void OnMirrorResponse(std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
                        bool success,
                        const base::Value::Dict& results);
  void OnAuthResponse(std::unique_ptr<v8::Global<v8::Promise::Resolver>> resolver,
                      bool success,
                      const base::Value::Dict& event);

  content::RenderFrame* render_frame_;
  int next_request_id_ = 1;

  // Track pending promises
  std::map<int, std::unique_ptr<v8::Global<v8::Promise::Resolver>>> pending_promises_;

  base::WeakPtrFactory<BlossomBindings> weak_factory_{this};
};

}  // namespace dryft

#endif  // CONTENT_RENDERER_NOSTR_BLOSSOM_BINDINGS_H_