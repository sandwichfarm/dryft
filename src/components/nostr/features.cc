// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nostr/features.h"

namespace nostr {
namespace features {

// Main feature flag for Nostr functionality
BASE_FEATURE(kNostr, "Nostr", base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag for local relay
BASE_FEATURE(kNostrLocalRelay, 
             "NostrLocalRelay", 
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag for Blossom server
BASE_FEATURE(kBlossomServer, 
             "BlossomServer", 
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag for Nsite support
BASE_FEATURE(kNsite, "Nsite", base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag for NIP-44 encryption (newer standard)
BASE_FEATURE(kNostrNip44Encryption, 
             "NostrNip44Encryption", 
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag for hardware wallet support
BASE_FEATURE(kNostrHardwareWallet, 
             "NostrHardwareWallet", 
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace nostr