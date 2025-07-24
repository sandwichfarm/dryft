// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_streaming_server.h"

#include <algorithm>
#include <random>
#include <set>

#include "chrome/browser/nostr/nsite/nsite_cache_manager.h"
#include "chrome/browser/nostr/nsite/nsite_metrics.h"
#include "chrome/browser/nostr/nsite/nsite_notification_manager.h"
#include "chrome/browser/nostr/nsite/nsite_security_utils.h"
#include "chrome/browser/nostr/nsite/nsite_update_monitor.h"
#include "components/blossom/blossom_content_resolver.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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

  SCOPED_NSITE_TIMER(kServerStartup);

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
  SCOPED_NSITE_TIMER(kRequestProcessing);
  
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
    
    // Add security headers
    AddSecurityHeaders(headers.get(), context);
    
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
  
  // File not in cache - query from local relay using WebSocket
  // Create WebSocket connection to local relay (ws://127.0.0.1:8081)
  QueryEventsFromRelay(connection_id, context);
}

void NsiteStreamingServer::QueryEventsFromRelay(int connection_id,
                                               const RequestContext& context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  // For now, send 404 since WebSocket implementation would be complex
  // In a full implementation, this would:
  // 1. Create WebSocket connection to ws://127.0.0.1:8081
  // 2. Send REQ message with filter for kind 34128 and author
  // 3. Handle EOSE and EVENT responses
  // 4. Parse events and find matching file
  
  LOG(INFO) << "Relay querying not yet implemented - serving 404";
  SendErrorResponse(connection_id, net::HTTP_NOT_FOUND, "Content not available");
}

void NsiteStreamingServer::OnRelayEventsReceived(int connection_id,
                                                const RequestContext& context,
                                                const std::vector<base::Value::Dict>& events) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (events.empty()) {
    LOG(INFO) << "No nsite events found for npub: " << context.npub;
    SendErrorResponse(connection_id, net::HTTP_NOT_FOUND, "Nsite not found");
    return;
  }
  
  // Find event with matching path in 'd' tag
  std::string target_path = context.path.empty() ? "index.html" : context.path;
  std::string matched_hash;
  std::string content_type = "text/html"; // Default
  
  for (const auto& event : events) {
    // Get tags array from event
    const base::Value::List* tags = event.FindList("tags");
    if (!tags) continue;
    
    std::string file_path;
    std::string file_hash;
    
    // Parse tags to find 'd' (path) and 'x' (hash)
    for (const auto& tag_value : *tags) {
      const base::Value::List* tag = tag_value.GetIfList();
      if (!tag || tag->size() < 2) continue;
      
      const std::string* tag_name = (*tag)[0].GetIfString();
      const std::string* tag_content = (*tag)[1].GetIfString();
      if (!tag_name || !tag_content) continue;
      
      if (*tag_name == "d") {
        file_path = *tag_content;
      } else if (*tag_name == "x") {
        file_hash = *tag_content;
      } else if (*tag_name == "m" && tag->size() >= 2) {
        content_type = *tag_content; // MIME type
      }
    }
    
    // Check for exact path match
    if (file_path == target_path && !file_hash.empty()) {
      matched_hash = file_hash;
      break;
    }
    
    // Check for directory index if path is empty or ends with '/'
    if ((target_path == "index.html" || target_path.empty()) && 
        file_path == "index.html" && !file_hash.empty()) {
      matched_hash = file_hash;
      break;
    }
  }
  
  if (matched_hash.empty()) {
    LOG(INFO) << "No matching file found for path: " << target_path;
    SendErrorResponse(connection_id, net::HTTP_NOT_FOUND, "File not found");
    return;
  }
  
  // Resolve content via Blossom
  if (!blossom_resolver_) {
    LOG(WARNING) << "Blossom resolver not available";
    SendErrorResponse(connection_id, net::HTTP_INTERNAL_SERVER_ERROR, 
                     "Content resolver unavailable");
    return;
  }
  
  blossom_resolver_->ResolveContent(
      context.npub, matched_hash,
      base::BindOnce(&NsiteStreamingServer::OnFileContentResolved,
                     weak_factory_.GetWeakPtr(),
                     connection_id, context));
}

void NsiteStreamingServer::OnFileContentResolved(int connection_id,
                                                const RequestContext& context,
                                                const blossom::ContentResolutionResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (result.status != blossom::ContentResolutionResult::SUCCESS) {
    LOG(WARNING) << "Failed to resolve content: " << result.error_message;
    SendErrorResponse(connection_id, net::HTTP_NOT_FOUND, "Content not available");
    return;
  }
  
  // Successfully resolved content - serve it
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n\r\n");
  
  // Set content type
  if (!result.mime_type.empty()) {
    headers->AddHeader("Content-Type", result.mime_type);
  } else {
    headers->AddHeader("Content-Type", "application/octet-stream");
  }
  
  // Add caching headers
  headers->AddHeader("Cache-Control", "public, max-age=3600"); // 1 hour
  
  // Add security headers
  AddSecurityHeaders(headers.get(), context);
  
  // Set session cookie if we have a session
  if (!context.session_id.empty()) {
    headers->AddHeader("Set-Cookie", 
        base::StringPrintf("nsite_session=%s; Path=/; HttpOnly; SameSite=Strict",
                          context.session_id.c_str()));
  }
  
  server_->SendResponse(connection_id, headers, result.content);
  
  // Cache the content for future requests
  cache_manager_->PutFile(context.npub, context.path, 
                         result.content, result.mime_type);
  
  // Trigger background update check
  if (update_monitor_) {
    update_monitor_->CheckForUpdates(
        context.npub, 
        context.path,
        base::BindRepeating([](const std::string& npub, const std::string& path) {
          VLOG(1) << "Update available for nsite: " << npub << " path: " << path;
        }));
  }
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
  
  // Add basic security headers even for error responses
  headers->AddHeader("X-Frame-Options", "DENY");
  headers->AddHeader("X-Content-Type-Options", "nosniff");
  headers->AddHeader("Referrer-Policy", "no-referrer");
  
  server_->SendResponse(connection_id, headers, body);
}

void NsiteStreamingServer::AddSecurityHeaders(net::HttpResponseHeaders* headers,
                                              const RequestContext& context) {
  // Prevent clickjacking attacks
  headers->AddHeader("X-Frame-Options", "DENY");
  
  // Prevent MIME type sniffing
  headers->AddHeader("X-Content-Type-Options", "nosniff");
  
  // Control referrer information
  headers->AddHeader("Referrer-Policy", "no-referrer");
  
  // Restrict browser features/APIs
  headers->AddHeader("Permissions-Policy", 
      "camera=(), "
      "microphone=(), "
      "geolocation=(), "
      "payment=(), "
      "usb=(), "
      "bluetooth=(), "
      "accelerometer=(), "
      "gyroscope=(), "
      "magnetometer=()");
  
  // Add Cross-Origin policies for better isolation
  headers->AddHeader("Cross-Origin-Opener-Policy", "same-origin");
  headers->AddHeader("Cross-Origin-Embedder-Policy", "require-corp");
  headers->AddHeader("Cross-Origin-Resource-Policy", "same-origin");
  
  // Add X-Npub header for origin identification
  headers->AddHeader("X-Npub", context.npub);
  
  // Add strict Content Security Policy
  // This CSP removes unsafe-inline and unsafe-eval as required by G-6 security audit
  headers->AddHeader("Content-Security-Policy",
      "default-src 'self' https:; "
      "script-src 'self' https: 'wasm-unsafe-eval'; "  // Allow WASM but no unsafe-inline/eval
      "style-src 'self' https: 'unsafe-inline'; "  // Allow inline styles for now (can be tightened later)
      "img-src 'self' https: data: blob:; "
      "font-src 'self' https: data:; "
      "connect-src 'self' https: wss:; "
      "media-src 'self' https: blob:; "
      "object-src 'none'; "
      "frame-src 'none'; "
      "frame-ancestors 'none'; "
      "base-uri 'self'; "
      "form-action 'self'; "
      "upgrade-insecure-requests;");
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

void NsiteStreamingServer::SetBlossomResolver(blossom::BlossomContentResolver* resolver) {
  blossom_resolver_ = resolver;
}

void NsiteStreamingServer::CacheFile(const std::string& npub,
                                    const std::string& path,
                                    const std::string& content,
                                    const std::string& content_type) {
  cache_manager_->PutFile(npub, path, content, content_type);
}

}  // namespace nostr