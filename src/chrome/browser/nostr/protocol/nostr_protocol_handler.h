// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_PROTOCOL_NOSTR_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_NOSTR_PROTOCOL_NOSTR_PROTOCOL_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace nostr {

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

  // Convert nostr:// URL to localhost HTTP URL for local server (public for testing)
  GURL ConvertNostrUrlToLocalhost(const GURL& nostr_url);

  // Get the local Nostr server port
  int GetLocalServerPort();

  content::BrowserContext* browser_context_;

  base::WeakPtrFactory<NostrProtocolURLLoaderFactory> weak_factory_{this};
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_PROTOCOL_NOSTR_PROTOCOL_HANDLER_H_