# Low Level Design Document: Nsite Support
## Tungsten Browser - Static Website Specification Implementation

### 1. Component Overview

The Nsite Support component implements the Pubkey Static Websites specification (NIP-XX) to enable decentralized static websites hosted on Nostr. It uses kind 34128 events for static files with absolute paths and SHA256 hashes, resolves content from user Blossom servers, and renders websites in a secure sandboxed environment.

### 2. Detailed Architecture

```
┌─────────────────────────────────────────────────────────┐
│                  Nsite URL Handler                       │
│  ┌─────────────────────────────────────────────────┐   │
│  │          Pubkey Resolution Handler              │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ NIP-05 Lookup│  │ <npub> Subdomain  │       │   │
│  │  │              │  │ Parser            │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│            Static File Event Resolver                    │
│  ┌─────────────────────────────────────────────────┐   │
│  │           Event Fetcher (Kind 34128)             │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ NIP-65 Relay │  │ Path Matching     │       │   │
│  │  │ List         │  │ (d tag)           │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                File Resolution Layer                     │
│  ┌─────────────────────────────────────────────────┐   │
│  │             File Content Fetcher                  │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ BUD-03 Server│  │ Hash Verification │       │   │
│  │  │ List Fetch   │  │ (x tag)           │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                 Website Assembly Layer                   │
│  ┌─────────────────────────────────────────────────┐   │
│  │              Site Builder                         │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ File Cache   │  │ Route Handler     │       │   │
│  │  │ Manager      │  │                   │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│               Sandboxed Rendering Layer                  │
│  ┌─────────────────────────────────────────────────┐   │
│  │            Isolated WebContents                   │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ CSP Enforcer │  │ Resource Loader   │       │   │
│  │  │              │  │                   │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### 3. Static Website Event Structure

```cpp
// Static file event (kind: 34128) - NIP-XX spec
struct StaticFileEvent {
  static constexpr int kKind = 34128;
  
  std::string author_pubkey;
  std::string d_tag;  // Absolute path from root (e.g., "/index.html")
  std::string file_hash;  // SHA256 hash from 'x' tag
  int64_t created_at;
  
  // Extract from tags
  static StaticFileEvent FromNostrEvent(const NostrEvent& event) {
    StaticFileEvent file;
    file.author_pubkey = event.pubkey;
    file.created_at = event.created_at;
    
    for (const auto& tag : event.tags) {
      if (tag.size() >= 2) {
        if (tag[0] == "d") {
          file.d_tag = tag[1];  // Absolute path
        } else if (tag[0] == "x") {
          file.file_hash = tag[1];  // SHA256 hash
        }
      }
    }
    
    return file;
  }
};
```

### 4. Class Definitions

```cpp
// browser/static_website_handler.cc
class StaticWebsiteHandler {
 public:
  StaticWebsiteHandler();
  
  // Handle requests based on pubkey resolution
  void HandleStaticWebsiteRequest(
      const std::string& pubkey,
      const std::string& path,
      content::WebContents* web_contents);
  
 private:
  // Resolve pubkey from various sources
  std::string ResolvePubkeyFromSubdomain(const std::string& subdomain);
  std::string ResolvePubkeyFromNIP05(const std::string& nip05_id);
  
  std::unique_ptr<StaticFileResolver> file_resolver_;
  std::unique_ptr<StaticWebsiteRenderer> renderer_;
};

// browser/static_file_resolver.cc  
class StaticFileResolver {
 public:
  using FileCallback = base::OnceCallback<void(
      bool success,
      std::unique_ptr<FileContent> content)>;
  
  // Resolve file for path using kind 34128 events
  void ResolveFile(const std::string& author_pubkey,
                  const std::string& path,
                  FileCallback callback);
  
 private:
  void FetchUserRelayList(const std::string& pubkey);
  void QueryRelaysForFile(const std::string& pubkey,
                         const std::string& path,
                         FileCallback callback);
  void FetchFromBlossomServers(const std::string& hash,
                              const std::string& user_pubkey,
                              FileCallback callback);
  
  std::unique_ptr<RelayConnectionManager> relay_manager_;
  std::unique_ptr<BlossomClient> blossom_client_;
};

// Pubkey resolution methods
std::string StaticWebsiteHandler::ResolvePubkeyFromSubdomain(
    const std::string& subdomain) {
  // Extract npub from subdomain format: <npub>.<domain>
  if (base::StartsWith(subdomain, "npub") && 
      subdomain.length() == 63) {  // npub1 + 58 chars
    // Decode npub to hex pubkey
    return DecodeNpub(subdomain);
  }
  return "";
}

// browser/static_website_renderer.cc
class StaticWebsiteRenderer {
 public:
  void RenderStaticWebsite(
      const std::string& pubkey,
      const std::string& initial_path,
      content::WebContents* web_contents);
  
 private:
  class StaticResourceLoader : public content::URLLoaderFactory {
   public:
    void CreateLoaderAndStart(
        network::mojom::URLLoaderRequest loader,
        int32_t routing_id,
        int32_t request_id,
        uint32_t options,
        const network::ResourceRequest& request,
        network::mojom::URLLoaderClientPtr client,
        const net::MutableNetworkTrafficAnnotationTag& annotation) override;
    
   private:
    std::string pubkey_;
    StaticFileResolver* file_resolver_;
    std::map<std::string, FileContent> file_cache_;
  };
  
  void SetupSandbox(content::WebContents* web_contents);
  void ApplySecurityPolicy(content::WebContents* web_contents);
  
  std::unique_ptr<StaticResourceLoader> resource_loader_;
};
```

### 5. File Resolution Implementation

```cpp
void StaticFileResolver::ResolveFile(
    const std::string& author_pubkey,
    const std::string& path,
    FileCallback callback) {
  // Normalize path - ensure it starts with /
  std::string normalized_path = path;
  if (!base::StartsWith(path, "/")) {
    normalized_path = "/" + path;
  }
  
  // Check if this is a directory request
  if (normalized_path == "/" || 
      base::EndsWith(normalized_path, "/")) {
    // Default to index.html for directories
    normalized_path += "index.html";
  }
  
  // First fetch user's NIP-65 relay list
  FetchUserRelayList(author_pubkey);
  
  // Query relays for kind 34128 event with matching d tag
  QueryRelaysForFile(author_pubkey, normalized_path, 
                    std::move(callback));
}

void StaticFileResolver::QueryRelaysForFile(
    const std::string& pubkey,
    const std::string& path,
    FileCallback callback) {
  // Build filter for kind 34128 events
  NostrFilter filter;
  filter.authors.push_back(pubkey);
  filter.kinds.push_back(34128);
  filter.tags["d"] = {path};  // Match d tag to path
  
  relay_manager_->QueryRelays(
      filter,
      base::BindOnce(&StaticFileResolver::OnFileEventReceived,
                    weak_factory_.GetWeakPtr(),
                    path,
                    std::move(callback)));
}

void StaticFileResolver::OnFileEventReceived(
    const std::string& requested_path,
    FileCallback callback,
    bool success,
    std::vector<NostrEvent> events) {
  if (!success || events.empty()) {
    // Try /404.html as fallback
    if (requested_path != "/404.html") {
      ResolveFile(current_pubkey_, "/404.html", 
                 std::move(callback));
      return;
    }
    std::move(callback).Run(false, nullptr);
    return;
  }
  
  // Parse the static file event
  const NostrEvent& event = events[0];
  StaticFileEvent file_event = StaticFileEvent::FromNostrEvent(event);
  
  if (file_event.file_hash.empty()) {
    LOG(WARNING) << "No x tag (hash) found in kind 34128 event";
    std::move(callback).Run(false, nullptr);
    return;
  }
  
  // Fetch file content from user's Blossom servers
  FetchFromBlossomServers(file_event.file_hash, 
                         event.pubkey,
                         std::move(callback));
}

void StaticFileResolver::FetchFromBlossomServers(
    const std::string& hash,
    const std::string& user_pubkey,
    FileCallback callback) {
  // Use BUD-03 to get user's server list
  blossom_client_->FetchContentFromUserServers(
      hash,
      user_pubkey,
      base::BindOnce(&StaticFileResolver::OnBlossomFetchComplete,
                    weak_factory_.GetWeakPtr(),
                    hash,
                    std::move(callback)));
}

void StaticFileResolver::OnBlossomFetchComplete(
    const std::string& expected_hash,
    FileCallback callback,
    bool success,
    std::unique_ptr<BlossomContent> content) {
  if (!success || !content) {
    LOG(WARNING) << "Failed to fetch content from Blossom servers";
    std::move(callback).Run(false, nullptr);
    return;
  }
  
  // Verify SHA256 hash matches x tag
  if (!VerifyHash(content->data, expected_hash)) {
    LOG(ERROR) << "Hash mismatch for fetched content";
    std::move(callback).Run(false, nullptr);
    return;
  }
  
  // Create file content
  auto file_content = std::make_unique<FileContent>();
  file_content->data = std::move(content->data);
  file_content->hash = expected_hash;
  file_content->mime_type = DetectMimeType(file_content->data);
  
  std::move(callback).Run(true, std::move(file_content));
}
```

### 6. File Content Structure

```cpp
struct FileContent {
  std::vector<uint8_t> data;
  std::string hash;
  std::string mime_type;
};

bool StaticFileResolver::VerifyHash(
    const std::vector<uint8_t>& data,
    const std::string& expected_hash) {
  std::vector<uint8_t> calculated_hash(32);
  SHA256(data.data(), data.size(), calculated_hash.data());
  
  std::string calculated_hex = HexEncode(calculated_hash);
  return calculated_hex == expected_hash;
}
  std::move(callback).Run(true, std::move(content));
}
```

### 7. Sandboxed Rendering Implementation

```cpp
void StaticWebsiteRenderer::RenderStaticWebsite(
    const std::string& pubkey,
    const std::string& initial_path,
    content::WebContents* web_contents) {
  // Setup sandbox
  SetupSandbox(web_contents);
  ApplySecurityPolicy(web_contents);
  
  // Create custom URL loader factory
  resource_loader_ = std::make_unique<StaticResourceLoader>(
      pubkey, file_resolver_.get());
  
  // Register custom scheme for this pubkey
  // Using subdomain format for origin isolation
  std::string npub = EncodeNpub(pubkey);
  std::string origin = base::StringPrintf(
      "https://%s.npub-sites.localhost",
      npub.c_str());
  
  // Navigate to requested path or index.html
  std::string path = initial_path.empty() ? "/index.html" : initial_path;
  web_contents->GetController().LoadURL(
      GURL(origin + path),
      content::Referrer(),
      ui::PAGE_TRANSITION_TYPED,
      std::string());
}

void StaticWebsiteRenderer::SetupSandbox(
    content::WebContents* web_contents) {
  // Disable features that could leak user data
  web_contents->GetMutableRendererPrefs()->
      javascript_can_access_clipboard = false;
  web_contents->GetMutableRendererPrefs()->
      javascript_can_open_windows_automatically = false;
  
  // Enable site isolation for security
  web_contents->GetMutableRendererPrefs()->
      site_per_process = true;
}

void StaticWebsiteRenderer::ApplySecurityPolicy(
    content::WebContents* web_contents) {
  // Apply strict Content Security Policy
  // No external connections allowed per spec
  const char kStaticWebsiteCSP[] = 
      "default-src 'self'; "
      "script-src 'self' 'unsafe-inline' 'unsafe-eval'; "
      "style-src 'self' 'unsafe-inline'; "
      "img-src 'self' data: blob:; "
      "font-src 'self' data:; "
      "connect-src 'none'; "  // No external connections
      "frame-src 'none'; "
      "object-src 'none'; "
      "media-src 'self'; "
      "worker-src 'self'; "
      "form-action 'none'; "
      "base-uri 'self'; "
      "frame-ancestors 'none';";
  
  web_contents->GetMutableRendererPrefs()->
      content_security_policy = kStaticWebsiteCSP;
}

// Custom URL loader implementation
void StaticResourceLoader::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& annotation) {
  // Extract path from request URL
  std::string path = request.url.path();
  
  // Check cache first
  auto cached_it = file_cache_.find(path);
  if (cached_it != file_cache_.end()) {
    SendFileResponse(std::move(client), cached_it->second);
    return;
  }
  
  // Fetch file from Nostr/Blossom
  file_resolver_->ResolveFile(
      pubkey_,
      path,
      base::BindOnce(&StaticResourceLoader::OnFileResolved,
                    weak_factory_.GetWeakPtr(),
                    path,
                    std::move(loader),
                    std::move(client)));
}

void StaticResourceLoader::OnFileResolved(
    const std::string& path,
    network::mojom::URLLoaderRequest loader,
    network::mojom::URLLoaderClientPtr client,
    bool success,
    std::unique_ptr<FileContent> content) {
  if (!success || !content) {
    Send404Response(std::move(client));
    return;
  }
  
  // Cache the file
  file_cache_[path] = *content;
  
  // Send response
  SendFileResponse(std::move(client), *content);
}

void StaticResourceLoader::SendFileResponse(
    network::mojom::URLLoaderClientPtr client,
    const FileContent& content) {
  // Create response headers
  auto response = network::mojom::URLResponseHead::New();
  response->mime_type = content.mime_type;
  response->content_length = content.data.size();
  response->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      "HTTP/1.1 200 OK");
  response->headers->AddHeader("Content-Type", content.mime_type);
  response->headers->AddHeader("Cache-Control", "public, max-age=3600");
  
  client->OnReceiveResponse(std::move(response));
  
  // Send body
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  mojo::CreateDataPipe(nullptr, &producer, &consumer);
  
  client->OnStartLoadingResponseBody(std::move(consumer));
  
  // Write data to pipe
  uint32_t num_bytes = content.data.size();
  producer->WriteData(content.data.data(), &num_bytes,
                     MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  
  client->OnComplete(network::URLLoaderCompletionStatus(net::OK));
}
```

### 9. Caching Strategy

```cpp
class NsiteCache {
 public:
  struct CacheEntry {
    std::unique_ptr<NsiteMetadataEvent> metadata;
    std::map<std::string, NsiteFileResolver::FileContent> files;
    base::Time last_access;
    size_t total_size;
  };
  
  void CacheNsite(const std::string& nsite_id,
                 const NsiteMetadataEvent& metadata) {
    auto& entry = cache_[nsite_id];
    entry.metadata = std::make_unique<NsiteMetadataEvent>(metadata);
    entry.last_access = base::Time::Now();
    
    EnforceSizeLimit();
  }
  
  void CacheFile(const std::string& nsite_id,
                const std::string& hash,
                const NsiteFileResolver::FileContent& content) {
    auto it = cache_.find(nsite_id);
    if (it == cache_.end()) return;
    
    it->second.files[hash] = content;
    it->second.total_size += content.data.size();
    it->second.last_access = base::Time::Now();
    
    EnforceSizeLimit();
  }
  
 private:
  void EnforceSizeLimit() {
    size_t total_size = 0;
    for (const auto& entry : cache_) {
      total_size += entry.second.total_size;
    }
    
    // Evict LRU entries if over limit (default 100MB)
    while (total_size > max_cache_size_) {
      auto oldest = std::min_element(
          cache_.begin(), cache_.end(),
          [](const auto& a, const auto& b) {
            return a.second.last_access < b.second.last_access;
          });
      
      total_size -= oldest->second.total_size;
      cache_.erase(oldest);
    }
  }
  
  std::map<std::string, CacheEntry> cache_;
  size_t max_cache_size_ = 100 * 1024 * 1024;  // 100MB
};
```

### 10. Error Handling

```cpp
class NsiteError {
 public:
  enum ErrorType {
    METADATA_NOT_FOUND,
    INVALID_METADATA,
    FILE_NOT_FOUND,
    HASH_MISMATCH,
    RELAY_ERROR,
    PARSE_ERROR,
  };
  
  static void ShowErrorPage(content::WebContents* web_contents,
                          ErrorType error,
                          const std::string& details) {
    std::string html = GenerateErrorHTML(error, details);
    
    web_contents->GetController().LoadDataWithBaseURL(
        GURL("data:text/html;charset=utf-8," + html),
        GURL("nsite://error"),
        html,
        "text/html");
  }
  
 private:
  static std::string GenerateErrorHTML(ErrorType error,
                                     const std::string& details) {
    return base::StringPrintf(R"(
      <!DOCTYPE html>
      <html>
      <head>
        <title>Nsite Error</title>
        <style>
          body { 
            font-family: system-ui; 
            max-width: 600px; 
            margin: 50px auto; 
            padding: 20px;
            background: #f5f5f5;
          }
          .error-box {
            background: white;
            border-radius: 8px;
            padding: 30px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
          }
          h1 { color: #e74c3c; }
          .details { 
            background: #f8f8f8; 
            padding: 15px; 
            border-radius: 4px;
            margin-top: 20px;
            font-family: monospace;
            font-size: 14px;
          }
        </style>
      </head>
      <body>
        <div class="error-box">
          <h1>Nsite Error</h1>
          <p>%s</p>
          <div class="details">%s</div>
        </div>
      </body>
      </html>
    )",
    GetErrorMessage(error).c_str(),
    details.c_str());
  }
};
```

### 11. Testing Strategy

```cpp
// Unit tests
TEST(NsiteMetadataParserTest, ParsesValidMetadata) {
  NostrEvent event;
  event.kind = 30503;
  event.content = R"({
    "name": "Test Site",
    "description": "A test nsite",
    "routes": [
      {
        "path": "/index.html",
        "type": "event",
        "hash": "abc123",
        "event_id": "def456"
      }
    ]
  })";
  event.tags = {{"d", "test-site"}};
  
  NsiteMetadataEvent metadata;
  EXPECT_TRUE(NsiteMetadataParser::ParseMetadataEvent(event, &metadata));
  EXPECT_EQ(metadata.metadata.name, "Test Site");
  EXPECT_EQ(metadata.metadata.routes.size(), 1);
}

TEST(NsiteFileResolverTest, VerifiesHashCorrectly) {
  NsiteFileResolver resolver;
  std::vector<uint8_t> data = {'t', 'e', 's', 't'};
  
  // Calculate expected hash
  std::vector<uint8_t> hash(32);
  SHA256(data.data(), data.size(), hash.data());
  std::string expected_hash = HexEncode(hash);
  
  EXPECT_TRUE(resolver.VerifyHash(data, expected_hash));
  EXPECT_FALSE(resolver.VerifyHash(data, "wronghash"));
}

// Integration tests
TEST(NsiteIntegrationTest, LoadsSimpleNsite) {
  TestBrowserContext context;
  NsiteHandler handler;
  
  // Create test nsite metadata
  NsiteMetadataEvent metadata;
  metadata.metadata.name = "Test Site";
  metadata.metadata.routes.push_back({
    "/index.html", "event", "testhash", "testevent", "100"
  });
  
  // Mock file content
  MockFileContent("<html><body>Hello Nsite!</body></html>");
  
  // Load nsite
  GURL url("nostr://nsite1testidentifier");
  handler.HandleNsiteURL(url, context.web_contents());
  
  // Verify content loaded
  EXPECT_EQ(context.web_contents()->GetTitle(), "Test Site");
}
```

### 12. Performance Optimizations

- **Parallel File Fetching**: Fetch multiple files concurrently
- **Progressive Loading**: Load critical files first (HTML, CSS)
- **Compression**: Support gzip/brotli for file content
- **Service Worker**: Enable offline functionality
- **Preloading**: Fetch linked resources proactively

### 13. Security Considerations

- **Strict CSP**: Prevent XSS and external resource loading
- **Origin Isolation**: Each nsite gets unique origin
- **No Cookie Access**: Nsites cannot access browser cookies
- **Sandboxed iframes**: Prevent frame busting
- **Resource Limits**: Cap memory and CPU usage per nsite
### 14. Key Implementation Differences from Previous Spec

This implementation follows the NIP-XX Pubkey Static Websites specification with these key differences:

1. **Event Structure**: Uses kind 34128 events with 'd' tag for absolute paths and 'x' tag for SHA256 hashes
2. **No Metadata Events**: No separate metadata events - files are directly mapped via kind 34128 events
3. **Blossom Integration**: Files are fetched from user's BUD-03 server list, not stored in events
4. **Path Resolution**: Direct path matching with d-tag, /index.html fallback for directories
5. **404 Handling**: Falls back to /404.html when requested path not found
6. **Pubkey Resolution**: Supports npub subdomains, NIP-05, DNS records
7. **Security**: Proxied requests to prevent IP leaks, strict CSP, no external connections
