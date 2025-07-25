// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_PROTOCOL_NOSTR_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_NOSTR_PROTOCOL_NOSTR_PROTOCOL_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace nostr {

class NsiteResolver;

// URL loader factory that handles nostr:// and snostr:// schemes by redirecting
// them to the local Nostr server for Nsite resolution and serving.
class NostrProtocolURLLoaderFactory
    : public network::SelfDeletingURLLoaderFactory {
 public:
  // Creates a new factory for handling nostr:// URLs
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create(
      content::BrowserContext* browser_context);

private:
  NostrProtocolURLLoaderFactory(
      content::BrowserContext* browser_context,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);

  ~NostrProtocolURLLoaderFactory() override;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory)
      override;

 public:
  // For testing
  bool IsNsiteUrl(const GURL& url) const;
  GURL ConvertNostrUrlToLocalhost(const GURL& nostr_url);

 private:
  // Data structure for nsite resolution callbacks
  struct NsiteResolveCallbackData {
    mojo::Remote<network::mojom::URLLoaderClient> client;
    Profile* profile;
    std::string remaining_path;
    GURL original_url;
  };

  // Handle nsite URL resolution and redirection
  void HandleNsiteUrl(
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // Callback for nsite identifier resolution
  void OnNsiteResolved(
      std::unique_ptr<NsiteResolver> resolver,
      NsiteResolveCallbackData* callback_data,
      const std::string& npub);

  // Get the local Nostr server port
  int GetLocalServerPort();

  content::BrowserContext* browser_context_;

  base::WeakPtrFactory<NostrProtocolURLLoaderFactory> weak_factory_{this};
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_PROTOCOL_NOSTR_PROTOCOL_HANDLER_H_