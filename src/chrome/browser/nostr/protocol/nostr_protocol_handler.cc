// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/protocol/nostr_protocol_handler.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/nostr/local_relay/local_relay_service.h"
#include "chrome/browser/nostr/local_relay/local_relay_service_factory.h"
#include "chrome/common/nostr_scheme.h"
#include "content/public/browser/browser_context.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace nostr {

namespace {

// Default port for local Nostr server (matches LocalRelayConfigManager::kDefaultPort)
constexpr int kDefaultLocalServerPort = 8081;

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
  
  // Convert nostr:// URL to localhost HTTP URL
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