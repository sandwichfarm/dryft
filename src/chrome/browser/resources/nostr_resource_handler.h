// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_NOSTR_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RESOURCES_NOSTR_RESOURCE_HANDLER_H_

#include <string>

#include "content/public/browser/url_data_source.h"

namespace chrome {

// Handles serving bundled Nostr JavaScript libraries at chrome:// URLs.
// Libraries are accessible at chrome://resources/js/nostr/*.js
class NostrResourceHandler : public content::URLDataSource {
 public:
  NostrResourceHandler();
  ~NostrResourceHandler() override;

  // content::URLDataSource implementation:
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL& url) override;
  bool ShouldServeMimeTypeAsContentTypeHeader() override;
  void AddResponseHeaders(const GURL& url,
                          content::HttpResponseHeaders* headers) override;
  bool AllowCaching() override;

  // Maps library paths to resource IDs (public for testing)
  int GetResourceIdForPath(const std::string& path) const;
};

// Registers the Nostr resource handler with the browser.
void RegisterNostrResources();

}  // namespace chrome

#endif  // CHROME_BROWSER_RESOURCES_NOSTR_RESOURCE_HANDLER_H_