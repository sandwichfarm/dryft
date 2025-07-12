// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NOSTR_BROWSER_UTILS_H_
#define CHROME_BROWSER_NOSTR_NOSTR_BROWSER_UTILS_H_

namespace nostr {

// Initializes Nostr-related browser resources.
// This should be called during browser startup.
void InitializeNostrResources();

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NOSTR_BROWSER_UTILS_H_