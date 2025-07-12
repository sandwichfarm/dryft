# Third Party Nostr Libraries

This directory contains the third-party JavaScript libraries bundled with Tungsten for Nostr functionality.

## Libraries

1. **NDK (Nostr Development Kit)** - v2.0.0
   - Comprehensive Nostr client library
   - https://github.com/nostr-dev-kit/ndk

2. **nostr-tools** - v1.17.0
   - Core utilities for Nostr protocol
   - https://github.com/nbd-wtf/nostr-tools

3. **Applesauce** - v0.5.0
   - Nostr event rendering library
   - https://github.com/coracle-social/applesauce

4. **Nostrify** - v1.2.0
   - Nostr framework for applications
   - https://github.com/soapbox-pub/nostrify

5. **Alby SDK** - v3.0.0
   - SDK for Alby wallet integration
   - https://github.com/getAlby/sdk

## Adding New Libraries

To add a new library:

1. Download the minified version to this directory
2. Update `bundle_libraries.py` with the new mapping
3. Add the resource entry to `nostr_resources.grd`
4. Update the resource IDs in `chrome/grit/nostr_resources.h`
5. Add the path mapping in `nostr_resource_handler.cc`
6. Update `Browser_API_Extensions.md` with the new library path

## License Compliance

Each library must be compatible with Tungsten's BSD-style license. Check the individual library licenses before inclusion.