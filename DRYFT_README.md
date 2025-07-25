# dryft browser

Tungsten is a fork of Thorium browser that adds native Nostr protocol support.

## Features

- **Native NIP-07 Support**: Built-in `window.nostr` provider, no extension needed
- **Local Relay**: SQLite-based local relay for offline access and caching
- **Blossom Server**: Content-addressed storage for media and files
- **nostr:// Protocol**: Handle nostr:// URLs natively
- **Nsite Support**: Browse static websites hosted on Nostr (kind 34128 events)
- **Pre-loaded Libraries**: Popular Nostr libraries available at `window.nostr.libs`

## Building Tungsten

### Prerequisites

Follow the standard Chromium build prerequisites for your platform.

### Build Instructions

1. Clone the repository:
```bash
git clone https://github.com/yourusername/tungsten.git
cd tungsten
```

2. Set up the build environment:
```bash
./setup.sh
```

3. Configure the build with Nostr features enabled:
```bash
cd src
gn gen out/Release --args='
  is_official_build=true
  enable_nostr=true
  enable_local_relay=true
  enable_blossom_server=true
'
```

4. Build Tungsten:
```bash
autoninja -C out/Release chrome
```

## Development

See [CLAUDE.md](CLAUDE.md) for development guidelines and workflow.

## Architecture

- **Browser Process**: Manages Nostr services, key storage, and permissions
- **Renderer Process**: Injects `window.nostr` and handles web API calls
- **Local Relay**: Runs as a service on `ws://localhost:8081`
- **Blossom Server**: HTTP server on `http://localhost:8080`

## Security

- Keys are stored using platform-specific secure storage:
  - Windows: Credential Manager
  - macOS: Keychain Services
  - Linux: Secret Service API
- All Nsite content runs in a sandboxed environment
- Strict CSP prevents external resource loading

## License

Tungsten inherits Thorium's BSD-style license. See LICENSE file for details.