// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blossom/blossom_request_handler.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_request_headers.h"

namespace blossom {

namespace {

// HTTP headers
const char kAccessControlAllowOrigin[] = "Access-Control-Allow-Origin";
const char kAccessControlAllowMethods[] = "Access-Control-Allow-Methods";
const char kAccessControlAllowHeaders[] = "Access-Control-Allow-Headers";
const char kAccessControlMaxAge[] = "Access-Control-Max-Age";
const char kCacheControl[] = "Cache-Control";
const char kContentType[] = "Content-Type";
const char kContentLength[] = "Content-Length";
const char kContentRange[] = "Content-Range";
const char kAcceptRanges[] = "Accept-Ranges";
const char kWwwAuthenticate[] = "WWW-Authenticate";
const char kXReason[] = "X-Reason";

// Cache control for immutable content
const char kImmutableCacheControl[] = "public, max-age=31536000, immutable";

// Supported CORS methods
const char kCorsMethods[] = "GET, HEAD, OPTIONS, PUT, DELETE";

// Extract hash from path like /abc123...def or /abc123...def.jpg
std::string ExtractHashFromPathInternal(const std::string& path) {
  if (path.empty() || path[0] != '/') {
    return "";
  }
  
  // Remove leading slash
  std::string filename = path.substr(1);
  
  // Find optional extension
  size_t dot_pos = filename.find('.');
  if (dot_pos != std::string::npos) {
    filename = filename.substr(0, dot_pos);
  }
  
  // Validate hash format (64 hex chars)
  if (filename.length() != 64) {
    return "";
  }
  
  for (char c : filename) {
    if (!base::IsHexDigit(c)) {
      return "";
    }
  }
  
  return filename;
}

}  // namespace

BlossomRequestHandler::BlossomRequestHandler(
    scoped_refptr<BlossomStorage> storage,
    std::unique_ptr<AuthorizationManager> auth_manager)
    : storage_(std::move(storage)),
      auth_manager_(std::move(auth_manager)) {
  DCHECK(storage_);
}

BlossomRequestHandler::~BlossomRequestHandler() = default;

void BlossomRequestHandler::HandleRequest(
    const net::HttpServerRequestInfo& request,
    ResponseCallback callback) {
  // Route based on method
  if (request.method == "GET" && request.path == "/list") {
    HandleList(request, std::move(callback));
  } else if (request.method == "GET") {
    HandleGet(request, std::move(callback));
  } else if (request.method == "HEAD") {
    HandleHead(request, std::move(callback));
  } else if (request.method == "OPTIONS") {
    HandleOptions(request, std::move(callback));
  } else if (request.method == "PUT") {
    HandlePut(request, std::move(callback));
  } else if (request.method == "DELETE") {
    HandleDelete(request, std::move(callback));
  } else {
    auto response = std::make_unique<net::HttpServerResponseInfo>(
        net::HTTP_METHOD_NOT_ALLOWED);
    AddCorsHeaders(response.get());
    std::move(callback).Run(net::HTTP_METHOD_NOT_ALLOWED, 
                           std::move(response), "");
  }
}

void BlossomRequestHandler::HandleGet(
    const net::HttpServerRequestInfo& request,
    ResponseCallback callback) {
  std::string hash = ExtractHashFromPath(request.path);
  if (hash.empty()) {
    auto response = std::make_unique<net::HttpServerResponseInfo>(
        net::HTTP_NOT_FOUND);
    AddCorsHeaders(response.get());
    std::move(callback).Run(net::HTTP_NOT_FOUND, 
                           std::move(response), "Not found");
    return;
  }
  
  // Get content from storage
  storage_->GetContent(
      hash,
      base::BindOnce(
          [](base::WeakPtr<BlossomRequestHandler> handler,
             net::HttpServerRequestInfo request,
             ResponseCallback callback,
             std::string hash,
             bool success,
             const std::string& data,
             const std::string& mime_type) {
            if (!handler || !success) {
              auto response = std::make_unique<net::HttpServerResponseInfo>(
                  net::HTTP_NOT_FOUND);
              handler->AddCorsHeaders(response.get());
              std::move(callback).Run(net::HTTP_NOT_FOUND,
                                     std::move(response), "Not found");
              return;
            }
            
            // Check for range request
            auto it = request.headers.find("range");
            if (it != request.headers.end()) {
              auto [start, end] = handler->ParseRangeHeader(
                  it->second, data.size());
              
              if (start >= 0 && end >= start && 
                  static_cast<size_t>(end) < data.size()) {
                // Send partial content
                auto response = std::make_unique<net::HttpServerResponseInfo>(
                    net::HTTP_PARTIAL_CONTENT);
                handler->AddCorsHeaders(response.get());
                response->AddHeader(kContentType, mime_type);
                response->AddHeader(kContentRange,
                    base::StringPrintf("bytes %lld-%lld/%zu",
                                      start, end, data.size()));
                response->AddHeader(kAcceptRanges, "bytes");
                response->AddHeader(kCacheControl, kImmutableCacheControl);
                
                std::string partial_data = data.substr(start, end - start + 1);
                std::move(callback).Run(net::HTTP_PARTIAL_CONTENT,
                                       std::move(response), partial_data);
                return;
              }
            }
            
            // Send full content
            auto response = std::make_unique<net::HttpServerResponseInfo>(
                net::HTTP_OK);
            handler->AddCorsHeaders(response.get());
            response->AddHeader(kContentType, mime_type);
            response->AddHeader(kContentLength, 
                               base::NumberToString(data.size()));
            response->AddHeader(kAcceptRanges, "bytes");
            response->AddHeader(kCacheControl, kImmutableCacheControl);
            
            std::move(callback).Run(net::HTTP_OK, std::move(response), data);
          },
          weak_factory_.GetWeakPtr(),
          request,
          std::move(callback),
          hash));
}

void BlossomRequestHandler::HandleHead(
    const net::HttpServerRequestInfo& request,
    ResponseCallback callback) {
  std::string hash = ExtractHashFromPath(request.path);
  if (hash.empty()) {
    auto response = std::make_unique<net::HttpServerResponseInfo>(
        net::HTTP_NOT_FOUND);
    AddCorsHeaders(response.get());
    std::move(callback).Run(net::HTTP_NOT_FOUND, 
                           std::move(response), "");
    return;
  }
  
  // Get metadata from storage
  storage_->GetMetadata(
      hash,
      base::BindOnce(
          [](base::WeakPtr<BlossomRequestHandler> handler,
             ResponseCallback callback,
             std::unique_ptr<BlobMetadata> metadata) {
            if (!handler || !metadata) {
              auto response = std::make_unique<net::HttpServerResponseInfo>(
                  net::HTTP_NOT_FOUND);
              handler->AddCorsHeaders(response.get());
              std::move(callback).Run(net::HTTP_NOT_FOUND,
                                     std::move(response), "");
              return;
            }
            
            // Send headers without body
            auto response = std::make_unique<net::HttpServerResponseInfo>(
                net::HTTP_OK);
            handler->AddCorsHeaders(response.get());
            response->AddHeader(kContentType, metadata->mime_type);
            response->AddHeader(kContentLength,
                               base::NumberToString(metadata->size));
            response->AddHeader(kAcceptRanges, "bytes");
            response->AddHeader(kCacheControl, kImmutableCacheControl);
            
            std::move(callback).Run(net::HTTP_OK, std::move(response), "");
          },
          weak_factory_.GetWeakPtr(),
          std::move(callback)));
}

void BlossomRequestHandler::HandleOptions(
    const net::HttpServerRequestInfo& request,
    ResponseCallback callback) {
  auto response = std::make_unique<net::HttpServerResponseInfo>(net::HTTP_OK);
  AddCorsHeaders(response.get());
  response->AddHeader(kAccessControlAllowMethods, kCorsMethods);
  response->AddHeader(kAccessControlAllowHeaders, 
                     "Content-Type, Authorization");
  response->AddHeader(kAccessControlMaxAge, "86400");  // 24 hours
  
  std::move(callback).Run(net::HTTP_OK, std::move(response), "");
}

void BlossomRequestHandler::HandlePut(
    const net::HttpServerRequestInfo& request,
    ResponseCallback callback) {
  // Check authorization first
  CheckAuthorization(
      request, "upload",
      base::BindOnce(
          [](base::WeakPtr<BlossomRequestHandler> handler,
             net::HttpServerRequestInfo request,
             ResponseCallback callback,
             bool authorized,
             const std::string& reason) {
            if (!handler) {
              return;
            }
            
            if (!authorized) {
              auto response = std::make_unique<net::HttpServerResponseInfo>(
                  net::HTTP_UNAUTHORIZED);
              handler->AddCorsHeaders(response.get());
              response->AddHeader(kWwwAuthenticate, "Nostr");
              response->AddHeader(kXReason, reason);
              std::move(callback).Run(net::HTTP_UNAUTHORIZED,
                                     std::move(response), "");
              return;
            }
            
            // Extract hash from path
            std::string hash = handler->ExtractHashFromPath(request.path);
            if (hash.empty()) {
              auto response = std::make_unique<net::HttpServerResponseInfo>(
                  net::HTTP_BAD_REQUEST);
              handler->AddCorsHeaders(response.get());
              std::move(callback).Run(net::HTTP_BAD_REQUEST,
                                     std::move(response), "Invalid hash");
              return;
            }
            
            // Get MIME type from Content-Type header
            std::string mime_type = "application/octet-stream";
            auto it = request.headers.find("content-type");
            if (it != request.headers.end()) {
              mime_type = it->second;
            }
            
            // Store content
            handler->storage_->StoreContent(
                hash, request.data, mime_type,
                base::BindOnce(
                    [](base::WeakPtr<BlossomRequestHandler> handler,
                       ResponseCallback callback,
                       bool success,
                       const std::string& error) {
                      if (!handler) {
                        return;
                      }
                      
                      if (!success) {
                        auto response = 
                            std::make_unique<net::HttpServerResponseInfo>(
                                net::HTTP_BAD_REQUEST);
                        handler->AddCorsHeaders(response.get());
                        std::move(callback).Run(net::HTTP_BAD_REQUEST,
                                               std::move(response), error);
                        return;
                      }
                      
                      auto response = 
                          std::make_unique<net::HttpServerResponseInfo>(
                              net::HTTP_CREATED);
                      handler->AddCorsHeaders(response.get());
                      std::move(callback).Run(net::HTTP_CREATED,
                                             std::move(response), "");
                    },
                    handler,
                    std::move(callback)));
          },
          weak_factory_.GetWeakPtr(),
          request,
          std::move(callback)));
}

void BlossomRequestHandler::HandleDelete(
    const net::HttpServerRequestInfo& request,
    ResponseCallback callback) {
  // Check authorization first
  CheckAuthorization(
      request, "delete",
      base::BindOnce(
          [](base::WeakPtr<BlossomRequestHandler> handler,
             net::HttpServerRequestInfo request,
             ResponseCallback callback,
             bool authorized,
             const std::string& reason) {
            if (!handler) {
              return;
            }
            
            if (!authorized) {
              auto response = std::make_unique<net::HttpServerResponseInfo>(
                  net::HTTP_UNAUTHORIZED);
              handler->AddCorsHeaders(response.get());
              response->AddHeader(kWwwAuthenticate, "Nostr");
              response->AddHeader(kXReason, reason);
              std::move(callback).Run(net::HTTP_UNAUTHORIZED,
                                     std::move(response), "");
              return;
            }
            
            // Extract hash from path
            std::string hash = handler->ExtractHashFromPath(request.path);
            if (hash.empty()) {
              auto response = std::make_unique<net::HttpServerResponseInfo>(
                  net::HTTP_BAD_REQUEST);
              handler->AddCorsHeaders(response.get());
              std::move(callback).Run(net::HTTP_BAD_REQUEST,
                                     std::move(response), "Invalid hash");
              return;
            }
            
            // Delete content
            handler->storage_->DeleteContent(
                hash,
                base::BindOnce(
                    [](base::WeakPtr<BlossomRequestHandler> handler,
                       ResponseCallback callback,
                       bool success) {
                      if (!handler) {
                        return;
                      }
                      
                      auto status = success ? net::HTTP_NO_CONTENT 
                                           : net::HTTP_NOT_FOUND;
                      auto response = 
                          std::make_unique<net::HttpServerResponseInfo>(status);
                      handler->AddCorsHeaders(response.get());
                      std::move(callback).Run(status, std::move(response), "");
                    },
                    handler,
                    std::move(callback)));
          },
          weak_factory_.GetWeakPtr(),
          request,
          std::move(callback)));
}

void BlossomRequestHandler::HandleList(
    const net::HttpServerRequestInfo& request,
    ResponseCallback callback) {
  // Check authorization
  CheckAuthorization(
      request, "list",
      base::BindOnce(
          [](base::WeakPtr<BlossomRequestHandler> handler,
             ResponseCallback callback,
             bool authorized,
             const std::string& reason) {
            if (!handler) {
              return;
            }
            
            if (!authorized) {
              auto response = std::make_unique<net::HttpServerResponseInfo>(
                  net::HTTP_UNAUTHORIZED);
              handler->AddCorsHeaders(response.get());
              response->AddHeader(kWwwAuthenticate, "Nostr");
              response->AddHeader(kXReason, reason);
              std::move(callback).Run(net::HTTP_UNAUTHORIZED,
                                     std::move(response), "");
              return;
            }
            
            // TODO: Implement blob listing
            auto response = std::make_unique<net::HttpServerResponseInfo>(
                net::HTTP_NOT_IMPLEMENTED);
            handler->AddCorsHeaders(response.get());
            std::move(callback).Run(net::HTTP_NOT_IMPLEMENTED,
                                   std::move(response), "Not implemented");
          },
          weak_factory_.GetWeakPtr(),
          std::move(callback)));
}

std::string BlossomRequestHandler::ExtractHashFromPath(
    const std::string& path) const {
  return ExtractHashFromPathInternal(path);
}

bool BlossomRequestHandler::ValidateHash(const std::string& hash) const {
  if (hash.length() != 64) {
    return false;
  }
  
  for (char c : hash) {
    if (!base::IsHexDigit(c)) {
      return false;
    }
  }
  
  return true;
}

void BlossomRequestHandler::AddCorsHeaders(
    net::HttpServerResponseInfo* response) const {
  response->AddHeader(kAccessControlAllowOrigin, "*");
}

std::pair<int64_t, int64_t> BlossomRequestHandler::ParseRangeHeader(
    const std::string& range_header,
    int64_t content_length) const {
  // Parse "bytes=start-end" format
  if (!base::StartsWith(range_header, "bytes=", 
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return {-1, -1};
  }
  
  std::string range_spec = range_header.substr(6);
  std::vector<std::string> parts = base::SplitString(
      range_spec, "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  
  if (parts.size() != 2) {
    return {-1, -1};
  }
  
  int64_t start = -1;
  int64_t end = -1;
  
  // Parse start
  if (!parts[0].empty()) {
    if (!base::StringToInt64(parts[0], &start) || start < 0) {
      return {-1, -1};
    }
  }
  
  // Parse end
  if (!parts[1].empty()) {
    if (!base::StringToInt64(parts[1], &end) || end < 0) {
      return {-1, -1};
    }
  } else {
    // No end specified, use content length
    end = content_length - 1;
  }
  
  // Handle suffix range (e.g., "-500" for last 500 bytes)
  if (parts[0].empty() && !parts[1].empty()) {
    int64_t suffix_length;
    if (base::StringToInt64(parts[1], &suffix_length) && suffix_length > 0) {
      start = content_length - suffix_length;
      end = content_length - 1;
    }
  }
  
  // Validate range
  if (start < 0 || end < start || end >= content_length) {
    return {-1, -1};
  }
  
  return {start, end};
}

void BlossomRequestHandler::CheckAuthorization(
    const net::HttpServerRequestInfo& request,
    const std::string& verb,
    base::OnceCallback<void(bool authorized, 
                          const std::string& reason)> callback) {
  // If no auth manager, allow everything
  if (!auth_manager_) {
    std::move(callback).Run(true, "");
    return;
  }
  
  // Extract Authorization header
  auto it = request.headers.find("authorization");
  if (it == request.headers.end()) {
    std::move(callback).Run(false, "Missing authorization header");
    return;
  }
  
  // Extract hash from path (if applicable)
  std::string hash = ExtractHashFromPath(request.path);
  
  // Check authorization
  auth_manager_->CheckAuthorization(
      it->second, verb, hash, std::move(callback));
}

}  // namespace blossom