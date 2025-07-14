# Tungsten Browser

A fork of [Thorium browser](https://github.com/Alex313031/thorium) with native Nostr protocol support built directly into the browser core.

## NIP Compatibility Overview

- **NIP-01**: Basic protocol (via local relay)
- **NIP-04**: Encrypted direct messages  
- **NIP-07**: `window.nostr` capability
- **NIP-44**: Modern encryption standard
- **NIP-XX (34128)**: Nsite static website hosting

## BUD Compatibility Overview

- **BUD-00**: Core Blossom protocol
- **BUD-01**: Server endpoints with kind 24242 auth events
- **BUD-03**: User server list integration

## Nsite Overview

Tungsten supports hosting static websites directly from Nostr events (kind 34128). Access sites via:
- `nostr://npub.../` URLs
- Subdomain routing: `npub123...456.domain.com`

## Authentication

- Multi-account support with profile switching
- Granular permissions per origin, method, and event kind
- Secure key storage using platform-specific APIs
- Built-in key generation and import/export

## Local Relay

Built-in WebSocket relay (port 8081):
- SQLite backend with WAL mode
- Full NIP-01 protocol implementation  
- Complex query support with indexing
- Automatic sync with external relays

## Local Blossom

Content-addressed storage server (port 8080):
- Direct file system storage
- SHA256 content addressing
- LRU cache management
- Integration with remote Blossom servers

## Technical Overview

### Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Tungsten Browser                      │
├─────────────────────────────────────────────────────────┤
│                    Browser Process                       │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │   Renderer  │  │  window.nostr │  │window.blossom │  │
│  │   Process   │  │     API       │  │     API       │  │
│  └──────┬──────┘  └──────┬───────┘  └───────┬───────┘  │
│         │                 │                   │          │
│  ┌──────┴─────────────────┴───────────────────┴──────┐  │
│  │              IPC Message Router                    │  │
│  └──────┬─────────────────┬───────────────────┬──────┘  │
│         │                 │                   │          │
├─────────┼─────────────────┼───────────────────┼─────────┤
│  ┌──────┴──────┐  ┌──────┴───────┐  ┌───────┴───────┐  │
│  │ Key Service │  │ Local Relay  │  │Local Blossom  │  │
│  │  (Isolated) │  │   Service    │  │    Service    │  │
│  └──────┬──────┘  └──────┬───────┘  └───────┬───────┘  │
│         │                 │                   │          │
├─────────┼─────────────────┼───────────────────┼─────────┤
│  ┌──────┴──────┐  ┌──────┴───────┐  ┌───────┴───────┐  │
│  │  Platform   │  │    SQLite    │  │  File System  │  │
│  │  Key Store  │  │   Database   │  │    Storage    │  │
│  └─────────────┘  └──────────────┘  └───────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### Browser API Extensions

```javascript
// Enhanced window.nostr
window.nostr = {
  // Standard NIP-07 methods
  getPublicKey(): Promise<string>,
  signEvent(event): Promise<SignedEvent>,
  getRelays(): Promise<RelayList>,
  nip04: { encrypt(), decrypt() },
  nip44: { encrypt(), decrypt() },
  
  // Tungsten extensions
  relay: {
    url: "ws://localhost:8081",
    connected: boolean,
    query(filter): Promise<Event[]>
  },
  libs: {
    ndk: "chrome://resources/js/nostr/ndk.js",
    'nostr-tools': "chrome://resources/js/nostr/nostr-tools.js",
    'applesauce-core': "chrome://resources/js/nostr/applesauce-core.js",
    'alby-sdk': "chrome://resources/js/nostr/alby-sdk.js",
    // ... more libraries
    versions: {
      ndk: "2.0.0",
      'nostr-tools': "1.17.0",
      // ... version info
    }
  }
}

// Blossom API
window.blossom = {
  upload(file): Promise<Result>,
  get(hash): Promise<Blob>,
  mirror(hash, servers): Promise<void>
}
```

### Storage Architecture

| Component | Technology | Purpose |
|-----------|------------|---------|
| Local Relay | SQLite + WAL | Complex queries, event storage |
| Blossom | File System | Content-addressed blob storage |
| Browser State | LMDB | Fast key-value for UI state |
| Keys | Platform Store | Secure credential storage |
| Preferences | JSON | User settings |

### Performance Targets

- Startup overhead: <50ms
- Memory overhead: <50MB base
- NIP-07 operations: <20ms  
- Local relay queries: <10ms
- CPU usage idle: <0.1%

## Building

See [Thorium's build instructions](https://github.com/Alex313031/thorium/blob/main/docs/BUILDING.md) with these additional flags:

```bash
gn gen out/Release --args="
  is_official_build=true 
  enable_nostr=true 
  enable_local_relay=true 
  enable_blossom_server=true
"
autoninja -C out/Release chrome
```

## License

Same as Thorium/Chromium - see LICENSE file.