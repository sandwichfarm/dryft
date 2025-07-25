> NOTICE: Alpha! Build system incomplete and project currently doesn't build. There _will_ be failing tests and docs _will_ be inaccurate. 

# dryft browser

A fork of [Thorium browser](https://github.com/Alex313031/thorium) with native Nostr protocol support built directly into the browser core.

## 📚 Documentation

- **[User Guide](docs/user-guide/README.md)** - Getting started, account management, and features
- **[Developer Docs](docs/developer/README.md)** - API reference and integration guide  
- **[Configuration](docs/configuration/README.md)** - Settings for users and enterprises
- **[Troubleshooting](docs/troubleshooting/README.md)** - Common issues and solutions

## ✨ Key Features

- **Native NIP-07**: Built-in `window.nostr` API, no extension needed
- **Local Relay**: Personal relay with SQLite backend on `ws://localhost:8081`
- **Blossom Storage**: Content-addressed file storage on `http://localhost:8080`
- **Nsite Support**: Browse static websites hosted on Nostr (kind 34128)
- **Multi-Account**: Manage multiple Nostr identities with secure key storage
- **Pre-loaded Libraries**: NDK, nostr-tools, and more available instantly

## 🚀 Quick Start

1. **[Download](https://github.com/sandwichfarm/dryft/releases)** the latest release for your platform
2. **Install** and launch dryft browser
3. **Create or import** your Nostr account
4. **Browse** any website - `window.nostr` is automatically available

Test it works:
```javascript
// Open Developer Tools on any https:// website
console.log('Nostr available:', !!window.nostr);
await window.nostr.getPublicKey(); // Your public key
```

## 🔧 Protocol Support

### NIPs (Nostr Implementation Possibilities)
- **NIP-01**: Basic protocol (via local relay)
- **NIP-04**: Legacy encrypted direct messages  
- **NIP-07**: `window.nostr` capability (native implementation)
- **NIP-44**: Modern encryption standard
- **NIP-65**: Relay list management
- **Kind 34128**: Nsite static website hosting

### BUDs (Blossom Protocol)
- **BUD-00**: Core Blossom protocol
- **BUD-01**: Server endpoints with kind 24242 auth events
- **BUD-03**: User server list integration

## 🏗️ Architecture

dryft integrates Nostr support directly into the browser core with these key components:

- **Browser Process**: Manages services, key storage, and permissions
- **Local Relay**: SQLite-backed WebSocket relay (`ws://localhost:8081`)
- **Blossom Server**: File storage service (`http://localhost:8080`) 
- **Secure Keys**: Platform-native storage (Keychain/Credential Manager)
- **Enhanced APIs**: Extended `window.nostr` and `window.blossom` objects

For complete technical details, see the [Browser Integration Guide](memory/reference/Browser_Integration_Details.md).

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
  
  // dryft extensions
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

## 🔧 Building from Source

See the [Building Guide](docs/BUILDING.md) for complete instructions, or follow these steps:

### Prerequisites
- Follow [Chromium build requirements](https://chromium.googlesource.com/chromium/src/+/main/docs/get_the_code.md) for your platform
- Python 3.8+, Git, and platform build tools

### Quick Build
```bash
# 1. Clone and setup
git clone https://github.com/sandwichfarm/dryft.git
cd dryft
./setup.sh

# 2. Configure with Nostr features
cd src
gn gen out/Release --args="
  is_official_build=true 
  enable_nostr=true 
  enable_local_relay=true 
  enable_blossom_server=true
"

# 3. Build
autoninja -C out/Release chrome
```

## 🤝 Contributing

- **[Development Guide](CLAUDE.md)** - Development workflow and standards
- **[GitHub Issues](https://github.com/sandwichfarm/dryft/issues)** - Bug reports and feature requests
- **[Pull Requests](https://github.com/sandwichfarm/dryft/pulls)** - Code contributions welcome

## 📄 License

BSD-style license, same as Chromium/Thorium. See [LICENSE.md](LICENSE.md) for details.
