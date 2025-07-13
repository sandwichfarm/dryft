// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_streaming_server.h"

#include <algorithm>
#include <random>
#include <set>

#include "chrome/browser/nostr/nsite/nsite_cache_manager.h"
#include "chrome/browser/nostr/nsite/nsite_notification_manager.h"
#include "chrome/browser/nostr/nsite/nsite_security_utils.h"
#include "chrome/browser/nostr/nsite/nsite_update_monitor.h"

#include "base/guid.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/socket/tcp_server_socket.h"

namespace nostr {

// static
constexpr char NsiteStreamingServer::kDismissEndpoint[];

namespace {

// Port allocation constants
constexpr uint16_t kMinEphemeralPort = 49152;
constexpr uint16_t kMaxEphemeralPort = 65535;
constexpr int kMaxRandomAttempts = 100;
constexpr int kMaxSequentialAttempts = 1000;

// Common development ports to avoid
const std::set<uint16_t> kBlacklistedPorts = {
    3000,   // React dev server
    3001,   // Create React App fallback
    3333,   // Meteor
    4200,   // Angular CLI
    5000,   // Flask, serve
    5173,   // Vite
    5432,   // PostgreSQL
    6379,   // Redis
    7000,   // Cassandra
    8000,   // Django, http-server
    8080,   // Common HTTP proxy
    8081,   // Common alternative HTTP
    8082,   // Common alternative HTTP
    8083,   // Common alternative HTTP
    8888,   // Jupyter
    9000,   // PHP-FPM, SonarQube
    9200,   // Elasticsearch
    9229,   // Node.js debugger
    27017,  // MongoDB
};

// HTTP response templates
constexpr char kErrorTemplate[] = R"(<!DOCTYPE html>
<html>
<head>
<title>Error %d</title>
<style>
body { font-family: system-ui, sans-serif; margin: 40px; }
h1 { color: #d73a49; }
p { color: #586069; }
</style>
</head>
<body>
<h1>Error %d</h1>
<p>%s</p>
</body>
</html>)";

}  // namespace

NsiteStreamingServer::NsiteStreamingServer(base::FilePath profile_path)
    : profile_path_(std::move(profile_path)),
      io_task_runner_(content::GetIOThreadTaskRunner({})) {
  DCHECK(!profile_path_.empty());
  
  // Create cache manager
  base::FilePath cache_dir = profile_path_.Append("nsite_cache");
  cache_manager_ = std::make_unique<NsiteCacheManager>(cache_dir);
  
  // Create rate limiter (60 requests per minute per client)
  rate_limiter_ = std::make_unique<NsiteSecurityUtils::RateLimiter>(60);
}

NsiteStreamingServer::~NsiteStreamingServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

uint16_t NsiteStreamingServer::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (IsRunning()) {
    LOG(WARNING) << "Nsite streaming server already running on port " << port_;
    return port_;
  }

  uint16_t allocated_port = AllocatePort();
  if (allocated_port == 0) {
    LOG(ERROR) << "Failed to allocate port for nsite streaming server";
    return 0;
  }

  port_ = allocated_port;
  LOG(INFO) << "Nsite streaming server started on port " << port_;
  return port_;
}

void NsiteStreamingServer::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!IsRunning()) {
    return;
  }

  server_.reset();
  server_socket_.reset();
  port_ = 0;
  
  LOG(INFO) << "Nsite streaming server stopped";
}

uint16_t NsiteStreamingServer::AllocatePort() {
  // Try random allocation first
  uint16_t port = TryRandomPorts();
  if (port != 0) {
    return port;
  }

  // Fall back to sequential scan
  return TrySequentialPorts();
}

uint16_t NsiteStreamingServer::TryRandomPorts() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint16_t> dist(kMinEphemeralPort, kMaxEphemeralPort);

  for (int i = 0; i < kMaxRandomAttempts; ++i) {
    uint16_t port = dist(gen);
    if (!IsPortBlacklisted(port) && TryBindPort(port)) {
      return port;
    }
  }

  return 0;
}

uint16_t NsiteStreamingServer::TrySequentialPorts() {
  // Start from a random offset to avoid always picking the same port
  uint16_t offset = base::RandInt(0, kMaxEphemeralPort - kMinEphemeralPort);
  
  for (int i = 0; i < kMaxSequentialAttempts; ++i) {
    uint16_t port = kMinEphemeralPort + ((offset + i) % (kMaxEphemeralPort - kMinEphemeralPort + 1));
    if (!IsPortBlacklisted(port) && TryBindPort(port)) {
      return port;
    }
  }

  return 0;
}

bool NsiteStreamingServer::IsPortBlacklisted(uint16_t port) {
  return kBlacklistedPorts.find(port) != kBlacklistedPorts.end();
}

bool NsiteStreamingServer::TryBindPort(uint16_t port) {
  auto socket = std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());
  
  int result = socket->Listen(net::IPEndPoint(net::IPAddress::IPv4Localhost(), port), 5);
  if (result != net::OK) {
    return false;
  }

  // Successfully bound, create the HTTP server
  server_socket_ = std::move(socket);
  server_ = std::make_unique<net::HttpServer>(std::move(socket), this);
  
  return true;
}

void NsiteStreamingServer::OnConnect(int connection_id) {
  // Connection established
}

void NsiteStreamingServer::OnHttpRequest(int connection_id,
                                        const net::HttpServerRequestInfo& info) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  
  // Get client identifier for rate limiting
  std::string client_id = info.peer.address().ToString() + ":" + 
                         std::to_string(info.peer.port());
  
  // Apply rate limiting
  if (!rate_limiter_->IsAllowed(client_id)) {
    LOG(WARNING) << "Rate limit exceeded for client: " << client_id;
    SendErrorResponse(connection_id, net::HTTP_TOO_MANY_REQUESTS, 
                     NsiteSecurityUtils::GetSafeErrorMessage(429));
    return;
  }
  
  // Log request details (sanitized)
  std::string sanitized_path = NsiteSecurityUtils::SanitizeInput(info.path, 256);
  LOG(INFO) << "Request: " << info.method << " " << sanitized_path 
            << " (connection: " << connection_id << ")";
  
  auto context = ParseNsiteRequest(info);
  if (!context.valid) {
    LOG(WARNING) << "Invalid request - no valid npub found";
    SendErrorResponse(connection_id, net::HTTP_BAD_REQUEST, 
                     NsiteSecurityUtils::GetSafeErrorMessage(400));
    return;
  }

  HandleNsiteRequest(connection_id, context);
}

void NsiteStreamingServer::OnWebSocketRequest(int connection_id,
                                             const net::HttpServerRequestInfo& info) {
  // WebSocket not supported for nsite streaming
  server_->Send404(connection_id);
}

void NsiteStreamingServer::OnWebSocketMessage(int connection_id, 
                                             std::string_view data) {
  // Should not be called
}

void NsiteStreamingServer::OnClose(int connection_id) {
  // Connection closed
}

NsiteStreamingServer::RequestContext NsiteStreamingServer::ParseNsiteRequest(
    const net::HttpServerRequestInfo& info) {
  RequestContext context;
  
  // Set HTTP method
  context.method = info.method;
  
  // Get or create session
  context.session_id = GetOrCreateSession(info);
  
  // Extract X-Npub header (case-insensitive)
  std::string npub;
  for (const auto& header : info.headers) {
    if (base::EqualsCaseInsensitiveASCII(header.first, "X-Npub")) {
      npub = header.second;
      break;
    }
  }

  // If no header, try session fallback
  if (npub.empty() && !context.session_id.empty()) {
    npub = GetNpubFromSession(context.session_id);
    if (!npub.empty()) {
      context.from_session = true;
      LOG(INFO) << "Using npub from session: " << npub;
    }
  }

  if (npub.empty()) {
    LOG(WARNING) << "Request missing X-Npub header and no session: " << info.path;
    return context;
  }

  // Validate npub format using security utils
  if (!NsiteSecurityUtils::IsValidNpub(npub)) {
    LOG(WARNING) << "Invalid npub format: " << NsiteSecurityUtils::SanitizeInput(npub, 64);
    return context;
  }

  // Update session with this npub
  if (!context.session_id.empty()) {
    UpdateSession(context.session_id, npub);
  }

  context.npub = npub;
  
  // Validate and sanitize path
  if (!NsiteSecurityUtils::IsPathSafe(info.path)) {
    LOG(WARNING) << "Unsafe path detected: " << NsiteSecurityUtils::SanitizeInput(info.path, 256);
    return context;
  }
  
  context.path = NsiteSecurityUtils::SanitizePath(info.path);
  context.valid = true;
  
  // Remove leading slash from path
  if (!context.path.empty() && context.path[0] == '/') {
    context.path = context.path.substr(1);
  }

  LOG(INFO) << "Nsite request: " << npub << " -> " << context.path 
            << " (session: " << context.session_id << ")";
  
  return context;
}

void NsiteStreamingServer::HandleNsiteRequest(int connection_id,
                                             const RequestContext& context) {
  // Handle dismiss notification endpoint
  if (context.path == kDismissEndpoint) {
    // Only accept POST requests for dismiss endpoint
    if (context.method != "POST") {
      SendErrorResponse(connection_id, net::HTTP_METHOD_NOT_ALLOWED, 
                       "Method not allowed - use POST");
      return;
    }
    
    if (notification_manager_) {
      notification_manager_->DismissNotification(context.npub);
    }
    
    // Send simple success response
    scoped_refptr<net::HttpResponseHeaders> headers =
        net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n\r\n");
    headers->AddHeader("Content-Type", "text/plain");
    server_->SendResponse(connection_id, headers, "dismissed");
    return;
  }
  
  // Try to get file from cache first
  auto cached_file = cache_manager_->GetFile(context.npub, context.path);
  
  if (cached_file) {
    // Serve from cache
    scoped_refptr<net::HttpResponseHeaders> headers =
        net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n\r\n");
    headers->AddHeader("Content-Type", cached_file->content_type);
    headers->AddHeader("Cache-Control", "public, max-age=3600");  // 1 hour
    headers->AddHeader("ETag", cached_file->hash);
    
    // Set session cookie if we have a session
    if (!context.session_id.empty()) {
      headers->AddHeader("Set-Cookie", 
          base::StringPrintf("nsite_session=%s; Path=/; HttpOnly; SameSite=Strict",
                            context.session_id.c_str()));
    }
    
    server_->SendResponse(connection_id, headers, cached_file->content);
    
    // Trigger background update check after serving cached content
    if (update_monitor_) {
      update_monitor_->CheckForUpdates(
          context.npub, 
          context.path,
          base::BindRepeating([](const std::string& npub, const std::string& path) {
            VLOG(1) << "Update available for nsite: " << npub << " path: " << path;
          }));
    }
    
    return;
  }
  
  // File not in cache - serve placeholder for now
  // TODO: Implement background fetching from Nostr relays
  
  std::string response = base::StringPrintf(
      R"(<!DOCTYPE html>
<html>
<head>
<title>Nsite: %s</title>
<style>
body { font-family: system-ui, sans-serif; margin: 40px; }
h1 { color: #0366d6; }
p { color: #586069; }
.info { background: #f6f8fa; padding: 20px; border-radius: 6px; }
.error { background: #ffebee; border-left: 4px solid #f44336; padding: 20px; margin: 20px 0; }
code { background: #e1e4e8; padding: 2px 4px; border-radius: 3px; }
</style>
</head>
<body>
<h1>Nsite Streaming Server</h1>
<div class="info">
<p><strong>Nsite:</strong> <code>%s</code></p>
<p><strong>Path:</strong> <code>%s</code></p>
<p><strong>Session:</strong> <code>%s</code></p>
<p><strong>From Session:</strong> %s</p>
</div>
<div class="error">
<p><strong>File not found in cache</strong></p>
<p>Nsite content fetching from relays will be implemented in a future update.</p>
</div>
</body>
</html>)",
      context.npub.c_str(),
      context.npub.c_str(),
      context.path.c_str(),
      context.session_id.c_str(),
      context.from_session ? "Yes" : "No");

  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 404 Not Found\r\n\r\n");
  headers->AddHeader("Content-Type", "text/html; charset=utf-8");
  headers->AddHeader("Cache-Control", "no-cache");
  
  // Set session cookie if we have a session
  if (!context.session_id.empty()) {
    headers->AddHeader("Set-Cookie", 
        base::StringPrintf("nsite_session=%s; Path=/; HttpOnly; SameSite=Strict",
                          context.session_id.c_str()));
  }
  
  server_->SendResponse(connection_id, headers, response);
}

void NsiteStreamingServer::SendErrorResponse(int connection_id,
                                            int status_code,
                                            const std::string& message) {
  std::string body = base::StringPrintf(kErrorTemplate, 
                                       status_code, status_code, message.c_str());
  
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate(
          base::StringPrintf("HTTP/1.1 %d %s\r\n\r\n",
                           status_code,
                           net::GetHttpReasonPhrase(status_code)));
  headers->AddHeader("Content-Type", "text/html; charset=utf-8");
  
  server_->SendResponse(connection_id, headers, body);
}

std::string NsiteStreamingServer::GetOrCreateSession(
    const net::HttpServerRequestInfo& info) {
  // Look for session cookie
  auto cookie_it = info.headers.find("cookie");
  if (cookie_it != info.headers.end()) {
    // Parse cookies
    std::vector<std::pair<std::string, std::string>> cookies =
        net::cookie_util::ParseRequestCookieLine(cookie_it->second);
    
    for (const auto& cookie : cookies) {
      if (cookie.first == "nsite_session") {
        // Validate session ID format
        if (!NsiteSecurityUtils::IsValidSessionId(cookie.second)) {
          LOG(WARNING) << "Invalid session ID format";
          break;
        }
        
        // Validate session exists
        if (sessions_.find(cookie.second) != sessions_.end()) {
          return cookie.second;
        }
        break;
      }
    }
  }
  
  // Create new session
  std::string session_id = base::GenerateGUID();
  sessions_[session_id] = "";  // Empty npub initially
  return session_id;
}

void NsiteStreamingServer::UpdateSession(const std::string& session_id,
                                        const std::string& npub) {
  sessions_[session_id] = npub;
}

std::string NsiteStreamingServer::GetNpubFromSession(
    const std::string& session_id) {
  auto it = sessions_.find(session_id);
  if (it != sessions_.end()) {
    return it->second;
  }
  return "";
}

void NsiteStreamingServer::SetUpdateMonitor(NsiteUpdateMonitor* update_monitor) {
  update_monitor_ = update_monitor;
}

void NsiteStreamingServer::SetNotificationManager(NsiteNotificationManager* notification_manager) {
  notification_manager_ = notification_manager;
}

void NsiteStreamingServer::CacheFile(const std::string& npub,
                                    const std::string& path,
                                    const std::string& content,
                                    const std::string& content_type) {
  cache_manager_->PutFile(npub, path, content, content_type);
}

}  // namespace nostr