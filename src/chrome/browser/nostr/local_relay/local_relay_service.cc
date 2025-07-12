// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/local_relay/local_relay_service.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/nostr/local_relay/protocol_handler.h"
#include "net/base/net_errors.h"
#include "net/server/http_server_response_info.h"

namespace nostr {
namespace local_relay {

namespace {

// WebSocket protocol headers
const char kWebSocketGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const char kWebSocketVersion[] = "13";

// HTTP response headers
const char kAccessControlAllowOrigin[] = "Access-Control-Allow-Origin";
const char kAccessControlAllowMethods[] = "Access-Control-Allow-Methods";
const char kAccessControlAllowHeaders[] = "Access-Control-Allow-Headers";

}  // namespace

LocalRelayService::LocalRelayService() = default;

LocalRelayService::~LocalRelayService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsRunning()) {
    Stop(base::DoNothing());
  }
}

void LocalRelayService::Start(const LocalRelayConfig& config,
                             base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!IsRunning());
  
  config_ = config;
  
  // Create server thread
  server_thread_ = std::make_unique<base::Thread>("NostrLocalRelay");
  if (!server_thread_->Start()) {
    LOG(ERROR) << "Failed to start server thread";
    std::move(callback).Run(false);
    return;
  }
  
  // Start server on dedicated thread
  server_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&LocalRelayService::StartOnServerThread,
                     weak_factory_.GetWeakPtr(),
                     config,
                     std::move(callback)));
}

void LocalRelayService::Stop(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!IsRunning()) {
    std::move(callback).Run();
    return;
  }
  
  server_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&LocalRelayService::StopOnServerThread,
                     weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

net::IPEndPoint LocalRelayService::GetLocalAddress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return local_address_;
}

int LocalRelayService::GetConnectionCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_manager_) {
    return 0;
  }
  return connection_manager_->GetConnectionCount();
}

int LocalRelayService::GetTotalSubscriptions() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_manager_) {
    return 0;
  }
  return connection_manager_->GetTotalSubscriptions();
}

base::Value::Dict LocalRelayService::GetStatistics() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  base::Value::Dict stats;
  stats.Set("running", IsRunning());
  stats.Set("address", local_address_.ToString());
  
  if (connection_manager_) {
    stats.Set("connections", connection_manager_->GetStatistics());
  }
  
  if (database_) {
    // Database stats would be retrieved async in real implementation
    stats.Set("database", base::Value::Dict());
  }
  
  return stats;
}

void LocalRelayService::OnConnect(int connection_id) {
  VLOG(1) << "Client connected: " << connection_id;
  
  if (!connection_manager_) {
    server_->Close(connection_id);
    return;
  }
  
  // Get client address
  std::string client_address = "unknown";
  net::IPEndPoint endpoint;
  if (server_->GetPeerAddress(connection_id, &endpoint) == net::OK) {
    client_address = endpoint.ToString();
  }
  
  // Add connection
  if (!connection_manager_->AddConnection(connection_id, client_address)) {
    LOG(WARNING) << "Connection limit reached, closing connection " 
                 << connection_id;
    server_->Close(connection_id);
  }
}

void LocalRelayService::OnHttpRequest(int connection_id,
                                     const net::HttpServerRequestInfo& info) {
  // Handle CORS preflight
  if (info.method == "OPTIONS") {
    net::HttpServerResponseInfo response(net::HTTP_OK);
    response.AddHeader(kAccessControlAllowOrigin, "*");
    response.AddHeader(kAccessControlAllowMethods, "GET, POST");
    response.AddHeader(kAccessControlAllowHeaders, "Content-Type");
    server_->SendResponse(connection_id, response);
    return;
  }
  
  // Regular HTTP not supported, only WebSocket
  net::HttpServerResponseInfo response(net::HTTP_NOT_FOUND);
  response.SetBody("This is a WebSocket-only endpoint", "text/plain");
  server_->SendResponse(connection_id, response);
}

void LocalRelayService::OnWebSocketRequest(int connection_id,
                                          const net::HttpServerRequestInfo& info) {
  // Accept WebSocket upgrade
  server_->AcceptWebSocket(connection_id, info);
  VLOG(1) << "WebSocket connection upgraded: " << connection_id;
}

void LocalRelayService::OnWebSocketMessage(int connection_id,
                                          std::string data) {
  VLOG(2) << "Received message from " << connection_id << ": " << data;
  
  // Check message size
  if (data.size() > config_.max_message_size) {
    SendNotice(connection_id, "message too large");
    return;
  }
  
  // Process Nostr protocol message
  ProcessNostrMessage(connection_id, data);
}

void LocalRelayService::OnClose(int connection_id) {
  VLOG(1) << "Client disconnected: " << connection_id;
  
  if (connection_manager_) {
    connection_manager_->RemoveConnection(connection_id);
  }
}

void LocalRelayService::HandleWebSocketUpgrade(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  // WebSocket upgrade is handled by HttpServer::AcceptWebSocket
  // This method is here for future custom upgrade logic if needed
}

void LocalRelayService::ProcessNostrMessage(int connection_id,
                                           const std::string& message) {
  if (!protocol_handler_) {
    SendNotice(connection_id, "server not ready");
    return;
  }
  
  protocol_handler_->ProcessMessage(connection_id, message);
}

void LocalRelayService::SendMessage(int connection_id,
                                   const std::string& message) {
  if (!server_) {
    return;
  }
  
  server_->SendOverWebSocket(connection_id, message);
  
  // Update statistics
  if (connection_manager_) {
    auto* conn = connection_manager_->GetConnection(connection_id);
    if (conn) {
      conn->messages_sent++;
    }
  }
}

void LocalRelayService::SendError(int connection_id,
                                 const std::string& event_id,
                                 const std::string& message) {
  auto response = ProtocolResponse::MakeOK(event_id, false, message);
  SendMessage(connection_id, response.ToJson());
}

void LocalRelayService::SendNotice(int connection_id,
                                  const std::string& message) {
  auto response = ProtocolResponse::MakeNotice(message);
  SendMessage(connection_id, response.ToJson());
}

void LocalRelayService::StartOnServerThread(
    const LocalRelayConfig& config,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(server_thread_->task_runner()->BelongsToCurrentThread());
  
  // Create database
  base::FilePath db_path = base::FilePath(FILE_PATH_LITERAL("nostr_local_relay.db"));
  database_ = std::make_unique<NostrDatabase>(db_path, config.database_config);
  
  // Initialize database synchronously for simplicity
  // In production, this would be async
  base::RunLoop run_loop;
  bool db_initialized = false;
  database_->Initialize(base::BindOnce(
      [](bool* success, base::OnceClosure quit, bool result) {
        *success = result;
        std::move(quit).Run();
      },
      &db_initialized, run_loop.QuitClosure()));
  run_loop.Run();
  
  if (!db_initialized) {
    LOG(ERROR) << "Failed to initialize database";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false));
    return;
  }
  
  // Create connection manager
  connection_manager_ = std::make_unique<ConnectionManager>(
      config.max_connections,
      config.max_subscriptions_per_connection);
  
  // Create protocol handler
  protocol_handler_ = std::make_unique<ProtocolHandler>(
      database_.get(),
      connection_manager_.get(),
      base::BindRepeating(&LocalRelayService::SendMessage,
                         weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          [](base::WeakPtr<LocalRelayService> service,
             const std::vector<int>& connection_ids,
             const std::string& subscription_id,
             const std::string& message) {
            if (!service) return;
            for (int conn_id : connection_ids) {
              service->SendMessage(conn_id, message);
            }
          },
          weak_factory_.GetWeakPtr()));
  
  protocol_handler_->SetMaxEventSize(config.max_event_size);
  
  // Create HTTP server
  server_ = std::make_unique<net::HttpServer>(
      std::make_unique<net::TCPServerSocketFactory>(),
      this);
  
  // Start listening
  net::IPAddress address;
  if (!address.AssignFromIPLiteral(config.bind_address)) {
    LOG(ERROR) << "Invalid bind address: " << config.bind_address;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false));
    return;
  }
  
  net::IPEndPoint endpoint(address, config.port);
  int result = server_->Listen(endpoint, 5);  // backlog of 5
  
  if (result != net::OK) {
    LOG(ERROR) << "Failed to listen on " << endpoint.ToString() 
               << ": " << net::ErrorToString(result);
    server_.reset();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false));
    return;
  }
  
  // Get actual listening address (in case port was 0)
  server_->GetLocalAddress(&local_address_);
  
  LOG(INFO) << "Nostr local relay listening on " << local_address_.ToString();
  
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), true));
}

void LocalRelayService::StopOnServerThread(base::OnceClosure callback) {
  DCHECK(server_thread_->task_runner()->BelongsToCurrentThread());
  
  // Stop accepting new connections
  server_.reset();
  
  // Clean up components
  protocol_handler_.reset();
  connection_manager_.reset();
  database_.reset();
  
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

}  // namespace local_relay
}  // namespace nostr