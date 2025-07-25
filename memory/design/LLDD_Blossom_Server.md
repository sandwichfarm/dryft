# Low Level Design Document: Blossom Server
## dryft browser - Local Content-Addressed Storage Server

### 1. Component Overview

The Blossom Server component implements a local content-addressed storage server following the Blossom protocol specification (BUD-00, BUD-01, BUD-03). It provides blob storage where blobs are raw binary data addressed by their SHA256 hash. The server caches media content, reduces bandwidth usage, and improves content loading performance while supporting Nostr authorization events.

### 2. Detailed Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   HTTP Server Layer                      │
│  ┌─────────────────────────────────────────────────┐   │
│  │             HTTP Request Handler                  │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ GET Handler  │  │ PUT Handler       │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ HEAD Handler │  │ OPTIONS Handler   │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│              Authorization Layer (Kind 24242)            │
│  ┌─────────────────────────────────────────────────┐   │
│  │         Nostr Authorization Validation            │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ Event Verify │  │ Permission Check  │       │   │
│  │  │              │  │ (t tags)          │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│               Content Validation Layer                   │
│  ┌─────────────────────────────────────────────────┐   │
│  │           Hash Verification Engine                │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ SHA256 Calc  │  │ MIME Type         │       │   │
│  │  │              │  │ Detection         │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                  Storage Layer                           │
│  ┌─────────────────────────────────────────────────┐   │
│  │          Content-Addressed Storage                │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ File System  │  │ Metadata DB       │       │   │
│  │  │ Storage      │  │                   │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                  Cache Management Layer                  │
│  ┌─────────────────────────────────────────────────┐   │
│  │              LRU Cache Manager                    │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ Eviction     │  │ Usage Tracking    │       │   │
│  │  │ Policy       │  │                   │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│             External Blossom Sync Layer                  │
│  ┌─────────────────────────────────────────────────┐   │
│  │           Remote Blossom Client                   │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ Fetch Client │  │ Upload Client     │       │   │
│  │  │              │  │                   │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### 3. HTTP Server Implementation

```cpp
// services/blossom_service.cc
class BlossomService : public service_manager::Service {
 public:
  BlossomService();
  
  void Start(uint16_t port = 8080);
  void Stop();
  
 private:
  class HttpHandler : public net::HttpServer::Delegate {
   public:
    // HttpServer::Delegate implementation
    void OnConnect(int connection_id) override;
    void OnHttpRequest(int connection_id,
                      const net::HttpServerRequestInfo& info) override;
    void OnWebSocketRequest(int connection_id,
                           const net::HttpServerRequestInfo& info) override;
    void OnWebSocketMessage(int connection_id,
                           const std::string& data) override;
    void OnClose(int connection_id) override;
    
   private:
    void HandleGet(int connection_id,
                  const net::HttpServerRequestInfo& info);
    void HandleHead(int connection_id,
                   const net::HttpServerRequestInfo& info);
    void HandleOptions(int connection_id,
                      const net::HttpServerRequestInfo& info);
    // Note: PUT /upload, DELETE /{sha256}, and GET /list 
    // are defined in BUD-03 but not implemented in this local cache
    
    ContentStorage* storage_;
    AuthManager* auth_manager_;
  };
  
  std::unique_ptr<net::HttpServer> http_server_;
  std::unique_ptr<HttpHandler> http_handler_;
  std::unique_ptr<ContentStorage> storage_;
};

// GET /{sha256} - retrieve blob by hash (BUD-01)
void HttpHandler::HandleGet(int connection_id,
                          const net::HttpServerRequestInfo& info) {
  // Extract hash from URL path (e.g., /abc123...)
  // Support optional file extension per BUD-01
  std::string path = info.path;
  std::string hash;
  std::string extension;
  
  // Extract hash (last 64 hex chars) and optional extension
  size_t dot_pos = path.find_last_of('.');
  if (dot_pos != std::string::npos && dot_pos > 64) {
    hash = path.substr(1, 64);  // Skip leading /
    extension = path.substr(dot_pos);
  } else if (path.length() == 65) {  // /{64-char-hash}
    hash = path.substr(1);
  } else {
    SendErrorResponse(connection_id, 400, "Invalid hash format");
    return;
  }
  
  if (!IsValidHash(hash)) {
    SendErrorResponse(connection_id, 400, "Invalid hash format");
    return;
  }
  
  // Check authorization if required
  if (auth_manager_->RequiresAuth("get")) {
    if (!CheckAuthorization(info.headers, "get")) {
      net::HttpServerResponseInfo response(net::HTTP_UNAUTHORIZED);
      response.AddHeader("WWW-Authenticate", "Nostr");
      response.AddHeader("X-Reason", "Authorization required for GET");
      response.AddHeader("Access-Control-Allow-Origin", "*");
      http_server_->SendResponse(connection_id, response);
      return;
    }
  }
  
  // Retrieve content
  auto content = storage_->GetContent(hash);
  if (!content) {
    // Try to fetch from remote Blossom servers
    FetchFromRemote(hash, connection_id);
    return;
  }
  
  // Send response with appropriate headers
  net::HttpServerResponseInfo response(net::HTTP_OK);
  response.AddHeader("Content-Type", content->mime_type);
  response.AddHeader("Content-Length", 
                    base::NumberToString(content->data.size()));
  response.AddHeader("Cache-Control", "public, max-age=31536000");
  response.AddHeader("Access-Control-Allow-Origin", "*");
  response.AddHeader("Accept-Ranges", "bytes");  // BUD-01 requirement
  
  // Handle Range requests (BUD-01)
  auto range_header = info.headers.find("Range");
  if (range_header != info.headers.end()) {
    // Parse range header and send partial content
    SendPartialContent(connection_id, content, range_header->second);
    return;
  }
  
  http_server_->SendResponse(connection_id, response);
  http_server_->SendRaw(connection_id, 
                       std::string(content->data.begin(), 
                                  content->data.end()));
}

// HEAD /{sha256} - check if blob exists (BUD-01)
void HttpHandler::HandleHead(int connection_id,
                           const net::HttpServerRequestInfo& info) {
  // Extract hash from URL path
  std::string path = info.path;
  std::string hash;
  
  // Support optional file extension
  size_t dot_pos = path.find_last_of('.');
  if (dot_pos != std::string::npos && dot_pos > 64) {
    hash = path.substr(1, 64);
  } else if (path.length() == 65) {
    hash = path.substr(1);
  } else {
    SendErrorResponse(connection_id, 400, "Invalid hash format");
    return;
  }
  
  if (!IsValidHash(hash)) {
    SendErrorResponse(connection_id, 400, "Invalid hash format");
    return;
  }
  
  // Check authorization if required
  if (auth_manager_->RequiresAuth("get")) {
    if (!CheckAuthorization(info.headers, "get")) {
      net::HttpServerResponseInfo response(net::HTTP_UNAUTHORIZED);
      response.AddHeader("WWW-Authenticate", "Nostr");
      response.AddHeader("X-Reason", "Authorization required");
      response.AddHeader("Access-Control-Allow-Origin", "*");
      http_server_->SendResponse(connection_id, response);
      return;
    }
  }
  
  // Check if content exists
  auto metadata = storage_->GetContentMetadata(hash);
  if (!metadata) {
    SendErrorResponse(connection_id, 404, "Blob not found");
    return;
  }
  
  // Send response headers only (no body for HEAD)
  net::HttpServerResponseInfo response(net::HTTP_OK);
  response.AddHeader("Content-Type", metadata->mime_type);
  response.AddHeader("Content-Length", 
                    base::NumberToString(metadata->size));
  response.AddHeader("Access-Control-Allow-Origin", "*");
  response.AddHeader("Accept-Ranges", "bytes");
  
  http_server_->SendResponse(connection_id, response);
}

// OPTIONS - handle CORS preflight requests (BUD-01)
void HttpHandler::HandleOptions(int connection_id,
                              const net::HttpServerRequestInfo& info) {
  net::HttpServerResponseInfo response(net::HTTP_OK);
  
  // BUD-01 required CORS headers
  response.AddHeader("Access-Control-Allow-Origin", "*");
  response.AddHeader("Access-Control-Allow-Methods", "GET,HEAD,OPTIONS");
  response.AddHeader("Access-Control-Allow-Headers", 
                    "Content-Type,X-Reason,X-Auth-*");
  response.AddHeader("Access-Control-Max-Age", "86400");  // 1 day
  
  http_server_->SendResponse(connection_id, response);
}
```

### 4. Content Storage Implementation

```cpp
// Content storage with file system backend
class ContentStorage {
 public:
  struct ContentMetadata {
    std::string hash;
    std::string mime_type;
    size_t size;
    base::Time created_at;
    base::Time last_accessed;
    int access_count;
  };
  
  struct Content {
    ContentMetadata metadata;
    std::vector<uint8_t> data;
  };
  
  ContentStorage(const base::FilePath& storage_path);
  
  std::unique_ptr<Content> GetContent(const std::string& hash);
  bool StoreContent(const std::string& hash,
                   const std::vector<uint8_t>& data,
                   const std::string& mime_type);
  bool DeleteContent(const std::string& hash);
  std::vector<ContentMetadata> ListContent();
  size_t GetTotalSize();
  
 private:
  base::FilePath GetContentPath(const std::string& hash);
  void UpdateAccessTime(const std::string& hash);
  bool SaveMetadata(const ContentMetadata& metadata);
  std::unique_ptr<ContentMetadata> LoadMetadata(const std::string& hash);
  
  base::FilePath storage_path_;
  std::unique_ptr<sql::Database> metadata_db_;
  mutable base::Lock storage_lock_;
};

// File system storage implementation
std::unique_ptr<ContentStorage::Content> ContentStorage::GetContent(
    const std::string& hash) {
  base::AutoLock lock(storage_lock_);
  
  // Load metadata
  auto metadata = LoadMetadata(hash);
  if (!metadata) {
    return nullptr;
  }
  
  // Read file content
  base::FilePath file_path = GetContentPath(hash);
  std::string file_data;
  if (!base::ReadFileToString(file_path, &file_data)) {
    return nullptr;
  }
  
  // Update access time
  UpdateAccessTime(hash);
  
  // Construct result
  auto content = std::make_unique<Content>();
  content->metadata = *metadata;
  content->data.assign(file_data.begin(), file_data.end());
  
  return content;
}

bool ContentStorage::StoreContent(const std::string& hash,
                                const std::vector<uint8_t>& data,
                                const std::string& mime_type) {
  base::AutoLock lock(storage_lock_);
  
  // Check if content already exists
  if (LoadMetadata(hash)) {
    return true;  // Already stored
  }
  
  // Ensure storage directory exists
  base::FilePath file_path = GetContentPath(hash);
  base::FilePath dir_path = file_path.DirName();
  if (!base::CreateDirectory(dir_path)) {
    return false;
  }
  
  // Write file
  if (!base::WriteFile(file_path, 
                      reinterpret_cast<const char*>(data.data()),
                      data.size())) {
    return false;
  }
  
  // Save metadata
  ContentMetadata metadata;
  metadata.hash = hash;
  metadata.mime_type = mime_type;
  metadata.size = data.size();
  metadata.created_at = base::Time::Now();
  metadata.last_accessed = metadata.created_at;
  metadata.access_count = 0;
  
  return SaveMetadata(metadata);
}

base::FilePath ContentStorage::GetContentPath(const std::string& hash) {
  // Use first 2 chars for directory sharding
  std::string dir1 = hash.substr(0, 2);
  std::string dir2 = hash.substr(2, 2);
  
  return storage_path_.Append(dir1).Append(dir2).Append(hash);
}
```

### 5. Hash Verification Implementation

```cpp
class HashVerifier {
 public:
  static std::string CalculateSHA256(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> hash(32);
    SHA256(data.data(), data.size(), hash.data());
    return HexEncode(hash);
  }
  
  static bool VerifyContent(const std::vector<uint8_t>& data,
                          const std::string& expected_hash) {
    std::string actual_hash = CalculateSHA256(data);
    return actual_hash == expected_hash;
  }
  
  static bool IsValidHash(const std::string& hash) {
    // SHA256 hash should be 64 hex characters
    if (hash.length() != 64) {
      return false;
    }
    
    // Check all characters are valid hex
    for (char c : hash) {
      if (!std::isxdigit(c)) {
        return false;
      }
    }
    
    return true;
  }
};

// MIME type detection
class MimeTypeDetector {
 public:
  static std::string DetectMimeType(const std::vector<uint8_t>& data) {
    if (data.size() < 4) {
      return "application/octet-stream";
    }
    
    // Check magic bytes
    if (IsPNG(data)) return "image/png";
    if (IsJPEG(data)) return "image/jpeg";
    if (IsGIF(data)) return "image/gif";
    if (IsWebP(data)) return "image/webp";
    if (IsMP4(data)) return "video/mp4";
    if (IsWebM(data)) return "video/webm";
    if (IsPDF(data)) return "application/pdf";
    
    // Text detection
    if (IsLikelyText(data)) {
      if (IsJSON(data)) return "application/json";
      if (IsHTML(data)) return "text/html";
      return "text/plain";
    }
    
    return "application/octet-stream";
  }
  
 private:
  static bool IsPNG(const std::vector<uint8_t>& data) {
    const uint8_t png_sig[] = {0x89, 0x50, 0x4E, 0x47, 
                               0x0D, 0x0A, 0x1A, 0x0A};
    return data.size() >= 8 && 
           std::equal(png_sig, png_sig + 8, data.begin());
  }
  
  static bool IsJPEG(const std::vector<uint8_t>& data) {
    return data.size() >= 3 && 
           data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF;
  }
  
  static bool IsGIF(const std::vector<uint8_t>& data) {
    return data.size() >= 6 &&
           ((data[0] == 'G' && data[1] == 'I' && data[2] == 'F' &&
             data[3] == '8' && data[4] == '7' && data[5] == 'a') ||
            (data[0] == 'G' && data[1] == 'I' && data[2] == 'F' &&
             data[3] == '8' && data[4] == '9' && data[5] == 'a'));
  }
};
```

### 6. Cache Management Implementation

```cpp
class CacheManager {
 public:
  struct CacheConfig {
    size_t max_cache_size = 1024 * 1024 * 1024;  // 1GB default
    size_t min_free_space = 100 * 1024 * 1024;   // 100MB
    double eviction_threshold = 0.9;  // Start eviction at 90% full
    int max_age_days = 30;
  };
  
  CacheManager(ContentStorage* storage, const CacheConfig& config);
  
  void EnforceLimits();
  void EvictLRU(size_t target_size);
  void EvictOldContent();
  
 private:
  struct CacheEntry {
    std::string hash;
    size_t size;
    base::Time last_accessed;
    int access_count;
  };
  
  std::vector<CacheEntry> GetEvictionCandidates();
  double CalculateEvictionScore(const CacheEntry& entry);
  
  ContentStorage* storage_;
  CacheConfig config_;
  base::RepeatingTimer cleanup_timer_;
};

// LRU eviction implementation
void CacheManager::EvictLRU(size_t target_size) {
  size_t current_size = storage_->GetTotalSize();
  if (current_size <= target_size) {
    return;
  }
  
  // Get all entries sorted by eviction score
  auto candidates = GetEvictionCandidates();
  std::sort(candidates.begin(), candidates.end(),
           [this](const CacheEntry& a, const CacheEntry& b) {
             return CalculateEvictionScore(a) > 
                    CalculateEvictionScore(b);
           });
  
  // Evict until under target size
  size_t evicted_size = 0;
  for (const auto& entry : candidates) {
    if (current_size - evicted_size <= target_size) {
      break;
    }
    
    if (storage_->DeleteContent(entry.hash)) {
      evicted_size += entry.size;
      LOG(INFO) << "Evicted content: " << entry.hash 
                << " (size: " << entry.size << ")";
    }
  }
}

double CacheManager::CalculateEvictionScore(const CacheEntry& entry) {
  // Higher score = more likely to evict
  double score = 0.0;
  
  // Factor 1: Age (older = higher score)
  base::TimeDelta age = base::Time::Now() - entry.last_accessed;
  score += age.InHours() / 24.0;  // Days since last access
  
  // Factor 2: Size (larger = higher score)
  score += static_cast<double>(entry.size) / (1024 * 1024);  // MB
  
  // Factor 3: Access frequency (less accessed = higher score)
  if (entry.access_count > 0) {
    score /= std::log2(entry.access_count + 1);
  }
  
  return score;
}
```

### 7. BUD-03 User Server List Integration

```cpp
// Manages user's Blossom server list from kind 10063 events
class UserServerListManager {
 public:
  struct ServerEntry {
    std::string url;
    int priority;  // Order in the list (0 = most trusted)
  };
  
  // Fetch user's server list from relays
  void FetchUserServerList(const std::string& pubkey,
                          ServerListCallback callback);
  
  // Parse kind 10063 event
  std::vector<ServerEntry> ParseServerListEvent(
      const NostrEvent& event);
  
 private:
  NostrRelayClient* relay_client_;
};

// Parse kind 10063 server list event (BUD-03)
std::vector<UserServerListManager::ServerEntry> 
UserServerListManager::ParseServerListEvent(const NostrEvent& event) {
  std::vector<ServerEntry> servers;
  
  if (event.kind != 10063) {
    return servers;
  }
  
  // Extract server tags in order (most trusted first)
  int priority = 0;
  for (const auto& tag : event.tags) {
    if (tag.size() >= 2 && tag[0] == "server") {
      ServerEntry entry;
      entry.url = tag[1];
      entry.priority = priority++;
      servers.push_back(entry);
    }
  }
  
  return servers;
}

// Client for fetching from remote Blossom servers
class RemoteBlossomClient {
 public:
  // Fetch content using user's server list
  void FetchContentFromUserServers(
      const std::string& hash,
      const std::string& user_pubkey,
      FetchCallback callback);
  
  // Extract SHA256 hash from URL (BUD-03 spec)
  static std::string ExtractHashFromURL(const std::string& url);
  
 private:
  void TryNextServer(const std::string& hash,
                    const std::vector<std::string>& servers,
                    size_t server_index,
                    FetchCallback callback);
  
  UserServerListManager* server_list_manager_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
};

// Extract hash from URL per BUD-03 specification
std::string RemoteBlossomClient::ExtractHashFromURL(
    const std::string& url) {
  // Find the last 64 character hex string in the URL
  std::regex hash_regex("[0-9a-f]{64}", std::regex::icase);
  std::smatch matches;
  std::string last_hash;
  
  std::string::const_iterator search_start = url.begin();
  while (std::regex_search(search_start, url.end(), matches, hash_regex)) {
    last_hash = matches[0];
    search_start = matches.suffix().first;
  }
  
  return last_hash;
}

// Fetch content using user's server list (BUD-03)
void RemoteBlossomClient::FetchContentFromUserServers(
    const std::string& hash,
    const std::string& user_pubkey,
    FetchCallback callback) {
  // First, fetch the user's server list
  server_list_manager_->FetchUserServerList(
      user_pubkey,
      base::BindOnce(&RemoteBlossomClient::OnServerListFetched,
                    weak_factory_.GetWeakPtr(),
                    hash,
                    std::move(callback)));
}

void RemoteBlossomClient::OnServerListFetched(
    const std::string& hash,
    FetchCallback callback,
    bool success,
    const std::vector<std::string>& server_urls) {
  if (!success || server_urls.empty()) {
    // Fallback to popular servers if no user list
    std::vector<std::string> fallback_servers = {
      "https://blossom.primal.net",
      "https://cdn.satellite.earth",
      "https://files.v0l.io"
    };
    TryNextServer(hash, fallback_servers, 0, std::move(callback));
    return;
  }
  
  // Try servers in order of trust (first = most trusted)
  TryNextServer(hash, server_urls, 0, std::move(callback));
}

void RemoteBlossomClient::TryNextServer(
    const std::string& hash,
    const std::vector<std::string>& servers,
    size_t server_index,
    FetchCallback callback) {
  if (server_index >= servers.size()) {
    LOG(WARNING) << "Failed to fetch blob from any server";
    std::move(callback).Run(false, nullptr);
    return;
  }
  
  const std::string& server_url = servers[server_index];
  
  // Build request URL - server URL should be root endpoint
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(server_url + "/" + hash);
  request->method = "GET";
  
  // Add CORS headers
  request->cors_mode = network::mojom::CorsMode::kCors;
  
  // Create loader
  loader_ = network::SimpleURLLoader::Create(
      std::move(request), TRAFFIC_ANNOTATION);
  
  // Fetch content
  loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&RemoteBlossomClient::OnContentFetched,
                    weak_factory_.GetWeakPtr(),
                    hash,
                    server_index,
                    std::move(callback)),
      kMaxContentSize);
}

void RemoteBlossomClient::OnContentFetched(
    const std::string& hash,
    size_t server_index,
    FetchCallback callback,
    std::unique_ptr<std::string> response_body) {
  if (!response_body || loader_->NetError() != net::OK) {
    // Try next server
    TryNextServer(hash, server_index + 1, std::move(callback));
    return;
  }
  
  // Verify hash
  std::vector<uint8_t> data(response_body->begin(), response_body->end());
  if (!HashVerifier::VerifyContent(data, hash)) {
    LOG(WARNING) << "Hash mismatch from server: " 
                 << servers_[server_index].url;
    TryNextServer(hash, server_index + 1, std::move(callback));
    return;
  }
  
  // Success
  auto content = std::make_unique<Content>();
  content->data = std::move(data);
  content->hash = hash;
  
  std::move(callback).Run(true, std::move(content));
}
```

### 8. Authorization and Access Control

```cpp
// BUD-01 Authorization Implementation
class AuthManager {
 public:
  struct AuthConfig {
    bool require_auth_for_get = false;
    bool require_auth_for_upload = true;
    bool require_auth_for_list = false;
    bool require_auth_for_delete = true;
  };
  
  bool CheckAuthorization(const net::HttpRequestHeaders& headers,
                         const std::string& verb);
  bool RequiresAuth(const std::string& verb);
  
 private:
  bool ValidateAuthEvent(const NostrEvent& event,
                        const std::string& required_verb);
  
  AuthConfig config_;
};

// Check authorization header for kind 24242 event
bool AuthManager::CheckAuthorization(
    const net::HttpRequestHeaders& headers,
    const std::string& verb) {
  // Look for Authorization header
  auto auth_header = headers.find("Authorization");
  if (auth_header == headers.end()) {
    return false;
  }
  
  // Expected format: "Nostr <base64-encoded-event>"
  if (!base::StartsWith(auth_header->second, "Nostr ")) {
    return false;
  }
  
  std::string encoded_event = auth_header->second.substr(6);
  std::string decoded;
  if (!base::Base64Decode(encoded_event, &decoded)) {
    return false;
  }
  
  // Parse Nostr event
  NostrEvent auth_event;
  if (!ParseNostrEvent(decoded, &auth_event)) {
    return false;
  }
  
  return ValidateAuthEvent(auth_event, verb);
}

// Validate kind 24242 authorization event per BUD-01
bool AuthManager::ValidateAuthEvent(const NostrEvent& event,
                                  const std::string& required_verb) {
  // 1. Event kind MUST be 24242
  if (event.kind != 24242) {
    LOG(WARNING) << "Invalid auth event kind: " << event.kind;
    return false;
  }
  
  // 2. Event created_at MUST be within reasonable time (~60 seconds)
  int64_t now = base::Time::Now().ToTimeT();
  if (std::abs(now - event.created_at) > 60) {
    LOG(WARNING) << "Auth event too old or in future";
    return false;
  }
  
  // 3. Event MUST have a 't' tag matching the request
  bool has_matching_verb = false;
  int64_t expiration = 0;
  
  for (const auto& tag : event.tags) {
    if (tag.size() >= 2) {
      if (tag[0] == "t" && tag[1] == required_verb) {
        has_matching_verb = true;
      }
      if (tag[0] == "expiration" && tag.size() >= 2) {
        expiration = std::stoll(tag[1]);
      }
    }
  }
  
  if (!has_matching_verb) {
    LOG(WARNING) << "No matching 't' tag for verb: " << required_verb;
    return false;
  }
  
  // 4. Event MUST have valid future expiration timestamp
  if (expiration == 0 || expiration <= now) {
    LOG(WARNING) << "Invalid or expired expiration tag";
    return false;
  }
  
  // 5. Verify event signature
  if (!VerifyNostrEventSignature(event)) {
    LOG(WARNING) << "Invalid event signature";
    return false;
  }
  
  // 6. Content should be human readable (just log it)
  LOG(INFO) << "Auth event content: " << event.content;
  
  return true;
}
```

### 9. HTTP Response Handling

```cpp
class ResponseBuilder {
 public:
  static void SendErrorResponse(net::HttpServer* server,
                              int connection_id,
                              int status_code,
                              const std::string& message) {
    net::HttpServerResponseInfo response(
        static_cast<net::HttpStatusCode>(status_code));
    
    std::string body = base::StringPrintf(
        "{\"error\":\"%s\",\"code\":%d}",
        message.c_str(), status_code);
    
    response.SetBody(body, "application/json");
    response.AddHeader("Access-Control-Allow-Origin", "*");
    
    server->SendResponse(connection_id, response);
  }
  
  static void SendListResponse(net::HttpServer* server,
                             int connection_id,
                             const std::vector<ContentMetadata>& items) {
    rapidjson::Document doc;
    doc.SetArray();
    
    for (const auto& item : items) {
      rapidjson::Value obj(rapidjson::kObjectType);
      obj.AddMember("hash", rapidjson::Value(item.hash.c_str(), 
                                           doc.GetAllocator()),
                   doc.GetAllocator());
      obj.AddMember("size", static_cast<int64_t>(item.size), 
                   doc.GetAllocator());
      obj.AddMember("type", rapidjson::Value(item.mime_type.c_str(),
                                           doc.GetAllocator()),
                   doc.GetAllocator());
      obj.AddMember("created", item.created_at.ToTimeT(), 
                   doc.GetAllocator());
      doc.PushBack(obj, doc.GetAllocator());
    }
    
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    
    net::HttpServerResponseInfo response(net::HTTP_OK);
    response.SetBody(buffer.GetString(), "application/json");
    response.AddHeader("Access-Control-Allow-Origin", "*");
    
    server->SendResponse(connection_id, response);
  }
};
```

### 10. Performance Optimizations

```cpp
// Streaming upload/download for large files
class StreamingHandler {
 public:
  void HandleStreamingUpload(int connection_id,
                           const std::string& initial_data) {
    auto context = std::make_unique<UploadContext>();
    context->hasher = std::make_unique<SHA256_CTX>();
    SHA256_Init(context->hasher.get());
    
    // Process initial chunk
    ProcessUploadChunk(context.get(), initial_data);
    
    // Store context for connection
    upload_contexts_[connection_id] = std::move(context);
  }
  
  void HandleUploadChunk(int connection_id,
                        const std::string& data) {
    auto it = upload_contexts_.find(connection_id);
    if (it == upload_contexts_.end()) {
      return;
    }
    
    ProcessUploadChunk(it->second.get(), data);
  }
  
  void FinalizeUpload(int connection_id) {
    auto it = upload_contexts_.find(connection_id);
    if (it == upload_contexts_.end()) {
      return;
    }
    
    auto* context = it->second.get();
    
    // Finalize hash
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, context->hasher.get());
    std::string hash_hex = HexEncode(hash, SHA256_DIGEST_LENGTH);
    
    // Save content
    storage_->StoreContent(hash_hex, context->data, 
                          context->mime_type);
    
    // Clean up
    upload_contexts_.erase(it);
  }
  
 private:
  struct UploadContext {
    std::unique_ptr<SHA256_CTX> hasher;
    std::vector<uint8_t> data;
    std::string mime_type;
    size_t total_size = 0;
  };
  
  void ProcessUploadChunk(UploadContext* context,
                         const std::string& chunk) {
    // Update hash
    SHA256_Update(context->hasher.get(), 
                 chunk.data(), chunk.size());
    
    // Append data
    context->data.insert(context->data.end(),
                        chunk.begin(), chunk.end());
    
    // Detect MIME type from first chunk
    if (context->mime_type.empty() && context->data.size() >= 512) {
      context->mime_type = MimeTypeDetector::DetectMimeType(
          context->data);
    }
  }
  
  std::map<int, std::unique_ptr<UploadContext>> upload_contexts_;
};
```

### 11. Testing Strategy

```cpp
// Unit tests
TEST(BlossomHashTest, CalculatesCorrectSHA256) {
  std::vector<uint8_t> data = {'t', 'e', 's', 't'};
  std::string hash = HashVerifier::CalculateSHA256(data);
  
  // Expected hash for "test"
  EXPECT_EQ(hash, 
    "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08");
}

TEST(MimeTypeTest, DetectsCommonTypes) {
  // PNG test
  std::vector<uint8_t> png_data = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
  };
  EXPECT_EQ(MimeTypeDetector::DetectMimeType(png_data), "image/png");
  
  // JPEG test
  std::vector<uint8_t> jpeg_data = {0xFF, 0xD8, 0xFF};
  EXPECT_EQ(MimeTypeDetector::DetectMimeType(jpeg_data), "image/jpeg");
}

// Integration tests
TEST(BlossomIntegrationTest, StoreAndRetrieve) {
  BlossomService service;
  service.Start(8080);
  
  // Upload content
  auto client = CreateHttpClient("http://localhost:8080");
  std::vector<uint8_t> test_data = {'h', 'e', 'l', 'l', 'o'};
  
  auto upload_response = client->Put("/upload", test_data);
  EXPECT_EQ(upload_response.status_code, 200);
  
  // Extract hash from response
  rapidjson::Document doc;
  doc.Parse(upload_response.body.c_str());
  std::string hash = doc["hash"].GetString();
  
  // Retrieve content
  auto get_response = client->Get("/" + hash);
  EXPECT_EQ(get_response.status_code, 200);
  EXPECT_EQ(get_response.body, "hello");
}
```

### 12. Configuration Management

```cpp
class BlossomConfig {
 public:
  struct Config {
    bool enabled = true;
    uint16_t port = 8080;
    std::string bind_address = "127.0.0.1";
    
    // Storage settings
    base::FilePath storage_path;
    size_t max_cache_size = 1024 * 1024 * 1024;  // 1GB
    size_t max_file_size = 100 * 1024 * 1024;    // 100MB
    
    // Auth settings
    bool require_auth = false;
    std::vector<std::string> api_keys;
    
    // Remote servers
    std::vector<std::string> remote_servers;
    
    // Performance
    bool enable_compression = true;
    int worker_threads = 4;
  };
  
  static Config Load();
  static void Save(const Config& config);
  
  static base::FilePath GetDefaultStoragePath() {
    return base::GetHomeDir()
        .Append(".tungsten")
        .Append("blossom");
  }
};
```

### 13. Monitoring and Metrics

```cpp
class BlossomMetrics {
 public:
  void RecordUpload(size_t size, base::TimeDelta duration);
  void RecordDownload(size_t size, base::TimeDelta duration);
  void RecordCacheHit(const std::string& hash);
  void RecordCacheMiss(const std::string& hash);
  
  struct Stats {
    int64_t total_uploads;
    int64_t total_downloads;
    int64_t total_bytes_uploaded;
    int64_t total_bytes_downloaded;
    double cache_hit_rate;
    size_t cache_size;
    double average_upload_speed_mbps;
    double average_download_speed_mbps;
  };
  
  Stats GetStats() const;
  
 private:
  std::atomic<int64_t> upload_count_{0};
  std::atomic<int64_t> download_count_{0};
  std::atomic<int64_t> bytes_uploaded_{0};
  std::atomic<int64_t> bytes_downloaded_{0};
  std::atomic<int64_t> cache_hits_{0};
  std::atomic<int64_t> cache_misses_{0};
  
  base::MovingAverage upload_speed_;
  base::MovingAverage download_speed_;
};
```