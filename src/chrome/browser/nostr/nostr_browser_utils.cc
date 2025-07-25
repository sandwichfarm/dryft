// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nostr_browser_utils.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/resources/nostr_resource_handler.h"
#include "chrome/common/chrome_features.h"
#include "components/nostr/features.h"

namespace nostr {

void InitializeNostrResources() {
  if (!nostr::features::IsNostrEnabled()) {
    return;
  }

  // Register the Nostr resource handler
  chrome::RegisterNostrResources();
}

}  // namespace nostr