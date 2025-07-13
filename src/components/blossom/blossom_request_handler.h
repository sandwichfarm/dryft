// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOSSOM_BLOSSOM_REQUEST_HANDLER_H_
#define COMPONENTS_BLOSSOM_BLOSSOM_REQUEST_HANDLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/blossom/authorization_manager.h"
#include "components/blossom/blossom_storage.h"
#include "net/http/http_status_code.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"

namespace blossom {

// Handles HTTP requests according to BUD-01 specification
class BlossomRequestHandler {
 public:
  // Response callback with status, headers, and body
  using ResponseCallback = base::OnceCallback<void(
      net::HttpStatusCode status,
      std::unique_ptr<net::HttpServerResponseInfo> response,
      const std::string& body)>;

  BlossomRequestHandler(scoped_refptr<BlossomStorage> storage,
                       std::unique_ptr<AuthorizationManager> auth_manager);
  ~BlossomRequestHandler();

  // Handle HTTP request and return response via callback
  void HandleRequest(const net::HttpServerRequestInfo& request,
                    ResponseCallback callback);

 private:
  // Individual endpoint handlers
  void HandleGet(const net::HttpServerRequestInfo& request,
                ResponseCallback callback);
  void HandleHead(const net::HttpServerRequestInfo& request,
                 ResponseCallback callback);
  void HandleOptions(const net::HttpServerRequestInfo& request,
                    ResponseCallback callback);
  void HandlePut(const net::HttpServerRequestInfo& request,
                ResponseCallback callback);
  void HandleDelete(const net::HttpServerRequestInfo& request,
                   ResponseCallback callback);
  void HandleList(const net::HttpServerRequestInfo& request,
                 ResponseCallback callback);

  // Helper methods
  std::string ExtractHashFromPath(const std::string& path) const;
  bool ValidateHash(const std::string& hash) const;
  void AddCorsHeaders(net::HttpServerResponseInfo* response) const;
  std::pair<int64_t, int64_t> ParseRangeHeader(const std::string& range_header,
                                               int64_t content_length) const;
  
  // Check authorization for request
  void CheckAuthorization(const net::HttpServerRequestInfo& request,
                         const std::string& verb,
                         base::OnceCallback<void(bool authorized, 
                                               const std::string& reason)> callback);

  // Storage backend
  scoped_refptr<BlossomStorage> storage_;
  
  // Authorization manager
  std::unique_ptr<AuthorizationManager> auth_manager_;
  
  // Weak pointer factory
  base::WeakPtrFactory<BlossomRequestHandler> weak_factory_{this};
};

}  // namespace blossom

#endif  // COMPONENTS_BLOSSOM_BLOSSOM_REQUEST_HANDLER_H_