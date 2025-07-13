// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/protocol/nsite_resolver.h"

#include <algorithm>
#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/dns/public/dns_query_type.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace nostr {

namespace {

const char kNostrWellKnownPath[] = "/.well-known/nostr.json";
const char kNostrDnsTxtPrefix[] = "_nostr.";

// Traffic annotation for NIP-05 resolution
constexpr net::NetworkTrafficAnnotationTag kNip05TrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("nsite_nip05_resolution", R"(
      semantics {
        sender: "Nsite Resolver"
        description:
          "Resolves Nostr pubkeys from domain names using NIP-05 protocol. "
          "This makes HTTPS requests to domain/.well-known/nostr.json to "
          "look up Nostr public keys associated with usernames."
        trigger:
          "When a user navigates to a nostr://nsite/ URL that contains a "
          "NIP-05 identifier (user@domain.com format)."
        data:
          "The domain name and optional username to resolve."
        destination: OTHER
        destination_other:
          "The domain specified in the Nsite URL."
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature is enabled when Nostr protocol support is enabled."
        policy_exception_justification:
          "Not implemented."
      })");

// Extract username and domain from NIP-05 identifier
std::pair<std::string, std::string> ParseNip05(const std::string& identifier) {
  size_t at_pos = identifier.find('@');
  if (at_pos == std::string::npos) {
    // No @ sign, treat entire string as domain with _ username
    return {"_", identifier};
  }
  
  std::string username = identifier.substr(0, at_pos);
  std::string domain = identifier.substr(at_pos + 1);
  
  // Empty username defaults to _
  if (username.empty()) {
    username = "_";
  }
  
  return {username, domain};
}

// Validate hex pubkey format
bool IsValidHexPubkey(const std::string& pubkey) {
  if (pubkey.length() != 64) {
    return false;
  }
  
  return std::all_of(pubkey.begin(), pubkey.end(), [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || 
           (c >= 'A' && c <= 'F');
  });
}

}  // namespace

NsiteResolver::NsiteResolver(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

NsiteResolver::~NsiteResolver() = default;

void NsiteResolver::Resolve(const GURL& nsite_url, ResolveCallback callback) {
  auto parsed = ParseNsiteUrl(nsite_url);
  if (!parsed) {
    LOG(WARNING) << "Invalid Nsite URL: " << nsite_url;
    std::move(callback).Run(std::nullopt);
    return;
  }
  
  // Check cache first
  auto cached = CheckCache(parsed->identifier);
  if (cached) {
    // Update path from current request
    cached->path = parsed->path;
    std::move(callback).Run(cached);
    return;
  }
  
  // Try NIP-05 resolution
  ResolveViaNip05(parsed->identifier, parsed->path, std::move(callback));
}

void NsiteResolver::ClearCache() {
  cache_.clear();
}

std::optional<NsiteResolver::ParsedNsiteUrl> NsiteResolver::ParseNsiteUrl(
    const GURL& url) {
  if (!url.is_valid() || url.scheme() != "nostr") {
    return std::nullopt;
  }
  
  // Extract host (which should be "nsite")
  if (url.host() != "nsite") {
    return std::nullopt;
  }
  
  // Get path without leading slash
  std::string path = url.path();
  if (!path.empty() && path[0] == '/') {
    path = path.substr(1);
  }
  
  // Split into identifier and remaining path
  size_t slash_pos = path.find('/');
  ParsedNsiteUrl result;
  
  if (slash_pos != std::string::npos) {
    result.identifier = path.substr(0, slash_pos);
    result.path = path.substr(slash_pos + 1);
  } else {
    result.identifier = path;
    result.path = "";
  }
  
  if (result.identifier.empty()) {
    return std::nullopt;
  }
  
  // Determine type of identifier
  if (result.identifier.find('@') != std::string::npos || 
      result.identifier.find('.') != std::string::npos) {
    result.is_nip05 = true;
  }
  
  return result;
}


void NsiteResolver::ResolveViaNip05(const std::string& identifier,
                                    const std::string& path,
                                    ResolveCallback callback) {
  auto [username, domain] = ParseNip05(identifier);
  
  // Build NIP-05 URL
  GURL nip05_url(std::string("https://") + domain + kNostrWellKnownPath);
  if (!nip05_url.is_valid()) {
    LOG(WARNING) << "Invalid NIP-05 domain: " << domain;
    std::move(callback).Run(std::nullopt);
    return;
  }
  
  // Create request
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = nip05_url;
  resource_request->method = "GET";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  
  // Create loader
  auto loader = network::SimpleURLLoader::Create(
      std::move(resource_request), kNip05TrafficAnnotation);
  loader->SetTimeoutDuration(base::Seconds(10));
  
  auto* loader_ptr = loader.get();
  active_loaders_.push_back(std::move(loader));
  
  // Start request
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&NsiteResolver::OnNip05Response,
                     weak_factory_.GetWeakPtr(),
                     identifier, path, std::move(callback)),
      1024 * 1024);  // 1MB max
}

void NsiteResolver::OnNip05Response(const std::string& identifier,
                                   const std::string& path,
                                   ResolveCallback callback,
                                   std::unique_ptr<std::string> response_body) {
  // Find and remove the loader
  auto loader_it = std::find_if(
      active_loaders_.begin(), active_loaders_.end(),
      [](const std::unique_ptr<network::SimpleURLLoader>& loader) {
        return loader->GetFinalURL().is_valid();
      });
  
  if (loader_it != active_loaders_.end()) {
    int response_code = -1;
    if ((*loader_it)->ResponseInfo() && (*loader_it)->ResponseInfo()->headers) {
      response_code = (*loader_it)->ResponseInfo()->headers->response_code();
    }
    
    active_loaders_.erase(loader_it);
    
    if (response_code != net::HTTP_OK || !response_body) {
      LOG(WARNING) << "NIP-05 request failed with code: " << response_code;
      std::move(callback).Run(std::nullopt);
      return;
    }
  }
  
  // Parse JSON response
  auto json_value = base::JSONReader::Read(*response_body);
  if (!json_value || !json_value->is_dict()) {
    LOG(WARNING) << "Invalid NIP-05 JSON response";
    std::move(callback).Run(std::nullopt);
    return;
  }
  
  const base::Value::Dict& json = json_value->GetDict();
  
  // Look for "names" object
  const base::Value::Dict* names = json.FindDict("names");
  if (!names) {
    LOG(WARNING) << "No 'names' field in NIP-05 response";
    std::move(callback).Run(std::nullopt);
    return;
  }
  
  // Extract username from identifier
  auto [username, domain] = ParseNip05(identifier);
  
  // Look up pubkey for username
  const std::string* pubkey_hex = names->FindString(username);
  if (!pubkey_hex) {
    LOG(WARNING) << "Username not found in NIP-05 response: " << username;
    std::move(callback).Run(std::nullopt);
    return;
  }
  
  // Validate pubkey format
  if (!IsValidHexPubkey(*pubkey_hex)) {
    LOG(WARNING) << "Invalid pubkey format in NIP-05 response";
    std::move(callback).Run(std::nullopt);
    return;
  }
  
  // Create result
  ResolveResult result;
  result.pubkey = *pubkey_hex;
  result.domain = domain;
  result.path = path;
  result.resolved_at = base::Time::Now();
  
  CacheResult(identifier, result);
  std::move(callback).Run(result);
}

void NsiteResolver::ResolveViaDns(const std::string& domain,
                                 const std::string& path,
                                 ResolveCallback callback) {
  // TODO: Implement DNS TXT record lookup
  // For now, this is a stub that always fails
  LOG(INFO) << "DNS TXT lookup not yet implemented for: " << domain;
  std::move(callback).Run(std::nullopt);
}

std::optional<NsiteResolver::ResolveResult> NsiteResolver::CheckCache(
    const std::string& identifier) {
  auto it = cache_.find(identifier);
  if (it == cache_.end()) {
    return std::nullopt;
  }
  
  // Check if cache entry is still valid
  if (base::Time::Now() - it->second.resolved_at > cache_ttl_) {
    cache_.erase(it);
    return std::nullopt;
  }
  
  return it->second;
}

void NsiteResolver::CacheResult(const std::string& identifier,
                               const ResolveResult& result) {
  cache_[identifier] = result;
  
  // Limit cache size
  while (cache_.size() > 1000) {
    // Find oldest entry
    auto oldest = cache_.begin();
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
      if (it->second.resolved_at < oldest->second.resolved_at) {
        oldest = it;
      }
    }
    cache_.erase(oldest);
  }
}

}  // namespace nostr