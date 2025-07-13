// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOSSOM_BLOSSOM_CONTENT_RESOLVER_H_
#define COMPONENTS_BLOSSOM_BLOSSOM_CONTENT_RESOLVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace blossom {

class BlossomUserServerManager;
struct BlossomServer;

// Result of content resolution
struct ContentResolutionResult {
  enum Status {
    SUCCESS,          // Content found and retrieved
    NOT_FOUND,        // Content not found on any server
    NETWORK_ERROR,    // Network/connection errors
    TIMEOUT,          // Operation timed out
    UNAUTHORIZED,     // Authorization failed
  };
  
  Status status = NOT_FOUND;
  std::string content;
  std::string mime_type;
  GURL resolved_url;        // URL where content was found
  base::TimeDelta duration; // Time taken to resolve
  std::string error_message;
};

// Resolves content from multiple Blossom servers with fallback
class BlossomContentResolver {
 public:
  // Configuration for content resolution
  struct Config {
    base::TimeDelta server_timeout = base::Seconds(30);
    base::TimeDelta total_timeout = base::Minutes(2);
    size_t max_concurrent_requests = 3;
    size_t max_servers_to_try = 10;
    bool prefer_recent_servers = true;
  };

  using ResolveCallback = base::OnceCallback<void(ContentResolutionResult result)>;
  using UploadCallback = base::OnceCallback<void(
      std::vector<GURL> success_urls,
      std::vector<std::pair<GURL, std::string>> failures)>;

  BlossomContentResolver(const Config& config,
                        BlossomUserServerManager* server_manager);
  ~BlossomContentResolver();

  // Resolve content by hash from user's servers
  void ResolveContent(const std::string& pubkey,
                     const std::string& hash,
                     ResolveCallback callback);

  // Upload content to multiple servers (mirroring)
  void UploadContent(const std::string& pubkey,
                    const std::string& hash,
                    const std::string& content,
                    const std::string& mime_type,
                    UploadCallback callback);

  // Check if content exists on any server
  void CheckContentExists(const std::string& pubkey,
                         const std::string& hash,
                         base::OnceCallback<void(bool exists, GURL found_url)> callback);

 private:
  // Internal state for resolution operation
  struct ResolutionState {
    std::string pubkey;
    std::string hash;
    ResolveCallback callback;
    std::vector<BlossomServer*> servers_to_try;
    size_t current_server_index = 0;
    base::Time start_time;
    base::Time current_server_start_time;
    ContentResolutionResult result;
  };

  // Internal state for upload operation
  struct UploadState {
    std::string pubkey;
    std::string hash;
    std::string content;
    std::string mime_type;
    UploadCallback callback;
    std::vector<BlossomServer*> servers;
    std::vector<GURL> success_urls;
    std::vector<std::pair<GURL, std::string>> failures;
    size_t pending_uploads = 0;
    size_t total_servers = 0;
    size_t next_server_index = 0;
  };

  // Try to resolve content from next server
  void TryNextServer(std::unique_ptr<ResolutionState> state);

  // Handle single server resolution result
  void OnServerResolutionComplete(std::unique_ptr<ResolutionState> state,
                                 bool success,
                                 const std::string& content,
                                 const std::string& mime_type,
                                 const std::string& error);

  // Upload to a single server
  void UploadToServer(std::shared_ptr<UploadState> state, BlossomServer* server);

  // Handle single server upload result
  void OnServerUploadComplete(std::shared_ptr<UploadState> state,
                             const GURL& server_url,
                             bool success,
                             const std::string& error);

  // Perform actual HTTP GET request to server
  void FetchFromServer(const GURL& server_url,
                      const std::string& hash,
                      base::OnceCallback<void(bool success,
                                            const std::string& content,
                                            const std::string& mime_type,
                                            const std::string& error)> callback);

  // Perform actual HTTP PUT request to server
  void UploadToServerHttp(const GURL& server_url,
                         const std::string& hash,
                         const std::string& content,
                         const std::string& mime_type,
                         base::OnceCallback<void(bool success,
                                               const std::string& error)> callback);

  // Configuration
  Config config_;

  // Server manager (not owned)
  BlossomUserServerManager* server_manager_;

  // Thread checker
  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointer factory
  base::WeakPtrFactory<BlossomContentResolver> weak_factory_{this};
};

}  // namespace blossom

#endif  // COMPONENTS_BLOSSOM_BLOSSOM_CONTENT_RESOLVER_H_