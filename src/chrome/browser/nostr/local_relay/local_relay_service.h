// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_LOCAL_RELAY_LOCAL_RELAY_SERVICE_H_
#define CHROME_BROWSER_NOSTR_LOCAL_RELAY_LOCAL_RELAY_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/thread.h"
#include "chrome/browser/nostr/local_relay/connection_manager.h"
#include "chrome/browser/nostr/local_relay/nostr_database.h"
#include "net/base/ip_endpoint.h"
#include "net/server/http_server.h"

namespace nostr {
namespace local_relay {

// Configuration for the local relay service
struct LocalRelayConfig {
  // Network configuration
  std::string bind_address = "127.0.0.1";
  int port = 8081;
  
  // Connection limits
  int max_connections = 100;
  int max_subscriptions_per_connection = 20;
  
  // Message limits  
  size_t max_message_size = 512 * 1024;  // 512KB
  size_t max_event_size = 256 * 1024;    // 256KB
  
  // Rate limiting
  int max_events_per_minute = 100;
  int max_req_per_minute = 60;
  
  // Database configuration
  NostrDatabase::Config database_config;
};

// Main service class for the Nostr local relay
// Implements a WebSocket server that speaks the Nostr protocol
class LocalRelayService : public net::HttpServer::Delegate {
 public:
  LocalRelayService();
  ~LocalRelayService() override;

  // Service lifecycle
  void Start(const LocalRelayConfig& config,
             base::OnceCallback<void(bool success)> callback);
  void Stop(base::OnceClosure callback);
  
  // Get service status
  bool IsRunning() const { return server_ != nullptr; }
  net::IPEndPoint GetLocalAddress() const;
  
  // Get statistics
  int GetConnectionCount() const;
  int GetTotalSubscriptions() const;
  base::Value::Dict GetStatistics() const;

  // HttpServer::Delegate implementation
  void OnConnect(int connection_id) override;
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override;
  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override;
  void OnWebSocketMessage(int connection_id,
                          std::string data) override;
  void OnClose(int connection_id) override;

 private:
  // WebSocket upgrade handling
  void HandleWebSocketUpgrade(int connection_id,
                              const net::HttpServerRequestInfo& info);
  
  // Message processing
  void ProcessNostrMessage(int connection_id, const std::string& message);
  
  // Send response to client
  void SendMessage(int connection_id, const std::string& message);
  void SendError(int connection_id, 
                 const std::string& event_id,
                 const std::string& message);
  void SendNotice(int connection_id, const std::string& message);
  
  // Server thread operations
  void StartOnServerThread(const LocalRelayConfig& config,
                          base::OnceCallback<void(bool)> callback);
  void StopOnServerThread(base::OnceClosure callback);
  
  // Configuration
  LocalRelayConfig config_;
  
  // HTTP/WebSocket server
  std::unique_ptr<net::HttpServer> server_;
  
  // Server thread for network operations
  std::unique_ptr<base::Thread> server_thread_;
  
  // Connection management
  std::unique_ptr<ConnectionManager> connection_manager_;
  
  // Database for event storage
  std::unique_ptr<NostrDatabase> database_;
  
  // Local address where server is listening
  net::IPEndPoint local_address_;
  
  // Thread checker for main thread
  SEQUENCE_CHECKER(sequence_checker_);
  
  // Weak pointer factory
  base::WeakPtrFactory<LocalRelayService> weak_factory_{this};
};

}  // namespace local_relay
}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_LOCAL_RELAY_LOCAL_RELAY_SERVICE_H_