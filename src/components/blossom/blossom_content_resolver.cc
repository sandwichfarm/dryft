// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blossom/blossom_content_resolver.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "components/blossom/blossom_user_server_manager.h"

namespace blossom {

BlossomContentResolver::BlossomContentResolver(
    const Config& config,
    BlossomUserServerManager* server_manager)
    : config_(config), server_manager_(server_manager) {
  DCHECK(server_manager_);
  DCHECK_GT(config_.max_servers_to_try, 0u);
  DCHECK_GT(config_.max_concurrent_requests, 0u);
}

BlossomContentResolver::~BlossomContentResolver() = default;

void BlossomContentResolver::ResolveContent(const std::string& pubkey,
                                           const std::string& hash,
                                           ResolveCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  auto state = std::make_unique<ResolutionState>();
  state->pubkey = pubkey;
  state->hash = hash;
  state->callback = std::move(callback);
  state->start_time = base::Time::Now();
  
  // Get best servers for this user
  state->servers_to_try = server_manager_->GetBestServers(
      pubkey, config_.max_servers_to_try);
  
  if (state->servers_to_try.empty()) {
    LOG(WARNING) << "No servers available for user " << pubkey;
    state->result.status = ContentResolutionResult::NOT_FOUND;
    state->result.error_message = "No servers configured";
    state->result.duration = base::Time::Now() - state->start_time;
    std::move(state->callback).Run(state->result);
    return;
  }
  
  LOG(INFO) << "Resolving content " << hash << " for user " << pubkey
            << " using " << state->servers_to_try.size() << " servers";
  
  // Start resolution with first server
  TryNextServer(std::move(state));
}

void BlossomContentResolver::UploadContent(const std::string& pubkey,
                                          const std::string& hash,
                                          const std::string& content,
                                          const std::string& mime_type,
                                          UploadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  auto state = std::make_shared<UploadState>();
  state->pubkey = pubkey;
  state->hash = hash;
  state->content = content;
  state->mime_type = mime_type;
  state->callback = std::move(callback);
  
  // Get servers for upload
  state->servers = server_manager_->GetBestServers(pubkey);
  
  if (state->servers.empty()) {
    LOG(WARNING) << "No servers available for upload";
    std::move(state->callback).Run({}, {});
    return;
  }
  
  LOG(INFO) << "Uploading content " << hash << " to " 
            << state->servers.size() << " servers";
  
  // Start concurrent uploads (limit concurrent requests, but upload to all servers)
  state->total_servers = state->servers.size();
  state->pending_uploads = state->total_servers;
  state->next_server_index = 0;
  
  // Start initial batch of uploads up to max_concurrent_requests
  size_t initial_uploads = std::min(state->servers.size(),
                                   config_.max_concurrent_requests);
  for (size_t i = 0; i < initial_uploads; i++) {
    UploadToServer(state, state->servers[i]);
    state->next_server_index++;
  }
}

void BlossomContentResolver::CheckContentExists(
    const std::string& pubkey,
    const std::string& hash,
    base::OnceCallback<void(bool exists, GURL found_url)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  // Use HEAD request to check existence without downloading content
  auto servers = server_manager_->GetBestServers(pubkey, 5);
  
  if (servers.empty()) {
    std::move(callback).Run(false, GURL());
    return;
  }
  
  // TODO: Implement HEAD request checking
  // For now, simulate by checking first server
  // Return the full resource URL including the hash path
  GURL resource_url = servers[0]->url.Resolve("/" + hash);
  std::move(callback).Run(true, resource_url);
}

void BlossomContentResolver::TryNextServer(std::unique_ptr<ResolutionState> state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  // Check timeout
  base::TimeDelta elapsed = base::Time::Now() - state->start_time;
  if (elapsed >= config_.total_timeout) {
    LOG(WARNING) << "Content resolution timed out after " << elapsed;
    state->result.status = ContentResolutionResult::TIMEOUT;
    state->result.error_message = "Resolution timed out";
    state->result.duration = elapsed;
    std::move(state->callback).Run(state->result);
    return;
  }
  
  // Check if we have more servers to try
  if (state->current_server_index >= state->servers_to_try.size()) {
    LOG(WARNING) << "Content not found on any server";
    state->result.status = ContentResolutionResult::NOT_FOUND;
    state->result.error_message = "Content not found on any server";
    state->result.duration = elapsed;
    std::move(state->callback).Run(state->result);
    return;
  }
  
  BlossomServer* server = state->servers_to_try[state->current_server_index];
  state->current_server_index++;
  state->current_server_start_time = base::Time::Now();
  
  LOG(INFO) << "Trying server " << server->url << " for content " << state->hash;
  
  // Fetch from this server
  FetchFromServer(
      server->url, state->hash,
      base::BindOnce(&BlossomContentResolver::OnServerResolutionComplete,
                     weak_factory_.GetWeakPtr(), std::move(state)));
}

void BlossomContentResolver::OnServerResolutionComplete(
    std::unique_ptr<ResolutionState> state,
    bool success,
    const std::string& content,
    const std::string& mime_type,
    const std::string& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  // Check if this server took too long
  base::TimeDelta server_duration = base::Time::Now() - state->current_server_start_time;
  if (server_duration >= config_.server_timeout) {
    LOG(WARNING) << "Server request timed out after " << server_duration;
    success = false;
  }
  
  if (success) {
    LOG(INFO) << "Content resolved successfully from server";
    
    // Mark server as successful
    if (state->current_server_index > 0) {
      BlossomServer* server = state->servers_to_try[state->current_server_index - 1];
      server->MarkSuccess();
      state->result.resolved_url = server->url;
    }
    
    state->result.status = ContentResolutionResult::SUCCESS;
    state->result.content = content;
    state->result.mime_type = mime_type;
    state->result.duration = base::Time::Now() - state->start_time;
    
    std::move(state->callback).Run(state->result);
    return;
  }
  
  // Mark server as failed
  if (state->current_server_index > 0) {
    BlossomServer* server = state->servers_to_try[state->current_server_index - 1];
    server->MarkFailure();
  }
  
  LOG(WARNING) << "Server failed: " << error;
  
  // Try next server
  TryNextServer(std::move(state));
}

void BlossomContentResolver::UploadToServer(std::shared_ptr<UploadState> state,
                                           BlossomServer* server) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  LOG(INFO) << "Uploading to server " << server->url;
  
  UploadToServerHttp(
      server->url, state->hash, state->content, state->mime_type,
      base::BindOnce(&BlossomContentResolver::OnServerUploadComplete,
                     weak_factory_.GetWeakPtr(), state, server->url));
}

void BlossomContentResolver::OnServerUploadComplete(
    std::shared_ptr<UploadState> state,
    const GURL& server_url,
    bool success,
    const std::string& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (success) {
    LOG(INFO) << "Upload successful to " << server_url;
    state->success_urls.push_back(server_url);
  } else {
    LOG(WARNING) << "Upload failed to " << server_url << ": " << error;
    state->failures.emplace_back(server_url, error);
  }
  
  state->pending_uploads--;
  
  // Start upload to next server if we have more servers to try
  if (state->next_server_index < state->servers.size()) {
    UploadToServer(state, state->servers[state->next_server_index]);
    state->next_server_index++;
    state->pending_uploads++;
  }
  
  // Check if all uploads are complete
  if (state->pending_uploads == 0) {
    LOG(INFO) << "Upload complete: " << state->success_urls.size() 
              << " successes, " << state->failures.size() << " failures";
    std::move(state->callback).Run(state->success_urls, state->failures);
  }
}

void BlossomContentResolver::FetchFromServer(
    const GURL& server_url,
    const std::string& hash,
    base::OnceCallback<void(bool success,
                          const std::string& content,
                          const std::string& mime_type,
                          const std::string& error)> callback) {
  // TODO: Implement actual HTTP GET request
  // For now, simulate a network request
  
  GURL content_url = server_url.Resolve("/" + hash);
  
  // Simulate network delay and failure
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), 
                     false,  // Simulate failure for now
                     "",     // No content
                     "",     // No mime type
                     "Simulated network error"),
      base::Milliseconds(100));  // Fixed delay for deterministic tests
}

void BlossomContentResolver::UploadToServerHttp(
    const GURL& server_url,
    const std::string& hash,
    const std::string& content,
    const std::string& mime_type,
    base::OnceCallback<void(bool success, const std::string& error)> callback) {
  // TODO: Implement actual HTTP PUT request
  // For now, simulate upload
  
  GURL upload_url = server_url.Resolve("/" + hash);
  
  // Simulate network delay and success
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), 
                     true,  // Simulate success for now
                     ""),   // No error
      base::Milliseconds(200));  // Fixed delay for deterministic tests
}

}  // namespace blossom