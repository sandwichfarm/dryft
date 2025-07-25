# Tungsten Build System Integration

This document describes how Tungsten components are integrated into the Chromium build system.

## Build Flags

The following build flags control dryft features:

```gn
# Main toggle for all Nostr functionality
enable_nostr = true

# Enable the local Nostr relay (SQLite-based)
enable_local_relay = true

# Enable the local Blossom server (file storage)
enable_blossom_server = true

# Bundle Nostr JavaScript libraries
bundle_nostr_libs = true

# Enable Nsite static website support
enable_nsite = true

# Enable hardware wallet support (disabled by default)
enable_nostr_hardware_wallet = false
```

## Directory Structure

```
dryft/
├── build/
│   └── config/
│       └── dryft/
│           ├── BUILD.gn              # Build flag header generation
│           ├── tungsten.gni          # Build flag declarations
│           └── tungsten_buildflags.h # C++ build flag macros
├── src/
│   ├── chrome/
│   │   ├── browser/
│   │   │   ├── nostr/               # Browser-side Nostr implementation
│   │   │   ├── blossom/             # Blossom server implementation
│   │   │   └── nsite/               # Nsite protocol handler
│   │   ├── common/
│   │   │   ├── nostr_messages.h     # IPC message definitions
│   │   │   └── BUILD.gn             # Common build configuration
│   │   └── renderer/
│   │       └── nostr/               # Renderer-side injection
│   ├── components/
│   │   └── nostr/                   # Core Nostr components
│   │       ├── BUILD.gn             # Component build configuration
│   │       ├── features.h/cc        # Feature flags
│   │       └── renderer/            # Renderer components
│   └── third_party/
│       └── nostr/                   # Bundled JavaScript libraries
│           ├── BUILD.gn             # Library bundling
│           └── README.chromium      # Library documentation
```

## Build Targets

### Main Targets

1. **`//components/nostr`** - Core Nostr implementation
2. **`//components/nostr:nostr_renderer`** - Renderer-side components
3. **`//components/nostr:local_relay`** - Local relay service
4. **`//components/nostr:blossom`** - Blossom server
5. **`//chrome/browser/nostr`** - Browser integration
6. **`//chrome/common:nostr_messages`** - IPC messages

### Test Targets

1. **`//components/nostr:nostr_unit_tests`** - Unit tests
2. **`//chrome/browser/nostr:nostr_browser_tests`** - Browser tests
3. **`//chrome/common:nostr_messages_unittests`** - IPC tests

## Building Tungsten

### Development Build

```bash
# Set up build directory with Nostr enabled
gn gen out/Default --args='
  is_debug = true
  enable_nostr = true
  enable_local_relay = true
  enable_blossom_server = true
  bundle_nostr_libs = true
'

# Build
autoninja -C out/Default chrome
```

### Release Build

```bash
gn gen out/Release --args='
  is_official_build = true
  enable_nostr = true
  enable_local_relay = true
  enable_blossom_server = true
  bundle_nostr_libs = true
  # Preserve Thorium optimizations
  use_thin_lto = true
  thin_lto_enable_optimizations = true
'

autoninja -C out/Release chrome
```

### Component Build

```bash
gn gen out/Component --args='
  is_component_build = true
  enable_nostr = true
'
```

## Resource Bundling

Nostr resources are bundled into `.pak` files:

1. **JavaScript Libraries** - Bundled via `//third_party/nostr`
2. **HTML/CSS/JS Resources** - Bundled via `nostr_resources.grd`
3. **Icons and Images** - Included in resource pack

## Feature Detection

Use the following patterns to conditionally compile Nostr code:

### C++ Code

```cpp
#include "components/nostr/features.h"

if (nostr::features::IsNostrEnabled()) {
  // Nostr-specific code
}
```

### Build Files

```gn
if (enable_nostr) {
  sources += [ "nostr_specific.cc" ]
  deps += [ "//components/nostr" ]
}
```

## Integration Points

1. **Browser Process**
   - Modified `//chrome/browser/BUILD.gn`
   - Added Nostr service factories
   - Integrated permission system

2. **Renderer Process**
   - Modified `//content/renderer/BUILD.gn`
   - Added window.nostr injection
   - Implemented IPC message handling

3. **Common Code**
   - Created `//chrome/common/BUILD.gn`
   - Added IPC message definitions
   - Shared types and constants

## Troubleshooting

### Build Errors

1. **Missing dependencies**: Ensure all imports are added to BUILD.gn files
2. **Undefined symbols**: Check that feature flags are properly defined
3. **Resource not found**: Verify .grd files are properly configured

### Runtime Issues

1. **Feature not working**: Check feature flags in chrome://flags
2. **IPC errors**: Verify message routing is properly set up
3. **Resource loading**: Check chrome://resources/js/nostr/

## Future Improvements

1. **Modularization**: Consider making Nostr a Chrome Extension API
2. **Dynamic Loading**: Load libraries on-demand to reduce binary size
3. **Build Time**: Optimize build configuration for faster compilation
4. **Testing**: Expand test coverage for all components