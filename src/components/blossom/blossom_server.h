// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOSSOM_BLOSSOM_SERVER_H_
#define COMPONENTS_BLOSSOM_BLOSSOM_SERVER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/thread.h"
#include "components/blossom/blossom_storage.h"
#include "net/base/ip_endpoint.h"
#include "net/server/http_server.h"

namespace blossom {

// Forward declarations
class BlossomRequestHandler;

// Configuration for Blossom HTTP server
struct ServerConfig {
  std::string bind_address = "127.0.0.1";
  int port = 8080;
  
  // Storage configuration
  StorageConfig storage_config;
  
  // Authorization settings
  bool require_auth_for_get = false;
  bool require_auth_for_upload = true;
  bool require_auth_for_list = true;
  bool require_auth_for_delete = true;
  std::string server_name;  // Server name for authorization events
  
  // Server limits
  size_t max_upload_size = 100 * 1024 * 1024;  // 100MB
  size_t max_connections = 1000;
};

// HTTP server for Blossom protocol (BUD-01)
class BlossomServer : public net::HttpServer::Delegate {
 public:
  using StartCallback = base::OnceCallback<void(bool success, const std::string& error)>;
  using StopCallback = base::OnceClosure;

  explicit BlossomServer(const ServerConfig& config);
  ~BlossomServer() override;

  // Start the HTTP server
  void Start(StartCallback callback);
  
  // Stop the HTTP server
  void Stop(StopCallback callback);
  
  // Check if server is running
  bool IsRunning() const { return server_ != nullptr; }
  
  // Get server address
  net::IPEndPoint GetServerAddress() const;
  
  // Get storage statistics
  void GetStats(base::OnceCallback<void(base::Value::Dict)> callback);

  // net::HttpServer::Delegate implementation
  void OnConnect(int connection_id) override;
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override;
  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override;
  void OnWebSocketMessage(int connection_id,
                          std::string data) override;
  void OnClose(int connection_id) override;

 private:
  // Server thread operations
  void StartOnServerThread(StartCallback callback);
  void StopOnServerThread(StopCallback callback);
  
  // Request handling
  void HandleRequest(int connection_id,
                    const net::HttpServerRequestInfo& info);
  
  // Send response helpers
  void SendResponse(int connection_id,
                   net::HttpStatusCode status,
                   const std::string& content_type,
                   const std::string& body);
  void SendErrorResponse(int connection_id,
                        net::HttpStatusCode status,
                        const std::string& reason);
  void SendFileResponse(int connection_id,
                       const base::FilePath& path,
                       const std::string& mime_type,
                       const net::HttpServerRequestInfo& info);
  
  // Add CORS headers to response
  void AddCorsHeaders(net::HttpServerResponseInfo* response);
  
  // Configuration
  ServerConfig config_;
  
  // HTTP server instance
  std::unique_ptr<net::HttpServer> server_;
  
  // Server thread for network operations
  std::unique_ptr<base::Thread> server_thread_;
  
  // Storage backend
  scoped_refptr<BlossomStorage> storage_;
  
  // Request handler
  std::unique_ptr<BlossomRequestHandler> request_handler_;
  
  // Local address where server is listening
  net::IPEndPoint local_address_;
  
  // Thread checker for main thread
  SEQUENCE_CHECKER(sequence_checker_);
  
  // Weak pointer factory
  base::WeakPtrFactory<BlossomServer> weak_factory_{this};
};

}  // namespace blossom

#endif  // COMPONENTS_BLOSSOM_BLOSSOM_SERVER_H_