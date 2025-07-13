// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_streaming_server.h"

#include <algorithm>
#include <random>
#include <set>

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
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/socket/tcp_server_socket.h"

namespace nostr {

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
  
  auto context = ParseNsiteRequest(info);
  if (!context.valid) {
    SendErrorResponse(connection_id, net::HTTP_BAD_REQUEST, 
                     "Missing or invalid X-Nsite-Pubkey header");
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
  
  // Extract X-Nsite-Pubkey header (case-insensitive)
  std::string npub;
  for (const auto& header : info.headers) {
    if (base::EqualsCaseInsensitiveASCII(header.first, "X-Nsite-Pubkey")) {
      npub = header.second;
      break;
    }
  }

  if (npub.empty()) {
    LOG(WARNING) << "Request missing X-Nsite-Pubkey header: " << info.path;
    return context;
  }

  // Validate npub format (basic check)
  if (!base::StartsWith(npub, "npub1") || npub.length() < 10) {
    LOG(WARNING) << "Invalid npub format: " << npub;
    return context;
  }

  context.npub = npub;
  context.path = info.path;
  context.valid = true;
  
  // Remove leading slash from path
  if (!context.path.empty() && context.path[0] == '/') {
    context.path = context.path.substr(1);
  }

  LOG(INFO) << "Nsite request: " << npub << " -> " << context.path;
  
  return context;
}

void NsiteStreamingServer::HandleNsiteRequest(int connection_id,
                                             const RequestContext& context) {
  // TODO: Implement actual file serving from cache
  // For now, return a placeholder response
  
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
code { background: #e1e4e8; padding: 2px 4px; border-radius: 3px; }
</style>
</head>
<body>
<h1>Nsite Streaming Server</h1>
<div class="info">
<p><strong>Nsite:</strong> <code>%s</code></p>
<p><strong>Path:</strong> <code>%s</code></p>
<p><strong>Status:</strong> Cache implementation pending</p>
</div>
</body>
</html>)",
      context.npub.c_str(),
      context.npub.c_str(),
      context.path.c_str());

  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n\r\n");
  headers->AddHeader("Content-Type", "text/html; charset=utf-8");
  headers->AddHeader("Cache-Control", "no-cache");
  
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

}  // namespace nostr