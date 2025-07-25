// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/protocol/nostr_protocol_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/nostr/local_relay/local_relay_service.h"
#include "chrome/browser/nostr/local_relay/local_relay_service_factory.h"
#include "chrome/browser/nostr/nsite/nsite_service.h"
#include "chrome/browser/nostr/protocol/nsite_resolver.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/nostr_scheme.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace nostr {

namespace {

// Default port for local Nostr server (matches LocalRelayConfigManager::kDefaultPort)
constexpr int kDefaultLocalServerPort = 8081;

// Nsite URL prefix
constexpr char kNsitePrefix[] = "/nsite/";

}  // namespace

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
NostrProtocolURLLoaderFactory::Create(content::BrowserContext* browser_context) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;
  
  // The factory will delete itself when the pipe is closed.
  new NostrProtocolURLLoaderFactory(
      browser_context, 
      pending_remote.InitWithNewPipeAndPassReceiver());
  
  return pending_remote;
}

NostrProtocolURLLoaderFactory::NostrProtocolURLLoaderFactory(
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver)
    : network::SelfDeletingURLLoaderFactory(std::move(factory_receiver)),
      browser_context_(browser_context) {
  DCHECK(browser_context_);
}

NostrProtocolURLLoaderFactory::~NostrProtocolURLLoaderFactory() = default;

void NostrProtocolURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  
  VLOG(1) << "NostrProtocolURLLoaderFactory handling: " << request.url;
  
  // Check if this is a nsite URL and handle specially
  if (IsNsiteUrl(request.url)) {
    HandleNsiteUrl(request, std::move(client));
    return;
  }
  
  // Convert regular nostr:// URL to localhost HTTP URL
  GURL localhost_url = ConvertNostrUrlToLocalhost(request.url);
  
  if (!localhost_url.is_valid()) {
    LOG(ERROR) << "Failed to convert nostr URL: " << request.url;
    mojo::Remote<network::mojom::URLLoaderClient> client_remote(std::move(client));
    client_remote->OnComplete(network::URLLoaderCompletionStatus(net::ERR_INVALID_URL));
    return;
  }
  
  VLOG(1) << "Redirecting nostr URL to: " << localhost_url;
  
  // Create a redirect response
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_head->headers->ReplaceStatusLine("HTTP/1.1 307 Temporary Redirect");
  response_head->headers->SetHeader("Location", localhost_url.spec());
  response_head->encoded_data_length = 0;
  response_head->content_length = 0;
  
  mojo::Remote<network::mojom::URLLoaderClient> client_remote(std::move(client));
  client_remote->OnReceiveRedirect(net::RedirectInfo::ComputeRedirectInfo(
      "GET", request.url, localhost_url, net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL), 
      std::move(response_head));
}

void NostrProtocolURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory) {
  new NostrProtocolURLLoaderFactory(browser_context_, std::move(factory));
}

bool NostrProtocolURLLoaderFactory::IsNsiteUrl(const GURL& url) const {
  if (!url.is_valid()) {
    return false;
  }
  
  std::string scheme = url.scheme();
  if (scheme != chrome::kNostrScheme && scheme != chrome::kSecureNostrScheme) {
    return false;
  }
  
  // Check if path starts with /nsite/ and contains a non-empty identifier
  std::string path = url.path();
  return base::StartsWith(path, kNsitePrefix) && path.size() > strlen(kNsitePrefix);
}

void NostrProtocolURLLoaderFactory::HandleNsiteUrl(
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  
  mojo::Remote<network::mojom::URLLoaderClient> client_remote(std::move(client));
  
  // Extract identifier from URL: nostr://host/nsite/<identifier>/path
  std::string path = request.url.path();
  if (!base::StartsWith(path, kNsitePrefix)) {
    LOG(ERROR) << "Invalid nsite URL format: " << request.url;
    client_remote->OnComplete(network::URLLoaderCompletionStatus(net::ERR_INVALID_URL));
    return;
  }
  
  // Remove /nsite/ prefix
  path = path.substr(strlen(kNsitePrefix));
  
  // Extract identifier (everything up to next slash or end)
  std::string identifier;
  std::string remaining_path;
  size_t slash_pos = path.find('/');
  if (slash_pos != std::string::npos) {
    identifier = path.substr(0, slash_pos);
    remaining_path = path.substr(slash_pos); // includes leading slash
  } else {
    identifier = path;
    remaining_path = "/";
  }
  
  if (identifier.empty()) {
    LOG(ERROR) << "Empty nsite identifier: " << request.url;
    client_remote->OnComplete(network::URLLoaderCompletionStatus(net::ERR_INVALID_URL));
    return;
  }
  
  VLOG(1) << "Nsite URL - identifier: " << identifier << ", path: " << remaining_path;
  
  // Get profile from browser context
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  if (!profile || profile->IsOffTheRecord()) {
    LOG(ERROR) << "No valid profile for nsite resolution";
    client_remote->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }
  
  // Create resolver and resolve identifier
  auto resolver = std::make_unique<NsiteResolver>();
  
  // Store callback data
  auto callback_data = std::make_unique<NsiteResolveCallbackData>();
  callback_data->client = std::move(client_remote);
  callback_data->profile = profile;
  callback_data->remaining_path = remaining_path;
  callback_data->original_url = request.url;
  
  NsiteResolveCallbackData* callback_data_ptr = callback_data.release();
  
  // Resolve identifier to npub
  resolver->ResolveIdentifier(
      identifier,
      base::BindOnce(&NostrProtocolURLLoaderFactory::OnNsiteResolved,
                     weak_factory_.GetWeakPtr(),
                     std::move(resolver),
                     base::Owned(callback_data_ptr)));
}

void NostrProtocolURLLoaderFactory::OnNsiteResolved(
    std::unique_ptr<NsiteResolver> resolver,
    NsiteResolveCallbackData* callback_data,
    const std::string& npub) {
  
  if (npub.empty()) {
    LOG(ERROR) << "Failed to resolve nsite identifier";
    callback_data->client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_NAME_NOT_RESOLVED));
    return;
  }
  
  VLOG(1) << "Resolved nsite to npub: " << npub;
  
  // Get or start streaming server
  NsiteService* nsite_service = NsiteService::GetInstance();
  uint16_t port = nsite_service->GetOrStartServer(callback_data->profile);
  
  if (port == 0) {
    LOG(ERROR) << "Failed to start nsite streaming server";
    callback_data->client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }
  
  // Build localhost URL with streaming server port
  GURL localhost_url = GURL(base::StringPrintf("http://localhost:%d%s", 
                                               port, 
                                               callback_data->remaining_path.c_str()));
  
  if (!localhost_url.is_valid()) {
    LOG(ERROR) << "Invalid localhost URL constructed";
    callback_data->client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INVALID_URL));
    return;
  }
  
  VLOG(1) << "Redirecting nsite to: " << localhost_url;
  
  // TODO: Set nsite context for the tab - need WebContents access
  // This will be handled by the header injector when the request is made
  
  // Create redirect response
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_head->headers->ReplaceStatusLine("HTTP/1.1 307 Temporary Redirect");
  response_head->headers->SetHeader("Location", localhost_url.spec());
  
  // Add X-Npub header to help streaming server identify the nsite
  response_head->headers->SetHeader("X-Npub", npub);
  
  response_head->encoded_data_length = 0;
  response_head->content_length = 0;
  
  callback_data->client->OnReceiveRedirect(
      net::RedirectInfo::ComputeRedirectInfo(
          "GET", callback_data->original_url, localhost_url, 
          net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL),
      std::move(response_head));
}

GURL NostrProtocolURLLoaderFactory::ConvertNostrUrlToLocalhost(const GURL& nostr_url) {
  if (!nostr_url.is_valid()) {
    return GURL();
  }
  
  std::string scheme = nostr_url.scheme();
  if (scheme != chrome::kNostrScheme && scheme != chrome::kSecureNostrScheme) {
    return GURL();
  }
  
  // Determine if we should use HTTP or HTTPS based on the scheme
  std::string target_scheme = (scheme == chrome::kSecureNostrScheme) ? "https" : "http";
  
  // Get the local server port
  int port = GetLocalServerPort();
  
  // Build the localhost URL using GURL APIs
  // nostr://example.com/path -> http://localhost:7777/nostr/example.com/path
  // snostr://example.com/path -> https://localhost:7777/nostr/example.com/path
  
  // Start with the base localhost URL
  GURL::Replacements replacements;
  replacements.SetSchemeStr(target_scheme);
  replacements.SetHostStr("localhost");
  replacements.SetPortStr(base::NumberToString(port));
  
  // Build the path: /nostr/[host][original_path]
  std::string new_path = "/nostr";
  if (nostr_url.has_host()) {
    new_path += "/" + nostr_url.host();
  }
  if (nostr_url.has_path()) {
    new_path += nostr_url.path();
  }
  replacements.SetPathStr(new_path);
  
  // Preserve query and fragment
  if (nostr_url.has_query()) {
    replacements.SetQueryStr(nostr_url.query());
  }
  if (nostr_url.has_ref()) {
    replacements.SetRefStr(nostr_url.ref());
  }
  
  // Create a base URL and apply replacements
  GURL base_url("http://localhost/");
  return base_url.ReplaceComponents(replacements);
}

int NostrProtocolURLLoaderFactory::GetLocalServerPort() {
  // Try to get the port from the local relay config manager
  if (browser_context_) {
    auto* config_manager = 
        nostr::local_relay::LocalRelayServiceFactory::GetForBrowserContext(
            browser_context_);
    if (config_manager) {
      int port = config_manager->GetPort();
      if (port > 0) {
        return port;
      }
    }
  }
  
  // Fall back to default port
  return kDefaultLocalServerPort;
}


}  // namespace nostr