// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOSTR_FEATURES_H_
#define COMPONENTS_NOSTR_FEATURES_H_

#include "base/feature_list.h"
#include "build/config/tungsten/tungsten_buildflags.h"

namespace nostr {
namespace features {

// Main feature flag for Nostr functionality
BASE_DECLARE_FEATURE(kNostr);

// Feature flag for local relay
BASE_DECLARE_FEATURE(kNostrLocalRelay);

// Feature flag for Blossom server
BASE_DECLARE_FEATURE(kBlossomServer);

// Feature flag for Nsite support
BASE_DECLARE_FEATURE(kNsite);

// Feature flag for NIP-44 encryption (newer standard)
BASE_DECLARE_FEATURE(kNostrNip44Encryption);

// Feature flag for hardware wallet support
BASE_DECLARE_FEATURE(kNostrHardwareWallet);

// Helper functions to check if features are enabled
inline bool IsNostrEnabled() {
#if BUILDFLAG(ENABLE_NOSTR)
  return base::FeatureList::IsEnabled(kNostr);
#else
  return false;
#endif
}

inline bool IsLocalRelayEnabled() {
#if BUILDFLAG(ENABLE_LOCAL_RELAY)
  return IsNostrEnabled() && base::FeatureList::IsEnabled(kNostrLocalRelay);
#else
  return false;
#endif
}

inline bool IsBlossomServerEnabled() {
#if BUILDFLAG(ENABLE_BLOSSOM_SERVER)
  return IsNostrEnabled() && base::FeatureList::IsEnabled(kBlossomServer);
#else
  return false;
#endif
}

inline bool IsNsiteEnabled() {
#if BUILDFLAG(ENABLE_NSITE)
  return IsNostrEnabled() && base::FeatureList::IsEnabled(kNsite);
#else
  return false;
#endif
}

}  // namespace features
}  // namespace nostr

#endif  // COMPONENTS_NOSTR_FEATURES_H_