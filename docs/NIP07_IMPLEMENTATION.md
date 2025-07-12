# NIP-07 Implementation in Tungsten

This document describes the implementation of NIP-07 (window.nostr capability) in Tungsten browser.

## Overview

NIP-07 defines a browser extension interface that provides signing capabilities to web pages. Tungsten implements this natively in the browser, eliminating the need for a separate extension.

## Current Implementation Status

### Phase 1: Basic Injection (COMPLETE)
- ✅ window.nostr object injection
- ✅ Origin checking (no injection on privileged URLs)
- ✅ Basic method stubs that return promises
- ✅ Browser tests for injection
- ✅ Unit tests for V8 bindings

### Phase 2: IPC Communication (TODO)
- [ ] Connect renderer bindings to browser process
- [ ] Implement message routing for all NIP-07 methods
- [ ] Add permission checks before operations

### Phase 3: Full Implementation (TODO)
- [ ] Implement actual signing with secure key storage
- [ ] Permission UI for user consent
- [ ] Relay management
- [ ] NIP-04/NIP-44 encryption

## Architecture

### Renderer Process
- `NostrBindings` - V8 bindings for window.nostr
- `NostrInjection` - Script context injection logic

### Browser Process
- `NostrService` - Core signing and key management (TODO)
- `NostrPermissionManager` - Permission handling (TODO)

### IPC Messages
Messages defined in `chrome/common/nostr_messages.h`:
- `NostrHostMsg_GetPublicKey` - Request public key
- `NostrHostMsg_SignEvent` - Sign an event
- `NostrHostMsg_GetRelays` - Get relay configuration
- `NostrHostMsg_Nip04Encrypt/Decrypt` - NIP-04 encryption

## API Surface

```javascript
window.nostr = {
  // Get the public key of the current user
  getPublicKey: async () => string,
  
  // Sign a Nostr event
  signEvent: async (event: UnsignedEvent) => SignedEvent,
  
  // Get relay configuration
  getRelays: async () => {[url: string]: {read: boolean, write: boolean}},
  
  // NIP-04 encryption (deprecated but still supported)
  nip04: {
    encrypt: async (pubkey: string, plaintext: string) => string,
    decrypt: async (pubkey: string, ciphertext: string) => string
  }
}
```

## Security Considerations

1. **Origin Restrictions**
   - No injection on chrome://, chrome-extension://, devtools:// URLs
   - Each origin requires separate permission

2. **Permission Model**
   - First use triggers permission prompt
   - Permissions can be managed in browser settings
   - Granular permissions per method

3. **Key Storage**
   - Keys never exposed to renderer process
   - Platform-specific secure storage (Keychain, Credential Manager, etc.)

4. **Rate Limiting**
   - Prevents abuse by malicious websites
   - Configurable limits per origin

## Testing

### Unit Tests
```bash
# Run bindings tests
autoninja -C out/Default content_unittests --gtest_filter="NostrBindingsTest.*"
```

### Browser Tests
```bash
# Run injection tests
autoninja -C out/Default content_browsertests --gtest_filter="NostrInjectionBrowserTest.*"
```

### Manual Testing
1. Navigate to any https:// website
2. Open DevTools console
3. Type `window.nostr` - should show the object
4. Try `await window.nostr.getPublicKey()` - should reject with "not implemented"

## Future Enhancements

1. **Hardware Wallet Support**
   - Integration with hardware signing devices
   - Additional security for high-value keys

2. **Multiple Accounts**
   - Account switcher in browser UI
   - Per-origin account selection

3. **Advanced Encryption**
   - NIP-44 support (modern encryption standard)
   - Group encryption support

4. **Developer Tools**
   - Nostr event inspector in DevTools
   - Network activity monitoring

## Debugging

Enable verbose logging:
```bash
--vmodule=nostr*=2
```

Check injection:
```javascript
console.log('Has Nostr:', typeof window.nostr !== 'undefined');
console.log('Methods:', Object.keys(window.nostr));
```