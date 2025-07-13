# Low Level Design Document: Nsite Streaming Server
## Tungsten Browser - Local HTTP Server for Nsite Content Delivery

### 1. Executive Summary

The Nsite Streaming Server is a critical component that bridges the gap between Nostr's decentralized content and traditional browser navigation. It runs as a persistent HTTP server on a dynamically allocated port, serving nsite content with intelligent caching, background updates, and seamless integration with the browser's UI for update notifications.

### 2. Core Design Challenges and Solutions

#### 2.1 The Subdomain Problem
**Challenge**: Traditional web hosting uses subdomains for site isolation (e.g., `site1.example.com`, `site2.example.com`). However, localhost cannot have subdomains, making it impossible to use `npub1xxx.localhost` for different nsites.

**Solution**: Custom header injection:
- Browser injects `X-Nsite-Pubkey: <npub>` header with every request
- Server serves from root path: `http://localhost:8081/<resource-path>`
- Server reads the header to determine which nsite's files to serve
- Preserves root-relative URLs that nsites expect

#### 2.2 The Collision Problem
**Challenge**: Different nsites WILL have identical paths. Every nsite likely has:
- `/index.html` - their home page
- `/style.css` - their styles
- `/script.js` - their JavaScript
- `/about.html`, `/contact.html` - common page names

Without context, the server cannot know which nsite's `/index.html` to serve.

**Solution**: The `X-Nsite-Pubkey` header provides persistent context:
- Header must be injected on initial navigation to an nsite
- Header must persist across ALL subsequent requests (pages, resources, AJAX)
- Header only changes when navigating to a different nsite
- Creates virtual namespaces while serving from root

#### 2.3 The Browser Integration Problem
**Challenge**: Standard browser navigation doesn't include custom headers. When a user clicks a link from `/index.html` to `/about.html`, the browser won't automatically include the `X-Nsite-Pubkey` header.

**Solution**: Multi-layered approach to ensure header persistence:
1. **WebRequest Interceptor**: Intercepts all requests to localhost:8081 and injects the header based on the current nsite context
2. **Navigation State Tracker**: Maintains the current npub in browser memory, updates only when navigating to a different nsite
3. **Session Fallback**: Server maintains session mapping as backup when headers fail

#### 2.4 The Update Problem
**Challenge**: Nostr content can be updated at any time, but checking for updates on every request would be slow.

**Solution**: Dual-mode serving:
1. Serve cached content immediately
2. Check for updates in the background
3. Notify user via browser banner when updates are available

### 3. Detailed Architecture

```cpp
// Main server class
class NsiteStreamingServer : public net::HttpServer::Delegate {
 public:
  explicit NsiteStreamingServer(Profile* profile);
  ~NsiteStreamingServer() override;
  
  // Start the server on port 8081
  bool Start();
  void Stop();
  
  // HttpServer::Delegate implementation
  void OnHttpRequest(int connection_id,
                    const net::HttpServerRequestInfo& info) override;
  
 private:
  struct RequestContext {
    std::string npub;
    std::string resource_path;
    int connection_id;
    bool is_nsite_request;
  };
  
  // Request handling pipeline
  void HandleNsiteRequest(const RequestContext& context);
  void ServeFromCache(const RequestContext& context);
  void CheckForUpdates(const RequestContext& context);
  
  // Core components
  std::unique_ptr<net::HttpServer> http_server_;
  std::unique_ptr<NsiteCacheManager> cache_manager_;
  std::unique_ptr<NsiteUpdateMonitor> update_monitor_;
  std::unique_ptr<NsiteResolver> nsite_resolver_;
  
  // Browser integration
  Profile* profile_;
  base::WeakPtrFactory<NsiteStreamingServer> weak_factory_{this};
};
```

### 4. Request Parsing with Header Extraction

```cpp
NsiteStreamingServer::RequestContext ParseNsiteRequest(
    const net::HttpServerRequestInfo& info) {
  RequestContext context;
  context.resource_path = info.path;
  
  // Extract npub from custom header
  auto it = info.headers.find("X-Nsite-Pubkey");
  if (it == info.headers.end()) {
    // Try alternate header names for compatibility
    it = info.headers.find("x-nsite-pubkey");
    if (it == info.headers.end()) {
      // Check for session-based context as fallback
      context.npub = GetNpubFromSession(info);
      context.is_nsite_request = !context.npub.empty();
      return context;
    }
  }
  
  context.npub = it->second;
  
  // Validate npub format
  if (!IsValidNpub(context.npub)) {
    context.is_nsite_request = false;
    return context;
  }
  
  // Normalize path - ensure it starts with /
  if (context.resource_path.empty() || context.resource_path == "/") {
    context.resource_path = "/index.html";
  }
  
  context.is_nsite_request = true;
  return context;
}

void NsiteStreamingServer::OnHttpRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  // Parse the request with header extraction
  RequestContext context = ParseNsiteRequest(info);
  context.connection_id = connection_id;
  
  if (!context.is_nsite_request) {
    // Not an nsite request or missing header
    SendErrorResponse(connection_id, 
                     "Missing X-Nsite-Pubkey header");
    return;
  }
  
  // Log request for debugging
  VLOG(1) << "Nsite request: " << context.npub 
          << " path: " << context.resource_path;
  
  // Handle nsite request
  HandleNsiteRequest(context);
}
```

### 5. Cache Manager Implementation

```cpp
class NsiteCacheManager {
 public:
  struct CachedFile {
    std::vector<uint8_t> content;
    std::string content_hash;
    std::string mime_type;
    base::Time cached_at;
    base::Time last_accessed;
    int64_t event_created_at;  // Nostr event timestamp
  };
  
  struct NsiteCache {
    std::string npub;
    std::map<std::string, CachedFile> files;  // path -> file
    base::Time last_update_check;
    size_t total_size;
  };
  
  // Cache operations
  std::optional<CachedFile> GetFile(const std::string& npub,
                                   const std::string& path);
  
  void StoreFile(const std::string& npub,
                const std::string& path,
                const CachedFile& file);
  
  bool HasNsite(const std::string& npub);
  
  // Get all cached paths for an nsite
  std::vector<std::string> GetCachedPaths(const std::string& npub);
  
 private:
  // LRU cache with size limits
  void EnforceSizeLimit();
  void EvictLeastRecentlyUsed();
  
  std::map<std::string, NsiteCache> cache_;
  size_t max_cache_size_ = 500 * 1024 * 1024;  // 500MB
  size_t current_size_ = 0;
  
  // Persistence
  base::FilePath cache_dir_;
  void LoadFromDisk();
  void SaveToDisk();
};

std::optional<NsiteCacheManager::CachedFile> 
NsiteCacheManager::GetFile(const std::string& npub,
                          const std::string& path) {
  auto nsite_it = cache_.find(npub);
  if (nsite_it == cache_.end()) {
    return std::nullopt;
  }
  
  auto file_it = nsite_it->second.files.find(path);
  if (file_it == nsite_it->second.files.end()) {
    return std::nullopt;
  }
  
  // Update access time
  file_it->second.last_accessed = base::Time::Now();
  
  return file_it->second;
}
```

### 6. Request Handling Pipeline

```cpp
void NsiteStreamingServer::HandleNsiteRequest(
    const RequestContext& context) {
  // Step 1: Try to serve from cache
  ServeFromCache(context);
  
  // Step 2: Check for updates in background
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(&NsiteStreamingServer::CheckForUpdates,
                    weak_factory_.GetWeakPtr(),
                    context));
}

void NsiteStreamingServer::ServeFromCache(
    const RequestContext& context) {
  auto cached_file = cache_manager_->GetFile(
      context.npub, context.resource_path);
  
  if (cached_file.has_value()) {
    // Serve the cached file
    SendHttpResponse(context.connection_id,
                    net::HTTP_OK,
                    cached_file->mime_type,
                    cached_file->content);
  } else {
    // Not in cache, need to fetch
    FetchAndServe(context);
  }
}

void NsiteStreamingServer::FetchAndServe(
    const RequestContext& context) {
  // Decode npub to hex pubkey
  std::string pubkey = DecodeNpub(context.npub);
  
  // Fetch the file using NsiteResolver
  nsite_resolver_->ResolveFile(
      pubkey,
      context.resource_path,
      base::BindOnce(&NsiteStreamingServer::OnFileFetched,
                    weak_factory_.GetWeakPtr(),
                    context));
}

void NsiteStreamingServer::OnFileFetched(
    const RequestContext& context,
    bool success,
    std::unique_ptr<FileContent> content) {
  if (!success || !content) {
    // Try 404.html
    if (context.resource_path != "/404.html") {
      RequestContext not_found_context = context;
      not_found_context.resource_path = "/404.html";
      HandleNsiteRequest(not_found_context);
      return;
    }
    
    // No 404.html either, send generic 404
    Send404Response(context.connection_id);
    return;
  }
  
  // Cache the file
  NsiteCacheManager::CachedFile cached_file;
  cached_file.content = std::move(content->data);
  cached_file.content_hash = content->hash;
  cached_file.mime_type = content->mime_type;
  cached_file.cached_at = base::Time::Now();
  cached_file.event_created_at = content->event_timestamp;
  
  cache_manager_->StoreFile(context.npub,
                           context.resource_path,
                           cached_file);
  
  // Serve the file
  SendHttpResponse(context.connection_id,
                  net::HTTP_OK,
                  cached_file.mime_type,
                  cached_file.content);
}
```

### 7. Update Monitoring System

```cpp
class NsiteUpdateMonitor {
 public:
  struct UpdateInfo {
    std::string npub;
    std::string path;
    std::string new_hash;
    int64_t new_timestamp;
    bool update_available;
  };
  
  using UpdateCallback = base::OnceCallback<void(const UpdateInfo&)>;
  
  void CheckForUpdates(const std::string& npub,
                      const std::string& path,
                      int64_t cached_timestamp,
                      UpdateCallback callback);
  
  // Batch update checking for efficiency
  void CheckMultipleFiles(const std::string& npub,
                         const std::map<std::string, int64_t>& files,
                         UpdateCallback callback);
  
 private:
  // Rate limiting to prevent overwhelming relays
  bool ShouldCheckNow(const std::string& npub);
  void RecordCheck(const std::string& npub);
  
  std::map<std::string, base::Time> last_check_times_;
  base::TimeDelta min_check_interval_ = base::Minutes(5);
};

void NsiteStreamingServer::CheckForUpdates(
    const RequestContext& context) {
  // Get current cached file info
  auto cached_file = cache_manager_->GetFile(
      context.npub, context.resource_path);
  
  if (!cached_file.has_value()) {
    return;  // Nothing to update
  }
  
  // Check if we should rate limit
  if (!update_monitor_->ShouldCheckNow(context.npub)) {
    return;
  }
  
  // Check for updates
  update_monitor_->CheckForUpdates(
      context.npub,
      context.resource_path,
      cached_file->event_created_at,
      base::BindOnce(&NsiteStreamingServer::OnUpdateCheckComplete,
                    weak_factory_.GetWeakPtr(),
                    context));
}

void NsiteStreamingServer::OnUpdateCheckComplete(
    const RequestContext& context,
    const NsiteUpdateMonitor::UpdateInfo& update_info) {
  if (!update_info.update_available) {
    return;  // No update needed
  }
  
  // Download the updated file in background
  DownloadUpdate(context, update_info);
  
  // Notify the browser to show update banner
  NotifyBrowserOfUpdate(context.npub, context.resource_path);
}
```

### 8. Browser Integration for Update Notifications

```cpp
class NsiteUpdateNotifier {
 public:
  explicit NsiteUpdateNotifier(Profile* profile);
  
  // Notify browser of available update
  void NotifyUpdate(const std::string& npub,
                   const std::string& path);
  
  // Clear notification when user reloads
  void ClearNotification(const std::string& npub);
  
 private:
  // IPC to browser process
  void SendUpdateMessage(const std::string& npub,
                        const UpdateStatus& status);
  
  Profile* profile_;
  std::map<std::string, UpdateStatus> pending_updates_;
};

void NsiteStreamingServer::NotifyBrowserOfUpdate(
    const std::string& npub,
    const std::string& path) {
  // Get the tab that's viewing this nsite
  content::WebContents* web_contents = 
      FindWebContentsForNsite(npub);
  
  if (!web_contents) {
    return;  // No active tab viewing this nsite
  }
  
  // Inject JavaScript to show update banner
  std::string script = base::StringPrintf(R"(
    (function() {
      // Create or update the banner
      let banner = document.getElementById('nsite-update-banner');
      if (!banner) {
        banner = document.createElement('div');
        banner.id = 'nsite-update-banner';
        banner.style.cssText = `
          position: fixed;
          top: 0;
          left: 0;
          right: 0;
          background: #f0ad4e;
          color: #333;
          padding: 10px;
          text-align: center;
          z-index: 999999;
          font-family: system-ui, sans-serif;
          box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        `;
        
        banner.innerHTML = `
          <span>A new version of this nsite is available.</span>
          <button onclick="location.reload()" style="
            margin-left: 10px;
            padding: 5px 15px;
            background: #333;
            color: white;
            border: none;
            border-radius: 3px;
            cursor: pointer;
          ">Reload</button>
          <button onclick="this.parentElement.remove()" style="
            margin-left: 5px;
            padding: 5px 10px;
            background: transparent;
            border: 1px solid #333;
            border-radius: 3px;
            cursor: pointer;
          ">Dismiss</button>
        `;
        
        document.body.appendChild(banner);
      }
      
      // Auto-hide after 30 seconds
      setTimeout(() => {
        if (banner && banner.parentElement) {
          banner.remove();
        }
      }, 30000);
    })();
  )", npub.c_str());
  
  web_contents->GetMainFrame()->ExecuteJavaScript(
      base::UTF8ToUTF16(script),
      base::NullCallback());
}
```

### 9. HTTP Response Handling

```cpp
void NsiteStreamingServer::SendHttpResponse(
    int connection_id,
    net::HttpStatusCode status_code,
    const std::string& mime_type,
    const std::vector<uint8_t>& content) {
  // Build response headers
  std::string headers = base::StringPrintf(
      "HTTP/1.1 %d %s\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %zu\r\n"
      "Cache-Control: public, max-age=3600\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "\r\n",
      status_code,
      net::GetHttpReasonPhrase(status_code),
      mime_type.c_str(),
      content.size());
  
  // Send headers
  http_server_->SendRaw(connection_id, headers);
  
  // Send body
  if (!content.empty()) {
    http_server_->SendRaw(
        connection_id,
        std::string(content.begin(), content.end()));
  }
  
  // Close connection
  http_server_->Close(connection_id);
}

void NsiteStreamingServer::Send404Response(int connection_id) {
  const char kNotFoundHTML[] = R"(
    <!DOCTYPE html>
    <html>
    <head>
      <title>404 - Nsite Not Found</title>
      <style>
        body {
          font-family: system-ui, -apple-system, sans-serif;
          display: flex;
          align-items: center;
          justify-content: center;
          height: 100vh;
          margin: 0;
          background: #f5f5f5;
        }
        .error-container {
          text-align: center;
          padding: 40px;
          background: white;
          border-radius: 8px;
          box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 { color: #e74c3c; margin-bottom: 10px; }
        p { color: #666; }
      </style>
    </head>
    <body>
      <div class="error-container">
        <h1>404</h1>
        <p>Nsite not found or resource unavailable</p>
      </div>
    </body>
    </html>
  )";
  
  SendHttpResponse(connection_id,
                  net::HTTP_NOT_FOUND,
                  "text/html",
                  std::vector<uint8_t>(kNotFoundHTML,
                                      kNotFoundHTML + strlen(kNotFoundHTML)));
}
```

### 10. Dynamic Port Management and Server Lifecycle

```cpp
class NsiteStreamingServer : public net::HttpServer::Delegate {
 public:
  // Port range for dynamic allocation
  static constexpr int kMinPort = 49152;  // Start of dynamic/private port range
  static constexpr int kMaxPort = 65535;  // End of port range
  
  // Ports to avoid (common development servers)
  static const std::set<int> kAvoidPorts;
  
  // Get the current server port (0 if not running)
  int GetPort() const { return current_port_; }
  
  // Start server with dynamic port allocation
  bool Start();
  
 private:
  int current_port_ = 0;
  bool TryBindToPort(int port);
};

// Define ports to avoid
const std::set<int> NsiteStreamingServer::kAvoidPorts = {
  3000,   // React dev server
  3001,   // Create React App fallback
  4200,   // Angular dev server
  5000,   // Flask default
  5173,   // Vite
  8000,   // Python http.server
  8080,   // Common HTTP alternate
  8081,   // Common HTTP alternate
  8888,   // Jupyter
  9000,   // PHP-FPM
  9090,   // Prometheus
  9200,   // Elasticsearch
  27017,  // MongoDB
};

bool NsiteStreamingServer::Start() {
  // Try to find an available port
  bool bound = false;
  
  // First, try a random port in the range
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(kMinPort, kMaxPort);
  
  // Try random ports up to 10 times
  for (int attempts = 0; attempts < 10; ++attempts) {
    int port = dis(gen);
    
    // Skip if in avoid list
    if (kAvoidPorts.find(port) != kAvoidPorts.end()) {
      continue;
    }
    
    if (TryBindToPort(port)) {
      current_port_ = port;
      bound = true;
      break;
    }
  }
  
  // If random selection failed, scan sequentially
  if (!bound) {
    for (int port = kMinPort; port <= kMaxPort; ++port) {
      // Skip avoided ports
      if (kAvoidPorts.find(port) != kAvoidPorts.end()) {
        continue;
      }
      
      if (TryBindToPort(port)) {
        current_port_ = port;
        bound = true;
        break;
      }
    }
  }
  
  if (!bound) {
    LOG(ERROR) << "Failed to find available port for Nsite server";
    return false;
  }
  
  // Initialize components
  cache_manager_ = std::make_unique<NsiteCacheManager>(
      profile_->GetPath().Append("NsiteCache"));
  
  update_monitor_ = std::make_unique<NsiteUpdateMonitor>();
  
  nsite_resolver_ = std::make_unique<NsiteResolver>(
      profile_->GetURLLoaderFactory());
  
  // Register port with browser service
  NsiteService::GetInstance()->SetServerPort(current_port_);
  
  LOG(INFO) << "Nsite streaming server started on localhost:" << current_port_;
  return true;
}

bool NsiteStreamingServer::TryBindToPort(int port) {
  std::unique_ptr<net::ServerSocketFactory> factory(
      new net::TCPServerSocketFactory());
  
  std::unique_ptr<net::ServerSocket> server_socket =
      factory->CreateAndListen("127.0.0.1", port);
  
  if (!server_socket) {
    VLOG(2) << "Port " << port << " is not available";
    return false;
  }
  
  // Successfully bound, create HTTP server
  http_server_ = std::make_unique<net::HttpServer>(
      std::move(server_socket), this);
  
  return true;
}

void NsiteStreamingServer::Stop() {
  if (http_server_) {
    http_server_.reset();
  }
  
  // Unregister port
  if (current_port_ != 0) {
    NsiteService::GetInstance()->SetServerPort(0);
    current_port_ = 0;
  }
  
  // Save cache to disk
  if (cache_manager_) {
    cache_manager_->SaveToDisk();
  }
  
  LOG(INFO) << "Nsite streaming server stopped";
}

// Browser service to track server port
class NsiteService : public KeyedService {
 public:
  static NsiteService* GetInstance();
  
  void SetServerPort(int port) {
    base::AutoLock lock(port_lock_);
    server_port_ = port;
    
    // Notify observers
    for (auto& observer : observers_) {
      observer.OnServerPortChanged(port);
    }
  }
  
  int GetServerPort() {
    base::AutoLock lock(port_lock_);
    return server_port_;
  }
  
  // Observer interface for port changes
  class Observer {
   public:
    virtual void OnServerPortChanged(int new_port) = 0;
  };
  
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  
 private:
  int server_port_ = 0;
  base::Lock port_lock_;
  base::ObserverList<Observer> observers_;
}
```

### 11. Browser-Side Header Injection System

The browser must ensure the `X-Nsite-Pubkey` header is included in EVERY request to localhost:8081. This is critical because different nsites have conflicting paths.

```cpp
// Browser-side component that tracks current nsite context
class NsiteNavigationContext : public content::WebContentsObserver {
 public:
  explicit NsiteNavigationContext(content::WebContents* web_contents);
  
  // Set when initially navigating to an nsite
  void SetCurrentNsite(const std::string& npub);
  
  // Get current nsite for header injection
  std::string GetCurrentNsite() const { return current_npub_; }
  
  // WebContentsObserver overrides
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  
 private:
  std::string current_npub_;
  bool is_nsite_active_ = false;
};

// WebRequest interceptor to inject headers
class NsiteHeaderInjector {
 public:
  explicit NsiteHeaderInjector(Profile* profile);
  
  void OnBeforeRequest(
      const network::ResourceRequest& request,
      OnBeforeRequestCallback callback);
  
  void OnBeforeSendHeaders(
      const network::ResourceRequest& request,
      net::HttpRequestHeaders* headers,
      OnBeforeSendHeadersCallback callback);
  
 private:
  // Get the npub for the current tab
  std::string GetNpubForRequest(const network::ResourceRequest& request);
  
  // Map of tab ID to current npub
  std::map<int, std::string> tab_npub_map_;
  Profile* profile_;
};

void NsiteHeaderInjector::OnBeforeSendHeaders(
    const network::ResourceRequest& request,
    net::HttpRequestHeaders* headers,
    OnBeforeSendHeadersCallback callback) {
  // Get current server port
  int server_port = NsiteService::GetInstance()->GetServerPort();
  if (server_port == 0) {
    // Server not running
    std::move(callback).Run(net::OK, std::nullopt);
    return;
  }
  
  // Only inject for localhost:<dynamic-port> requests
  if (request.url.host() != "127.0.0.1" || 
      request.url.port() != server_port) {
    std::move(callback).Run(net::OK, std::nullopt);
    return;
  }
  
  // Get npub for this request's tab
  std::string npub = GetNpubForRequest(request);
  if (npub.empty()) {
    // No nsite context, let request proceed without header
    std::move(callback).Run(net::OK, std::nullopt);
    return;
  }
  
  // Inject the header
  headers->SetHeader("X-Nsite-Pubkey", npub);
  
  VLOG(2) << "Injected X-Nsite-Pubkey: " << npub 
          << " for " << request.url.path();
  
  std::move(callback).Run(net::OK, std::nullopt);
}

// Navigation tracking to update context
void NsiteNavigationContext::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame()) {
    return;  // Only track main frame navigations
  }
  
  GURL url = navigation_handle->GetURL();
  
  // Check if this is a navigation to a different nsite
  if (url.scheme() == "nostr" && url.host() == "nsite") {
    // Extract new npub from the nostr:// URL
    std::string new_npub = ExtractNpubFromNostrUrl(url);
    if (new_npub != current_npub_) {
      LOG(INFO) << "Switching nsite context from " 
                << current_npub_ << " to " << new_npub;
      SetCurrentNsite(new_npub);
    }
  }
  
  // If navigating away from localhost:<server-port>, clear context
  int server_port = NsiteService::GetInstance()->GetServerPort();
  if (is_nsite_active_ && server_port > 0 &&
      (url.host() != "127.0.0.1" || url.port() != server_port)) {
    LOG(INFO) << "Leaving nsite context";
    current_npub_.clear();
    is_nsite_active_ = false;
  }
}
```

### 12. Protocol Handler Integration

The nostr:// protocol handler must be updated to redirect to the streaming server and set up the navigation context:

```cpp
void NostrProtocolHandler::HandleNsiteUrl(
    const GURL& url,
    content::WebContents* web_contents) {
  // Parse the nsite URL
  // Format: nostr://nsite/<identifier>/<path>
  std::string path = url.path();
  if (base::StartsWith(path, "/")) {
    path = path.substr(1);
  }
  
  // Extract identifier and resource path
  size_t slash_pos = path.find('/');
  std::string identifier;
  std::string resource_path = "/";
  
  if (slash_pos != std::string::npos) {
    identifier = path.substr(0, slash_pos);
    resource_path = path.substr(slash_pos);
  } else {
    identifier = path;
  }
  
  // Resolve identifier to npub if needed
  std::string npub;
  if (base::StartsWith(identifier, "npub1")) {
    npub = identifier;
  } else {
    // Use NsiteResolver to resolve NIP-05 or domain
    // For now, we'll handle this synchronously
    npub = ResolveToNpub(identifier);
    if (npub.empty()) {
      ShowErrorPage(web_contents, "Failed to resolve: " + identifier);
      return;
    }
  }
  
  // CRITICAL: Set up the navigation context BEFORE redirecting
  auto* context = NsiteNavigationContext::FromWebContents(web_contents);
  if (!context) {
    context = NsiteNavigationContext::CreateForWebContents(web_contents);
  }
  context->SetCurrentNsite(npub);
  
  // Register this tab with the header injector
  NsiteHeaderInjector::GetInstance()->RegisterTab(
      web_contents->GetMainFrame()->GetProcess()->GetID(),
      web_contents->GetMainFrame()->GetRoutingID(),
      npub);
  
  // Get current server port
  int server_port = NsiteService::GetInstance()->GetServerPort();
  if (server_port == 0) {
    // Server not running, try to start it
    if (!NsiteService::GetInstance()->EnsureServerRunning()) {
      ShowErrorPage(web_contents, "Failed to start Nsite server");
      return;
    }
    server_port = NsiteService::GetInstance()->GetServerPort();
  }
  
  // Build localhost URL (serves from root!)
  GURL redirect_url(base::StringPrintf(
      "http://localhost:%d%s",
      server_port,
      resource_path.c_str()));
  
  LOG(INFO) << "Navigating to nsite: " << npub 
            << " at path: " << resource_path
            << " on port: " << server_port;
  
  // Redirect the navigation
  content::NavigationController::LoadURLParams params(redirect_url);
  params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  
  // Add referrer to help server track context as fallback
  params.referrer = content::Referrer(
      GURL("nostr://nsite/" + npub),
      network::mojom::ReferrerPolicy::kDefault);
  
  web_contents->GetController().LoadURLWithParams(params);
}
```

### 12. Performance Optimizations

```cpp
class NsitePreloader {
 public:
  // Preload common resources when user hovers over link
  void PreloadNsite(const std::string& npub);
  
  // Batch fetch multiple files
  void BatchFetchFiles(const std::string& npub,
                      const std::vector<std::string>& paths);
  
 private:
  // Smart preloading based on typical web patterns
  std::vector<std::string> PredictResources(
      const std::string& initial_path);
};

std::vector<std::string> NsitePreloader::PredictResources(
    const std::string& initial_path) {
  std::vector<std::string> resources;
  
  if (initial_path == "/" || initial_path == "/index.html") {
    // Common resources for home page
    resources.push_back("/style.css");
    resources.push_back("/script.js");
    resources.push_back("/favicon.ico");
    resources.push_back("/manifest.json");
  }
  
  return resources;
}
```

### 13. Security Considerations

1. **Port Security**: Bind only to localhost (127.0.0.1) to prevent external access
2. **Path Traversal**: Sanitize all paths to prevent directory traversal attacks
3. **Content Validation**: Always verify content hashes before serving
4. **CORS Headers**: Set appropriate CORS headers for browser compatibility
5. **Request Limits**: Implement rate limiting to prevent DoS

```cpp
bool NsiteStreamingServer::IsPathSafe(const std::string& path) {
  // Prevent directory traversal
  if (path.find("..") != std::string::npos) {
    return false;
  }
  
  // Ensure absolute paths
  if (!base::StartsWith(path, "/")) {
    return false;
  }
  
  // Prevent access to hidden files
  if (path.find("/.") != std::string::npos) {
    return false;
  }
  
  return true;
}
```

### 14. Error Handling and Recovery

```cpp
class NsiteErrorHandler {
 public:
  enum ErrorType {
    NETWORK_ERROR,
    INVALID_CONTENT,
    RELAY_TIMEOUT,
    BLOSSOM_UNAVAILABLE,
    CACHE_CORRUPTION
  };
  
  void HandleError(ErrorType error,
                  const RequestContext& context);
  
 private:
  void LogError(ErrorType error, const std::string& details);
  void ServeErrorPage(int connection_id, ErrorType error);
};
```

### 15. Monitoring and Diagnostics

```cpp
class NsiteServerMetrics {
 public:
  void RecordRequest(const std::string& npub,
                    const std::string& path,
                    bool cache_hit);
  
  void RecordUpdateCheck(const std::string& npub,
                        bool update_found);
  
  void RecordError(NsiteErrorHandler::ErrorType error);
  
  // Get metrics for chrome://nsite-internals
  base::Value GetMetricsSnapshot();
  
 private:
  struct Metrics {
    int64_t total_requests = 0;
    int64_t cache_hits = 0;
    int64_t cache_misses = 0;
    int64_t update_checks = 0;
    int64_t updates_found = 0;
    std::map<int, int64_t> error_counts;
  };
  
  Metrics metrics_;
  base::Lock metrics_lock_;
};
```

### 16. Future Enhancements

1. **WebSocket Support**: For real-time nsite features
2. **Service Worker**: Enable offline functionality
3. **P2P Sharing**: Share cached nsites between local Tungsten instances
4. **Compression**: Support gzip/brotli for bandwidth efficiency
5. **Partial Updates**: Only download changed files
6. **CDN Integration**: Optional CDN fallback for popular nsites