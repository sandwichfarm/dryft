# dryft browser User Guide

Welcome to dryft browser - the first browser with native Nostr protocol support built directly into the core.

## Quick Navigation

- [Getting Started](getting-started.md) - Installation and first setup
- [Account Management](account-management.md) - Creating and managing Nostr accounts
- [Nostr Features](nostr-features.md) - Using built-in Nostr capabilities
- [Browsing Nsites](browsing-nsites.md) - Accessing Nostr-hosted websites
- [Blossom File Sharing](blossom-features.md) - Content-addressed file storage
- [Privacy and Security](privacy-security.md) - Protecting your keys and data
- [Settings Reference](settings-reference.md) - Complete settings guide

## What Makes dryft Special?

dryft is built on Thorium browser (a Chromium fork) with native Nostr protocol integration:

### ✨ Key Features

- **No Extension Required**: Built-in `window.nostr` API on all websites
- **Local Relay**: Your own relay running locally for offline access
- **Blossom Storage**: Content-addressed file storage and sharing
- **Nsite Support**: Browse static websites hosted on Nostr
- **Multi-Account**: Manage multiple Nostr identities seamlessly
- **Secure Keys**: Platform-native secure key storage
- **Pre-loaded Libraries**: Popular Nostr libraries available instantly

### 🔒 Security First

- Keys stored in your OS's secure storage (Keychain, Credential Manager, etc.)
- Granular permissions per website and operation type
- Sandboxed Nsite content execution
- No external dependencies for core functionality

### 🌐 Protocol Support

- **NIP-01**: Basic protocol flow
- **NIP-04**: Legacy encrypted direct messages
- **NIP-07**: Native `window.nostr` capability
- **NIP-44**: Modern encryption standard
- **NIP-65**: Relay list management
- **Kind 34128**: Nsite static website hosting
- **BUD-00/01/03**: Blossom content storage protocol

## Getting Help

- [Troubleshooting Guide](../troubleshooting/README.md)
- [FAQ](../troubleshooting/faq.md)
- [Community Support](../troubleshooting/support.md)
- [Report Issues](https://github.com/sandwichfarm/dryft/issues)

## For Developers

If you're building applications that integrate with dryft, see the [Developer Documentation](../developer/README.md).