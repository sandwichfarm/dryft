// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_PROTOCOL_NSITE_RESOLVER_H_
#define CHROME_BROWSER_NOSTR_PROTOCOL_NSITE_RESOLVER_H_

#include <memory>
#include <string>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace nostr {

// Resolves Nsite URLs to Nostr pubkeys using various methods
class NsiteResolver {
 public:
  // Result of Nsite resolution
  struct ResolveResult {
    std::string pubkey;  // Hex-encoded public key
    std::string domain;  // Domain that was resolved
    std::string path;    // Path component from URL
    base::Time resolved_at;
  };

  using ResolveCallback = base::OnceCallback<void(std::optional<ResolveResult>)>;

  explicit NsiteResolver(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~NsiteResolver();

  // Resolve a Nsite URL to a pubkey
  // Format: nostr://nsite/<identifier>/<path>
  // Where identifier can be:
  // - npub1xxx.domain.com (npub in subdomain)
  // - user@domain.com (NIP-05 format)
  // - domain.com (look up _@domain.com)
  void Resolve(const GURL& nsite_url, ResolveCallback callback);

  // Clear the resolution cache
  void ClearCache();

  // Set cache TTL (default: 1 hour)
  void SetCacheTTL(base::TimeDelta ttl) { cache_ttl_ = ttl; }

 private:
  // Parse Nsite URL and extract components
  struct ParsedNsiteUrl {
    std::string identifier;
    std::string path;
    bool is_npub_subdomain = false;
    bool is_nip05 = false;
  };

  std::optional<ParsedNsiteUrl> ParseNsiteUrl(const GURL& url);

  // Try to extract npub from subdomain
  std::optional<std::string> ExtractNpubFromSubdomain(const std::string& host);

  // Resolve via NIP-05 (HTTPS lookup)
  void ResolveViaNip05(const std::string& identifier,
                      const std::string& path,
                      ResolveCallback callback);

  // Handle NIP-05 response
  void OnNip05Response(const std::string& identifier,
                      const std::string& path,
                      ResolveCallback callback,
                      std::unique_ptr<std::string> response_body);

  // Resolve via DNS TXT record
  void ResolveViaDns(const std::string& domain,
                    const std::string& path,
                    ResolveCallback callback);

  // Check cache for existing resolution
  std::optional<ResolveResult> CheckCache(const std::string& identifier);

  // Add result to cache
  void CacheResult(const std::string& identifier, const ResolveResult& result);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  
  // Simple in-memory cache
  std::map<std::string, ResolveResult> cache_;
  base::TimeDelta cache_ttl_ = base::Hours(1);

  // Active URL loaders
  std::vector<std::unique_ptr<network::SimpleURLLoader>> active_loaders_;

  base::WeakPtrFactory<NsiteResolver> weak_factory_{this};
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_PROTOCOL_NSITE_RESOLVER_H_