// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blossom/blossom_server.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "components/blossom/blossom_authorization_manager.h"
#include "components/blossom/blossom_request_handler.h"
#include "net/base/net_errors.h"
#include "net/server/http_server_response_info.h"

namespace blossom {

BlossomServer::BlossomServer(const ServerConfig& config)
    : config_(config) {}

BlossomServer::~BlossomServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsRunning()) {
    Stop(base::DoNothing());
  }
}

void BlossomServer::Start(StartCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!IsRunning());
  
  // Create storage
  storage_ = base::MakeRefCounted<BlossomStorage>(config_.storage_config);
  
  // Initialize storage
  storage_->Initialize(
      base::BindOnce(
          [](base::WeakPtr<BlossomServer> server,
             StartCallback callback,
             bool success) {
            if (!server) {
              std::move(callback).Run(false, "Server destroyed");
              return;
            }
            
            if (!success) {
              std::move(callback).Run(false, "Failed to initialize storage");
              return;
            }
            
            // Create server thread
            server->server_thread_ = std::make_unique<base::Thread>("BlossomHTTP");
            if (!server->server_thread_->Start()) {
              LOG(ERROR) << "Failed to start server thread";
              std::move(callback).Run(false, "Failed to start server thread");
              return;
            }
            
            // Start server on dedicated thread
            server->server_thread_->task_runner()->PostTask(
                FROM_HERE,
                base::BindOnce(&BlossomServer::StartOnServerThread,
                               server,
                               std::move(callback)));
          },
          weak_factory_.GetWeakPtr(),
          std::move(callback)));
}

void BlossomServer::Stop(StopCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!IsRunning()) {
    std::move(callback).Run();
    return;
  }
  
  server_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BlossomServer::StopOnServerThread,
                     weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

net::IPEndPoint BlossomServer::GetServerAddress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return local_address_;
}

void BlossomServer::GetStats(base::OnceCallback<void(base::Value::Dict)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  base::Value::Dict stats;
  stats.Set("running", IsRunning());
  stats.Set("address", local_address_.ToString());
  
  if (storage_) {
    storage_->GetStats(
        base::BindOnce(
            [](base::OnceCallback<void(base::Value::Dict)> callback,
               base::Value::Dict stats,
               int64_t total_size,
               int64_t blob_count) {
              stats.Set("storage_size", static_cast<double>(total_size));
              stats.Set("blob_count", static_cast<double>(blob_count));
              std::move(callback).Run(std::move(stats));
            },
            std::move(callback),
            std::move(stats)));
  } else {
    std::move(callback).Run(std::move(stats));
  }
}

void BlossomServer::OnConnect(int connection_id) {
  VLOG(1) << "Blossom client connected: " << connection_id;
}

void BlossomServer::OnHttpRequest(int connection_id,
                                 const net::HttpServerRequestInfo& info) {
  VLOG(2) << "Blossom request: " << info.method << " " << info.path;
  
  if (!request_handler_) {
    SendErrorResponse(connection_id, net::HTTP_INTERNAL_SERVER_ERROR,
                     "Server not initialized");
    return;
  }
  
  HandleRequest(connection_id, info);
}

void BlossomServer::OnWebSocketRequest(int connection_id,
                                      const net::HttpServerRequestInfo& info) {
  // WebSocket not supported for Blossom
  server_->Send404(connection_id);
}

void BlossomServer::OnWebSocketMessage(int connection_id,
                                      std::string data) {
  // WebSocket not supported
}

void BlossomServer::OnClose(int connection_id) {
  VLOG(1) << "Blossom client disconnected: " << connection_id;
}

void BlossomServer::StartOnServerThread(StartCallback callback) {
  DCHECK(server_thread_->task_runner()->BelongsToCurrentThread());
  
  // Create authorization manager if server name is configured
  std::unique_ptr<AuthorizationManager> auth_manager;
  if (!config_.server_name.empty()) {
    BlossomAuthorizationManager::Config auth_config;
    auth_config.server_name = config_.server_name;
    auth_config.cache_ttl = base::Hours(1);
    auth_config.max_cache_size = 1000;
    auth_config.require_expiration = true;
    
    auth_manager = std::make_unique<BlossomAuthorizationManager>(auth_config);
  }
  
  // Create request handler
  request_handler_ = std::make_unique<BlossomRequestHandler>(
      storage_, std::move(auth_manager));
  
  // Create HTTP server
  server_ = std::make_unique<net::HttpServer>(
      std::make_unique<net::TCPServerSocketFactory>(),
      this);
  
  // Start listening
  net::IPAddress address;
  if (!address.AssignFromIPLiteral(config_.bind_address)) {
    LOG(ERROR) << "Invalid bind address: " << config_.bind_address;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false, "Invalid bind address"));
    return;
  }
  
  net::IPEndPoint endpoint(address, config_.port);
  int result = server_->Listen(endpoint, 5);  // backlog of 5
  
  if (result != net::OK) {
    LOG(ERROR) << "Failed to listen on " << endpoint.ToString() 
               << ": " << net::ErrorToString(result);
    server_.reset();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false,
                      base::StringPrintf("Failed to listen: %s",
                                        net::ErrorToString(result))));
    return;
  }
  
  // Get actual listening address (in case port was 0)
  server_->GetLocalAddress(&local_address_);
  
  LOG(INFO) << "Blossom server listening on " << local_address_.ToString();
  
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), true, ""));
}

void BlossomServer::StopOnServerThread(StopCallback callback) {
  DCHECK(server_thread_->task_runner()->BelongsToCurrentThread());
  
  // Stop accepting new connections
  server_.reset();
  request_handler_.reset();
  
  // Notify completion
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      std::move(callback));
  
  // Stop the server thread
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<base::Thread> thread) {
            thread->Stop();
          },
          std::move(server_thread_)));
}

void BlossomServer::HandleRequest(int connection_id,
                                 const net::HttpServerRequestInfo& info) {
  // Pass to request handler
  request_handler_->HandleRequest(
      info,
      base::BindOnce(&BlossomServer::SendResponse,
                     weak_factory_.GetWeakPtr(),
                     connection_id,
                     info.path));
}

void BlossomServer::SendResponse(int connection_id,
                                const std::string& /*path*/,
                                net::HttpStatusCode status,
                                std::unique_ptr<net::HttpServerResponseInfo> response,
                                const std::string& body) {
  if (!server_) {
    return;
  }
  
  if (response) {
    server_->SendResponse(connection_id, *response, body);
  } else {
    // Create basic response
    net::HttpServerResponseInfo basic_response(status);
    AddCorsHeaders(&basic_response);
    server_->SendResponse(connection_id, basic_response, body);
  }
}

void BlossomServer::SendErrorResponse(int connection_id,
                                     net::HttpStatusCode status,
                                     const std::string& reason) {
  if (!server_) {
    return;
  }
  
  net::HttpServerResponseInfo response(status);
  AddCorsHeaders(&response);
  response.AddHeader("Content-Type", "text/plain");
  server_->SendResponse(connection_id, response, reason);
}

void BlossomServer::SendFileResponse(int connection_id,
                                    const base::FilePath& path,
                                    const std::string& mime_type,
                                    const net::HttpServerRequestInfo& info) {
  // Read file and send
  std::string content;
  if (!base::ReadFileToString(path, &content)) {
    SendErrorResponse(connection_id, net::HTTP_INTERNAL_SERVER_ERROR,
                     "Failed to read file");
    return;
  }
  
  net::HttpServerResponseInfo response(net::HTTP_OK);
  AddCorsHeaders(&response);
  response.AddHeader("Content-Type", mime_type);
  response.AddHeader("Content-Length", base::NumberToString(content.size()));
  response.AddHeader("Cache-Control", "public, max-age=31536000, immutable");
  response.AddHeader("Accept-Ranges", "bytes");
  
  server_->SendResponse(connection_id, response, content);
}

void BlossomServer::AddCorsHeaders(net::HttpServerResponseInfo* response) {
  response->AddHeader("Access-Control-Allow-Origin", "*");
}

}  // namespace blossom