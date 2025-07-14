# Third Party Nostr Libraries

This directory contains bundled JavaScript libraries for Nostr functionality.

## Libraries

1. **NDK (Nostr Development Kit)** - v2.0.0
   - Comprehensive Nostr client library
   - https://github.com/nostr-dev-kit/ndk

2. **nostr-tools** - v1.17.0
   - Core utilities for Nostr protocol
   - https://github.com/nbd-wtf/nostr-tools

3. **applesauce** - Multiple packages
   - **applesauce-core** - v0.3.4
   - **applesauce-content** - v0.3.4
   - **applesauce-lists** - v0.3.4
   - https://github.com/hzrd149/applesauce

4. **alby-sdk** - v3.0.0
   - SDK for Alby wallet integration
   - https://github.com/getAlby/sdk

## Usage

These libraries are accessible via `window.nostr.libs` which returns importable paths:

```javascript
// Import NDK
const NDK = await import(window.nostr.libs.ndk);

// Import nostr-tools
const { nip19, generatePrivateKey } = await import(window.nostr.libs['nostr-tools']);

// Import applesauce packages
const { parseContent } = await import(window.nostr.libs['applesauce-content']);
```

## Adding New Libraries

1. Add the minified library file to this directory
2. Update `bundle_libraries.py` with the new mapping
3. Add resource entry to `nostr_resources.grd`
4. Update `nostr_resource_handler.cc` with path mapping
5. Add the library path to `window.nostr.libs` object

## Build Process

The libraries are bundled during the build process via `bundle_libraries.py`.
They are served at `chrome://resources/js/nostr/*.js`.