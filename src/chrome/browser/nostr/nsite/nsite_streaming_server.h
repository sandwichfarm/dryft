// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NSITE_NSITE_STREAMING_SERVER_H_
#define CHROME_BROWSER_NOSTR_NSITE_NSITE_STREAMING_SERVER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/socket/server_socket.h"

namespace nostr {

class NsiteCacheManager;
class NsiteSecurityUtils;
class NsiteUpdateMonitor;
class NsiteNotificationManager;

// HTTP server for streaming nsite content with dynamic port allocation
class NsiteStreamingServer : public net::HttpServer::Delegate {
 public:
  // Endpoint paths
  static constexpr char kDismissEndpoint[] = "/.nsite-dismiss";
  explicit NsiteStreamingServer(base::FilePath profile_path);
  ~NsiteStreamingServer() override;

  // Start the server on a dynamically allocated port
  // Returns the allocated port on success, 0 on failure
  uint16_t Start();

  // Stop the server
  void Stop();

  // Check if server is running
  bool IsRunning() const { return server_ != nullptr; }

  // Get the server port (0 if not running)
  uint16_t GetPort() const { return port_; }

  // Cache management (for testing and future content injection)
  void CacheFile(const std::string& npub,
                 const std::string& path,
                 const std::string& content,
                 const std::string& content_type);
  
  // Get cache manager (for testing)
  NsiteCacheManager* GetCacheManager() const { return cache_manager_.get(); }

  // Set update monitor (called by NsiteService)
  void SetUpdateMonitor(NsiteUpdateMonitor* update_monitor);

  // Set notification manager (called by NsiteService)
  // Takes ownership reference to the notification manager instance.
  // Must be called on the sequence that owns this server.
  // The manager pointer is not owned and must remain valid for the lifetime of this server.
  void SetNotificationManager(NsiteNotificationManager* notification_manager);

  // net::HttpServer::Delegate implementation
  void OnConnect(int connection_id) override;
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override;
  void OnWebSocketRequest(int connection_id,
                         const net::HttpServerRequestInfo& info) override;
  void OnWebSocketMessage(int connection_id, std::string_view data) override;
  void OnClose(int connection_id) override;

 private:
  // Port allocation strategies
  uint16_t AllocatePort();
  uint16_t TryRandomPorts();
  uint16_t TrySequentialPorts();
  bool IsPortBlacklisted(uint16_t port);
  bool TryBindPort(uint16_t port);

  // Request handling
  struct RequestContext {
    std::string npub;
    std::string path;
    std::string method;
    std::string session_id;
    bool valid = false;
    bool from_session = false;
  };

  RequestContext ParseNsiteRequest(const net::HttpServerRequestInfo& info);
  void HandleNsiteRequest(int connection_id, const RequestContext& context);
  void SendErrorResponse(int connection_id, int status_code,
                        const std::string& message);

  // Session management
  std::string GetOrCreateSession(const net::HttpServerRequestInfo& info);
  void UpdateSession(const std::string& session_id, const std::string& npub);
  std::string GetNpubFromSession(const std::string& session_id);

  base::FilePath profile_path_;
  std::unique_ptr<net::HttpServer> server_;
  std::unique_ptr<net::ServerSocket> server_socket_;
  std::unique_ptr<NsiteCacheManager> cache_manager_;
  uint16_t port_ = 0;

  // Update monitoring (not owned)
  NsiteUpdateMonitor* update_monitor_ = nullptr;

  // Notification management (not owned)
  NsiteNotificationManager* notification_manager_ = nullptr;

  // Session tracking: session_id -> npub
  std::map<std::string, std::string> sessions_;
  
  // Security utilities
  std::unique_ptr<NsiteSecurityUtils::RateLimiter> rate_limiter_;
  
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<NsiteStreamingServer> weak_factory_{this};
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NSITE_NSITE_STREAMING_SERVER_H_