// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_CONFIG_DRYFT_DRYFT_BUILDFLAGS_H_
#define BUILD_CONFIG_DRYFT_DRYFT_BUILDFLAGS_H_

#include "build/buildflag.h"

// Core Nostr functionality
#define BUILDFLAG_INTERNAL_ENABLE_NOSTR() (1)

// Local relay feature
#define BUILDFLAG_INTERNAL_ENABLE_LOCAL_RELAY() (1)

// Blossom server feature
#define BUILDFLAG_INTERNAL_ENABLE_BLOSSOM_SERVER() (1)

// Nsite static website support
#define BUILDFLAG_INTERNAL_ENABLE_NSITE() (1)

// Bundle Nostr JavaScript libraries
#define BUILDFLAG_INTERNAL_BUNDLE_NOSTR_LIBS() (1)

// Hardware wallet support
#define BUILDFLAG_INTERNAL_ENABLE_NOSTR_HARDWARE_WALLET() (0)

// Developer mode
#define BUILDFLAG_INTERNAL_ENABLE_NOSTR_DEV_MODE() (0)

// Port configurations
#define NOSTR_LOCAL_RELAY_DEFAULT_PORT 8081
#define BLOSSOM_SERVER_DEFAULT_PORT 8080

// Storage limits
#define NOSTR_RELAY_MAX_STORAGE_GB 10
#define BLOSSOM_MAX_STORAGE_GB 50

#endif  // BUILD_CONFIG_DRYFT_DRYFT_BUILDFLAGS_H_